#!/usr/bin/env python3
r"""Delta-minimize an ic3 safety counterexample witness (.yw).

Given a SystemVerilog case file and a Yosys witness written by
`ic3 -cex <prefix>` (see run_ic3.py), greedily 1-minimize the stimulus:
clear individual data-input bits (set them to 0) and truncate the suffix
for as long as the SAME assertion still fires at the SAME step in a
`sim -r` replay.  The result is a 1-minimal witness: every remaining 1-bit
is load-bearing for the counterexample.

Replay uses EXACTLY the battery ic3 prep (the ic3 cex export comment
requires it: the design is re-derived from the prepped netlist, so the
replay must see the same setundef -init -zero the ic3 run used):

    read_verilog -sv -formal; hierarchy -check [-top]; sva2check; proc;
    async2sync; flatten; opt_clean; memory; opt_clean; setundef -init -zero

with the PREP-EXTRA-AFTER-ASYNC2SYNC / PREP-EXTRA-AFTER-MEMORY pragmas
honored the same way run_ic3.py/run_bmc.py honor them (run_bmc.py's
prep_extras: the IC3-BMC pragma only affects the ic3 invocation, not the
replay prep).  The prepped RTLIL is written once to a temp dir; every
trial replays via `yosys -p "read_rtlil prep.il; sim -r trial.yw
-summary sum.json"`, which is the fast path (no re-parse/re-prep).

Algorithm:
  1. Replay the original witness; from the summary pick the target
     (assertion path, step k): the rarest-firing assertion path at its
     last firing step.  The witness' own assertion fires only because of
     this trace, while a collateral constant-false property (tconst1's
     p1) fires at every step -- with an irrelevant suffix it even fires
     after the real target, so highest-step selection would chase it.
     Error out if no assertion fired.
  2. Greedy bit-clear over the data-input words of steps 0..k (steps
     after k cannot influence firing at k; suffix truncation drops them).
     yw bit packing mirrors kernel/yw.cc get_bits / the ic3 exporter:
     the i-th bit of the concatenated data-input vector sits at string
     index width-1-i (LSB at the highest string index).  Up to 3 sweeps
     until a fixpoint; trials within a sweep run in --jobs processes.
     A chunk with several accepted clears is re-confirmed sequentially so
     every accepted clear is verified against the witness it lands in.
  3. Truncate the trace to exactly k+1 steps and verify the replay still
     fires at k (keep the original length if it does not).
  4. Prefix steps are never removed: indices would shift and step k would
     no longer mean the same cycle.

Exit code 0 iff the minimized witness replays to the target assertion at
step k.  --clock is accepted for CLI symmetry with the other battery
runners but unused: the yw header carries the clock and `sim -r` rejects
-clock outright.
"""

import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
YOSYS = os.environ.get("YOSYS", os.path.join(ROOT, "third_party/yosys/build/yosys"))

PREP = """read_verilog -sv -formal {inc} {files}
hierarchy -check{top}
sva2check
proc
async2sync
{extra_a2s}flatten
opt_clean
memory
opt_clean
{extra_mem}setundef -init -zero
write_rtlil {il}
"""

# same prep flattened to a one-shot replay recipe (printed at the end)
REPLAY_RECIPE = """read_verilog -sv -formal {inc} {files}
hierarchy -check{top}
sva2check
proc
async2sync
{extra_a2s}flatten
opt_clean
memory
opt_clean
{extra_mem}setundef -init -zero
sim -r {out} -summary s.json
"""

SIM_CLI = "read_rtlil {il}; sim -r {yw} -summary {sum}"

PREP_TIMEOUT = 600
TRIAL_TIMEOUT = 120
SWEEPS = 3


