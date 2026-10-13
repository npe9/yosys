#!/usr/bin/env python3
r"""SVA lowering-fuzz differential oracle driver.

Per generated case (see gen.py), cross-checks the two evaluation paths of
an SVA property lowered by sva2check:

  sim leg     read_verilog -sv -formal; sva2check; hierarchy -check -top top;
              proc; flatten; sim -r stim.yw -zinit -summary sum.json
  formal leg  read_verilog -sv -formal; hierarchy -check -top top; sva2check;
              proc; async2sync; flatten; opt_clean; memory; opt_clean;
              setundef -init -zero; write_smt2 -wires out.smt2
              then yosys-smtbmc -s z3 -t B --dump-yw cex.yw -m top out.smt2

Both legs zero-initialize registers (sim -zinit ~ setundef -init -zero),
and the sim leg replays a random per-step input witness whose register
states it re-derives, so the two legs see identical prep semantics.

Classification (per case):
  agree-quiet  BMC UNSAT within bound, sim fired nothing
  agree-cex    BMC SAT at step j and replaying cex.yw in sim fires p0 at j
  A  BMC UNSAT but sim fired at step k <= B   (false proof; most severe)
  B  BMC SAT at step j but cex replay does not fire p0 at j
  C  frontend/sva2check ERROR or unsupported $check on legal syntax
     (deduped by normalized error signature; first repro saved)
  D  yosys/smtbmc crash: nonzero exit without an ERROR line
  E  infrastructure failure (timeout, missing outputs, no Solver banner)
  N  determinism-guard failure: two identical sim runs disagree

Usage:
  python3 fuzz.py --seed 1 --count 300 [--bound 12] [--jobs 8]
                  [--out DIR] [--report FILE] [--no-save]
Failures are saved under OUT/failures/<class>-<seed>-<index>/ with the
.sv, stimulus .yw, sim summary .json, smtbmc log/output, yosys scripts and
a NOTES.txt describing the mismatch and how to re-run it.
"""

import argparse
import concurrent.futures as cf
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
YOSYS = os.environ.get("YOSYS", os.path.join(REPO, "build", "yosys"))
SMTBMC = os.environ.get("SMTBMC", os.path.join(os.path.dirname(YOSYS), "yosys-smtbmc"))

SIM_SCRIPT = """read_verilog -sv -formal case.sv
sva2check
hierarchy -check -top top
proc
flatten
{sim_cmd}
"""

FORMAL_SCRIPT = """read_verilog -sv -formal case.sv
hierarchy -check -top top
sva2check
proc
async2sync
flatten
opt_clean
memory
opt_clean
setundef -init -zero
write_smt2 -wires out.smt2
"""

lock = threading.Lock()
state = {"c_sigs": {}, "results": [], "saved": []}


def run(cmd, timeout):
    try:
        return subprocess.run(cmd, capture_output=True, text=True,
                              timeout=timeout)
    except subprocess.TimeoutExpired:
        return None
    except OSError as e:
        p = subprocess.CompletedProcess(cmd, 127)
        p.stdout, p.stderr = "", "OSError: %s" % e
        return p


def yosys(script_text, cwd, timeout):
    ys = os.path.join(cwd, "script.ys")
    with open(ys, "w") as fh:
        fh.write(script_text)
    try:
        p = subprocess.run([YOSYS, ys], capture_output=True, text=True,
                           timeout=timeout, cwd=cwd)
    except subprocess.TimeoutExpired:
        return None, "timeout after %ds" % timeout
    except OSError as e:
        p = subprocess.CompletedProcess([YOSYS, ys], 127)
        p.stdout, p.stderr = "", "OSError: %s" % e
        return p, p.stdout + "\n" + p.stderr
    return p, p.stdout + "\n" + p.stderr


def first_error(blob):
    for line in blob.splitlines():
        if "ERROR:" in line:
            return line.split("ERROR:", 1)[1].strip()
    return None


def sig_key(msg):
    """Normalize an error message into a dedupe signature."""
    msg = re.sub(r"\$auto\$[^\s;]*", "$auto$", msg)
    msg = re.sub(r"\$sva\$[^\s;]*", "$sva$", msg)
    msg = re.sub(r"\b\d+\b", "N", msg)
    msg = re.sub(r"case\.sv:\d+:\d*", "case.sv:L", msg)
    return msg[:160]


def unsupported_count(blob):
    m = re.search(r"Converted (\d+) \$check cells, (\d+) unsupported", blob)
    return (int(m.group(1)), int(m.group(2))) if m else (None, None)


def read_fired(summary_path):
    with open(summary_path) as fh:
        data = json.load(fh)
    return [a["step"] for a in data.get("assertions", [])]