def prep_extras(path):
    """Per-file opt-in prep passes from pragma comments (multiclock etc.)."""
    a2s = mem = ""
    for line in open(path):
        m = re.match(r'\s*//\s*PREP-EXTRA-AFTER-ASYNC2SYNC:\s*(.+)', line)
        if m:
            a2s = m.group(1).strip() + "\n"
        m = re.match(r'\s*//\s*PREP-EXTRA-AFTER-MEMORY:\s*(.+)', line)
        if m:
            mem = m.group(1).strip() + "\n"
    return a2s, mem


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def first_error(blob):
    for line in blob.splitlines():
        if "ERROR:" in line:
            return line.split("ERROR:", 1)[1].strip()
    return None


def prep_design(yosys, case, top, inc_dirs, tmpdir):
    """Run the battery ic3 prep once; return the prepped RTLIL path."""
    il = os.path.join(tmpdir, "prep.il")
    inc = " ".join("-I" + d for d in inc_dirs)
    extra_a2s, extra_mem = prep_extras(case)
    ys = os.path.join(tmpdir, "prep.ys")
    with open(ys, "w") as fh:
        fh.write(PREP.format(inc=inc, files=case, top=" -top " + top if top else "",
                             extra_a2s=extra_a2s, extra_mem=extra_mem, il=il))
    proc = run(["zsh", "-f", "-c", 'ulimit -s 65520; exec "$1" "$2"', "y", yosys, ys],
               timeout=PREP_TIMEOUT)
    blob = proc.stdout + "\n" + proc.stderr
    if proc.returncode != 0 or not os.path.exists(il):
        print("%s: prep-failed: %s" % (os.path.basename(case),
                                       (first_error(blob) or "no RTLIL written")[:120]))
        return None
    return il


def replay(yosys, il, yw_path, sum_path, timeout=TRIAL_TIMEOUT):
    """Replay a witness against the prepped design; return the summary."""
    proc = run([yosys, "-p", SIM_CLI.format(il=il, yw=yw_path, sum=sum_path)],
               timeout=timeout)
    if not os.path.exists(sum_path):
        blob = proc.stdout + "\n" + proc.stderr
        print("replay-failed: %s" % (first_error(blob) or "no summary written")[:120])
        return None
    with open(sum_path) as fh:
        return json.load(fh)


def load_yw(path):
    with open(path) as fh:
        data = json.load(fh)
    if not isinstance(data, dict) or data.get("format") != "Yosys Witness Trace":
        print("%s: not a Yosys witness trace" % path)
        return None
    steps = data.get("steps", [])
    widths = {len(s.get("bits", "")) for s in steps}
    if len(steps) == 0 or len(widths) != 1:
        print("%s: witness has no steps or mixed step widths" % path)
        return None
    return data


def write_yw(path, data):
    with open(path, "w") as fh:
        fh.write(json.dumps(data, indent=1) + "\n")


def pick_target(summary):
    """Target (path, step) = the rarest-firing assertion path, at its last
    firing step.  The witness' own assertion fires only because of this
    trace; a collateral constant-false property (tconst1's p1) fires at
    every step, so highest-step selection would chase it -- especially
    with an irrelevant suffix where it fires after the real target.
    Ties: latest firing, then first seen in the summary."""
    fired = summary.get("assertions", [])
    if not fired:
        return None
    counts, last, order = {}, {}, {}
    for idx, a in enumerate(fired):
        key = tuple(a["path"])
        counts[key] = counts.get(key, 0) + 1
        last[key] = max(last.get(key, -1), a["step"])
        order.setdefault(key, idx)
    best = min(counts, key=lambda p: (counts[p], -last[p], order[p]))
    return (best, last[best])


def fires(summary, target_path, k):
    if summary is None:
        return False
    for a in summary.get("assertions", []):
        if tuple(a["path"]) == target_path and a["step"] == k:
            return True
    return False


def fired_bit_count(steps, k):
    return sum(s["bits"].count("1") for s in steps[:k + 1])


def clear_bit(steps_bits, t, i):
    """Copy of per-step bit strings with input bit i of step t cleared
    (bit i of the concatenated data-input vector, LSB at the highest
    string index)."""
    out = list(steps_bits)
    s = out[t]
    out[t] = s[:len(s) - 1 - i] + "0" + s[len(s) - 1 - i + 1:]
    return out


def yw_text(data, steps_bits):
    trial = dict(data)
    trial["steps"] = [{"bits": b} for b in steps_bits]
    return json.dumps(trial, indent=1) + "\n"


def _trial(task):
    """Worker: replay one trial witness with `bit` cleared; (bit, summary)."""
    yosys, il, data, steps_bits, bit = task
    t, i = bit
    text = yw_text(data, clear_bit(steps_bits, t, i))
    with tempfile.TemporaryDirectory(prefix="mincex_") as td:
        yw = os.path.join(td, "trial.yw")
        with open(yw, "w") as fh:
            fh.write(text)
        return bit, replay(yosys, il, yw, os.path.join(td, "sum.json"))


def run_trials(yosys, il, data, steps_bits, bits, jobs):
    """Replay one chunk of bit-clear trials in parallel; {bit: summary}."""
    tasks = [(yosys, il, data, steps_bits, bit) for bit in bits]
    results = {}
    if jobs > 1:
        with concurrent.futures.ProcessPoolExecutor(max_workers=jobs) as ex:
            for bit, summary in ex.map(_trial, tasks):
                results[bit] = summary
    else:
        for task in tasks:
            bit, summary = _trial(task)
            results[bit] = summary
    return results


def check_bits(yosys, il, data, steps_bits, tmpdir, tag):
    """Parent-side oracle for one candidate witness."""
    yw = os.path.join(tmpdir, "chk_%s.yw" % tag)
    with open(yw, "w") as fh:
        fh.write(yw_text(data, steps_bits))
    summary = replay(yosys, il, yw, os.path.join(tmpdir, "chk_%s.json" % tag))
    os.unlink(yw)
    return summary


def minimize_bits(yosys, il, data, target_path, k, jobs, tmpdir):
    """Greedy 1-minimization over the data-input bits of steps 0..k.
    Returns (minimized step bits, sweeps run)."""
    steps_bits = [s["bits"] for s in data["steps"]]
    width = len(steps_bits[0])
    total_sweeps = 0
    for sweep in range(1, SWEEPS + 1):
        total_sweeps = sweep
        cleared = 0
        candidates = [(t, i) for t in range(k + 1) for i in range(width)
                      if steps_bits[t][width - 1 - i] == "1"]
        for start in range(0, len(candidates), jobs):
            chunk = candidates[start:start + jobs]
            results = run_trials(yosys, il, data, steps_bits, chunk, jobs)
            accepted = [b for b in chunk if fires(results[b], target_path, k)]
            # apply accepted clears in order; each was verified against the
            # witness at chunk start, so re-confirm any past the first
            # against the witness it actually lands in
            for j, (t, i) in enumerate(accepted):
                cand = clear_bit(steps_bits, t, i)
                if j > 0 and not fires(check_bits(yosys, il, data, cand,
                                                  tmpdir, "c%d" % j), target_path, k):
                    continue
                steps_bits = cand
                cleared += 1
        if cleared == 0:
            break
    return steps_bits, total_sweeps


def fixpoint_pass(yosys, il, data, steps_bits, target_path, k, jobs):
    """Final pass: no single kept 1-bit may be clearable."""
    width = len(steps_bits[0])
    candidates = [(t, i) for t in range(k + 1) for i in range(width)
                  if steps_bits[t][width - 1 - i] == "1"]
    for start in range(0, len(candidates), jobs):
        chunk = candidates[start:start + jobs]
        results = run_trials(yosys, il, data, steps_bits, chunk, jobs)
        for bit in chunk:
            if fires(results[bit], target_path, k):
                return bit
    return None