def save_failure(cls, seed, index, workdir, notes, extra_files=None):
    """Copy a failure repro into OUT/failures/<cls>-<seed>-<index>/.

    Returns the saved directory, or None if this exact directory name was
    already saved (can happen if two classes collide on one seed/index,
    which run_case prevents, so treat as save-anyway).
    """
    outdir = os.path.join(state["failures_root"], "%s-%s-%s" % (cls, seed, index))
    os.makedirs(outdir, exist_ok=True)
    for name in ("case.sv", "stim.yw", "sim.ys", "formal.ys", "sum.json",
                 "sum2.json", "out.smt2", "smtbmc.log", "cex.yw",
                 "sim_leg.log", "formal_leg.log", "replay.log"):
        src = os.path.join(workdir, name)
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(outdir, name))
    for src in extra_files or []:
        if os.path.exists(src):
            shutil.copy2(src, outdir)
    with open(os.path.join(outdir, "NOTES.txt"), "w") as fh:
        fh.write(notes)
    with lock:
        state["saved"].append(outdir)
    return outdir


def c_record(msg, seed, index, workdir, notes_prefix):
    """Register a class-C signature; save a repro only for the first."""
    key = sig_key(msg)
    with lock:
        sig = state["c_sigs"].setdefault(key, {"seed": seed, "index": index,
                                               "count": 0, "dir": None})
        sig["count"] += 1
        first = sig["seed"] == seed and sig["index"] == index
    if first:
        d = save_failure("C", seed, index, workdir,
                         notes_prefix + "\nsignature: %s\n" % msg)
        with lock:
            sig["dir"] = d
    return key


def run_case(seed, index, bound, timeouts, keep_work):
    t0 = time.time()
    workdir = tempfile.mkdtemp(prefix="svafuzz_%d_%d_" % (seed, index))
    res = {"seed": seed, "index": index, "cls": "E", "detail": "",
           "dir": None, "wall": 0.0}

    def fin(cls, detail=""):
        res["cls"] = cls
        res["detail"] = detail
        res["wall"] = time.time() - t0
        if cls in ("A", "B", "D", "N") or (cls == "E" and keep_work):
            res["dir"] = save_failure(cls, seed, index, workdir,
                                      notes_for(cls, detail))
        if not keep_work:
            shutil.rmtree(workdir, ignore_errors=True)
        return res

    def notes_for(cls, detail):
        return ("class %s, seed %d index %d (bound %d)\n%s\n\n"
                "sim leg:    yosys sim.ys      (sim_leg.log, sum.json)\n"
                "formal leg: yosys formal.ys   (formal_leg.log, out.smt2)\n"
                "bmc:        yosys-smtbmc -s z3 -t %d --dump-yw cex.yw -m top out.smt2\n"
                "cex replay: yosys replay.ys   (replay.log, sum2.json)\n"
                % (cls, seed, index, bound, detail, bound))

    sv, stim, meta = gen.build_case(seed, index, bound)
    with open(os.path.join(workdir, "case.sv"), "w") as fh:
        fh.write(sv)
    with open(os.path.join(workdir, "stim.yw"), "w") as fh:
        fh.write(stim)
    with open(os.path.join(workdir, "sim.ys"), "w") as fh:
        fh.write(SIM_SCRIPT.format(sim_cmd="sim -r stim.yw -zinit -summary sum.json"))
    with open(os.path.join(workdir, "formal.ys"), "w") as fh:
        fh.write(FORMAL_SCRIPT)
    with open(os.path.join(workdir, "replay.ys"), "w") as fh:
        fh.write(SIM_SCRIPT.format(sim_cmd="sim -r cex.yw -zinit -summary sum2.json"))

    # ---- sim leg (twice: determinism guard) --------------------------------
    fired = None
    for attempt in (1, 2):
        p, blob = yosys(SIM_SCRIPT.format(
            sim_cmd="sim -r stim.yw -zinit -summary sum.json"),
            workdir, timeouts["yosys"])
        with open(os.path.join(workdir, "sim_leg.log"), "w") as fh:
            fh.write(blob or "")
        if p is None:
            return fin("E", "sim leg timed out (attempt %d)" % attempt)
        if p.returncode != 0:
            err = first_error(blob)
            if err:
                key = c_record(err, seed, index, workdir,
                               "sim leg yosys ERROR (attempt %d)" % attempt)
                return fin("C", "sim-leg error sig: %s" % key)
            return fin("D", "sim leg exit %d, no ERROR line" % p.returncode)
        conv, unsup = unsupported_count(blob)
        if unsup:
            key = c_record("sva2check: %d of %d $check cells unsupported (sim leg)"
                           % (unsup, conv or 0), seed, index, workdir,
                           "sim leg sva2check left %d unsupported $check(s)" % unsup)
            return fin("C", "sim-leg unsupported sig: %s" % key)
        sumpath = os.path.join(workdir, "sum.json")
        if not os.path.exists(sumpath):
            return fin("E", "sim leg produced no sum.json (attempt %d)" % attempt)
        f = read_fired(sumpath)
        if fired is None:
            fired = f
        elif f != fired:
            return fin("N", "sim determinism mismatch: %s vs %s" % (fired, f))

    # ---- formal prep --------------------------------------------------------
    p, blob = yosys(FORMAL_SCRIPT, workdir, timeouts["yosys"])
    with open(os.path.join(workdir, "formal_leg.log"), "w") as fh:
        fh.write(blob or "")
    if p is None:
        return fin("E", "formal prep timed out")
    if p.returncode != 0:
        err = first_error(blob)
        if err:
            key = c_record(err, seed, index, workdir, "formal leg yosys ERROR")
            return fin("C", "formal-prep error sig: %s" % key)
        return fin("D", "formal prep exit %d, no ERROR line" % p.returncode)
    conv, unsup = unsupported_count(blob)
    if unsup:
        key = c_record("sva2check: %d of %d $check cells unsupported (formal leg)"
                       % (unsup, conv or 0), seed, index, workdir,
                       "formal leg sva2check left %d unsupported $check(s)" % unsup)
        return fin("C", "formal-prep unsupported sig: %s" % key)

    # ---- smtbmc --------------------------------------------------------------
    smt2 = os.path.join(workdir, "out.smt2")
    if not os.path.exists(smt2):
        return fin("E", "formal prep produced no out.smt2")
    cexyw = os.path.join(workdir, "cex.yw")
    cmd = [SMTBMC, "-s", "z3", "-t", str(bound), "--dump-yw", cexyw,
           "-m", "top", smt2]
    p = run(cmd, timeouts["smtbmc"])
    log = ""
    if p is not None:
        log = p.stdout + "\n" + p.stderr
    with open(os.path.join(workdir, "smtbmc.log"), "w") as fh:
        fh.write(" ".join(cmd) + "\n\n" + log)
    if p is None:
        return fin("E", "smtbmc timed out after %ds" % timeouts["smtbmc"])
    if "Solver:" not in log:
        return fin("E", "smtbmc produced no Solver banner (exit %d)"
                   % p.returncode)

    step = None
    failed_step = None
    for line in log.splitlines():
        m = re.search(r"Checking assertions in step (\d+)", line)
        if m:
            step = int(m.group(1))
        if re.search(r"Assert failed in \S+: p0\b", line) and failed_step is None:
            failed_step = step

    if failed_step is None:
        # UNSAT within bound: formal sees no failure.
        if fired:
            return fin("A", "BMC UNSAT to bound %d but sim fired p0 at %s"
                       % (bound, fired))
        return fin("agree-quiet", "both legs quiet to bound %d" % bound)

    # SAT at failed_step: replay the cex in sim.
    if not os.path.exists(cexyw):
        return fin("E", "smtbmc SAT at step %d but wrote no cex.yw" % failed_step)
    p, blob = yosys(SIM_SCRIPT.format(
        sim_cmd="sim -r cex.yw -zinit -summary sum2.json"),
        workdir, timeouts["yosys"])
    with open(os.path.join(workdir, "replay.log"), "w") as fh:
        fh.write(blob or "")
    if p is None:
        return fin("E", "cex replay timed out")
    if p.returncode != 0:
        err = first_error(blob)
        if err:
            key = c_record(err, seed, index, workdir,
                           "cex replay yosys ERROR")
            return fin("C", "replay error sig: %s" % key)
        return fin("D", "cex replay exit %d, no ERROR line" % p.returncode)
    fired2 = read_fired(os.path.join(workdir, "sum2.json"))
    if failed_step in fired2:
        return fin("agree-cex", "cex at step %d reproduced in sim" % failed_step)
    return fin("B", "BMC SAT at step %d but cex replay fired p0 at %s"
               % (failed_step, fired2))