def pretty_path(path):
    return ".".join(p.lstrip("\\") for p in path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("case", help=".sv file the witness was generated from")
    ap.add_argument("witness", help=".yw counterexample witness (ic3 -cex)")
    ap.add_argument("--top", default=None)
    ap.add_argument("--clock", default=None,
                    help="accepted for symmetry; unused (the yw header carries "
                         "the clock and sim -r rejects -clock)")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("-o", "--output", default=None)
    ap.add_argument("--yosys", default=YOSYS)
    args = ap.parse_args()

    label = os.path.basename(args.witness)
    out = args.output or re.sub(r"\.yw$", "", args.witness) + ".min.yw"

    data = load_yw(args.witness)
    if data is None:
        sys.exit(1)

    with tempfile.TemporaryDirectory(prefix="mincex_") as tmpdir:
        il = prep_design(args.yosys, args.case, args.top, [], tmpdir)
        if il is None:
            sys.exit(1)

        base = replay(args.yosys, il, args.witness,
                      os.path.join(tmpdir, "base.json"))
        if base is None:
            sys.exit(1)
        target = pick_target(base)
        if target is None:
            print("%s: no assertion fired in baseline replay" % label)
            sys.exit(1)
        target_path, k = target
        if k >= len(data["steps"]):
            print("%s: fired step %d beyond witness (%d steps)" %
                  (label, k, len(data["steps"])))
            sys.exit(1)

        n_bits0 = fired_bit_count(data["steps"], k)
        n_steps0 = len(data["steps"])
        print("%s: baseline: %s @ step %d: fired-bits=%d steps=%d" %
              (label, pretty_path(target_path), k, n_bits0, n_steps0))

        steps_bits, sweeps = minimize_bits(args.yosys, il, data, target_path,
                                           k, max(1, args.jobs), tmpdir)

        # suffix truncation: keep exactly k+1 steps, then verify
        n_steps = len(steps_bits)
        if n_steps > k + 1:
            cand = steps_bits[:k + 1]
            if fires(check_bits(args.yosys, il, data, cand, tmpdir, "trunc"),
                     target_path, k):
                steps_bits = cand
            else:
                print("%s: suffix truncation broke the replay; keeping %d steps"
                      % (label, n_steps))

        stubborn = fixpoint_pass(args.yosys, il, data, steps_bits,
                                 target_path, k, max(1, args.jobs))
        if stubborn is not None:
            print("%s: WARNING: fixpoint pass cleared bit (step %d bit %d) -- "
                  "oracle drift?" % (label, stubborn[0], stubborn[1]))

        minimized = dict(data)
        minimized["steps"] = [{"bits": b} for b in steps_bits]
        write_yw(out, minimized)

        final = replay(args.yosys, il, out, os.path.join(tmpdir, "final.json"))
        if not fires(final, target_path, k):
            print("%s: ERROR: written witness does not replay to the target" % label)
            sys.exit(1)

        extra_a2s, extra_mem = prep_extras(args.case)
        recipe = "; ".join(" ".join(l.split()) for l in REPLAY_RECIPE.format(
            inc="", files=args.case, top=" -top " + args.top if args.top else "",
            extra_a2s=extra_a2s, extra_mem=extra_mem, out=out).splitlines() if l.strip())
        print("%s: minimized: %s @ step %d: fired-bits=%d steps=%d (sweeps=%d)%s" %
              (label, pretty_path(target_path), k, fired_bit_count(minimized["steps"], k),
               len(steps_bits), sweeps,
               "" if stubborn is None else " FIXPOINT-FAIL"))
        print("%s: fixpoint: no single kept 1-bit is clearable%s" %
              (label, "" if stubborn is None else " (see WARNING above)"))
        print("%s: wrote %s" % (label, out))
        print("replay with: yosys -p \"%s\"" % recipe)

    sys.exit(0)


if __name__ == "__main__":
    main()