def write_report(path, seed, count, bound, elapsed):
    rs = state["results"]
    by = {}
    for r in rs:
        by.setdefault(r["cls"], []).append(r)
    lines = []
    lines.append("# sva_fuzz report -- seed %d, %d cases, bound %d" %
                 (seed, count, bound))
    lines.append("")
    lines.append("- yosys: `%s`" % YOSYS)
    lines.append("- smtbmc: `%s`, solver z3" % SMTBMC)
    lines.append("- wall time: %.1f s (%.2f s/case)" %
                 (elapsed, elapsed / max(1, len(rs))))
    lines.append("")
    lines.append("## Totals")
    lines.append("")
    total = len(rs)
    agree = len(by.get("agree-quiet", [])) + len(by.get("agree-cex", []))
    lines.append("- cases: %d" % total)
    lines.append("- agreement: %d (quiet %d, cex %d)" %
                 (agree, len(by.get("agree-quiet", [])),
                  len(by.get("agree-cex", []))))
    lines.append("- bug-class findings: %d" % (total - agree -
                  len(by.get("E", [])) - len(by.get("N", []))))
    for cls in ("A", "B", "C", "D", "E", "N"):
        n = len(by.get(cls, []))
        if n:
            lines.append("- class %s: %d" % (cls, n))
    nd = by.get("N", [])
    if not nd:
        lines.append("- determinism guard: PASS (all %d cases ran the sim leg "
                     "twice with identical summaries)" % total)
    lines.append("")
    lines.append("## Class C signatures (frontend/lowering gaps, deduped)")
    lines.append("")
    if state["c_sigs"]:
        for key, sig in sorted(state["c_sigs"].items()):
            lines.append("- `%s` -- %d case(s); first repro: `%s` (seed %d "
                         "index %d)" % (key, sig["count"], sig["dir"],
                                        sig["seed"], sig["index"]))
    else:
        lines.append("- none")
    lines.append("")
    for cls in ("A", "B", "D", "N"):
        rows = by.get(cls, [])
        if not rows:
            continue
        lines.append("## Class %s findings" % cls)
        lines.append("")
        for r in rows:
            lines.append("- seed %d index %d: %s" %
                         (r["seed"], r["index"], r["detail"]))
            if r["dir"]:
                lines.append("  - repro: `%s`" % r["dir"])
        lines.append("")
    rows = by.get("E", [])
    if rows:
        lines.append("## Infrastructure notes (class E, not bugs)")
        lines.append("")
        for r in rows:
            lines.append("- seed %d index %d: %s" %
                         (r["seed"], r["index"], r["detail"]))
        lines.append("")
    lines.append("## Full per-case log")
    lines.append("")
    lines.append("| index | class | detail | wall s |")
    lines.append("|---|---|---|---|")
    for r in rs:
        lines.append("| %d | %s | %s | %.2f |" %
                     (r["index"], r["cls"], r["detail"].replace("|", "\\|"),
                      r["wall"]))
    text = "\n".join(lines) + "\n"
    with open(path, "w") as fh:
        fh.write(text)
    return text


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--count", type=int, required=True)
    ap.add_argument("--bound", type=int, default=12)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--out", default=HERE,
                    help="output dir for failures/ and the report")
    ap.add_argument("--report", default=None,
                    help="report path (default OUT/report_seedN.md)")
    ap.add_argument("--timeout-yosys", type=int, default=90)
    ap.add_argument("--timeout-smtbmc", type=int, default=180)
    ap.add_argument("--keep-work", action="store_true",
                    help="keep per-case temp dirs (debugging)")
    args = ap.parse_args()

    if not os.path.exists(YOSYS):
        sys.exit("yosys binary not found: %s" % YOSYS)
    if not os.path.exists(SMTBMC):
        sys.exit("yosys-smtbmc not found: %s" % SMTBMC)

    state["failures_root"] = os.path.join(args.out, "failures")
    os.makedirs(state["failures_root"], exist_ok=True)
    report = args.report or os.path.join(
        args.out, "report_seed%d.md" % args.seed)

    timeouts = {"yosys": args.timeout_yosys, "smtbmc": args.timeout_smtbmc}
    t0 = time.time()
    indices = list(range(args.count))
    with cf.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futs = {ex.submit(run_case, args.seed, i, args.bound, timeouts,
                          args.keep_work): i for i in indices}
        done = 0
        for fut in cf.as_completed(futs):
            r = fut.result()
            with lock:
                state["results"].append(r)
            done += 1
            if r["cls"] not in ("agree-quiet", "agree-cex") or done % 50 == 0:
                print("[%3d/%3d] seed %d index %3d: %-11s %s"
                      % (done, args.count, r["seed"], r["index"], r["cls"],
                         r["detail"][:100]), flush=True)

    with lock:
        state["results"].sort(key=lambda r: r["index"])
    elapsed = time.time() - t0
    text = write_report(report, args.seed, args.count, args.bound, elapsed)
    print("\n" + "\n".join(text.splitlines()[:40]))
    print("...\nfull report: %s" % report)
    by = {}
    for r in state["results"]:
        by[r["cls"]] = by.get(r["cls"], 0) + 1
    print("summary: %s" % json.dumps(by, sort_keys=True))
    # gate semantics: anything but agreement is a failure
    bad = {k: v for k, v in by.items() if k not in ("agree-quiet", "agree-cex")}
    if bad:
        sys.exit(1)


if __name__ == "__main__":
    main()
