#!/usr/bin/env python3
r"""Run the native yosys `ic3` pass on the SVA safety properties of one or
more SystemVerilog files.

Pipeline per file (yosys with the sv-yosys frontend):
    read_verilog -sv -formal; sva2check; proc; async2sync; flatten;
    opt_clean; ic3 -check-cells -init-zero
-- pure RTLIL/SAT, no dffunmap/aigmap/write_aiger, no external solver.

The $check cells compiled by sva2check are lowered to $assert cells by
async2sync (chformal -lower); ic3 enumerates them directly. Registers
without an \init attribute are zero-initialized (-init-zero) to match the
semantics of the ABC PDR oracle in run_pdr.py (yosys-abc's pdr treats
AIGER unknown-init latches as zero; see the run_pdr hand-check recipe,
which also uses -set-init-zero).

Output format mirrors run_pdr.py (property names are reported with the
bad_ prefix, verdicts proved/disproved/undecided) so the two drivers are
diffable. Exit code 0 iff every property is proved.
"""

import argparse
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
{extra_mem}ic3 -check-cells -init-zero{bmc}{ctg}
"""


def prep_extras(path):
    """Per-file opt-in prep passes from pragma comments (multiclock etc.)."""
    a2s = mem = bmc = ctg = ""
    for line in open(path):
        m = re.match(r'\s*//\s*PREP-EXTRA-AFTER-ASYNC2SYNC:\s*(.+)', line)
        if m:
            a2s = m.group(1).strip() + "\n"
        m = re.match(r'\s*//\s*PREP-EXTRA-AFTER-MEMORY:\s*(.+)', line)
        if m:
            mem = m.group(1).strip() + "\n"
        # bounded-reachability pre-pass depth (deep CEXes that would
        # otherwise need one IC3 frame per counterexample step, e.g.
        # t7's ctr-wrap CEXes at depth 256/259)
        m = re.match(r'\s*//\s*IC3-BMC:\s*(\d+)', line)
        if m:
            bmc = " -bmc " + m.group(1)
        # CTG-guided generalization depth (counter-heavy proofs the plain
        # generalizer thrashes on, e.g. tanc2's [=]-antecedent vacuity;
        # measured mixed -- 14x faster there, 3x slower on t10 -- so it
        # stays opt-in rather than a default)
        m = re.match(r'\s*//\s*IC3-CTG:\s*(\d+)', line)
        if m:
            ctg = " -ctg " + m.group(1)
    return a2s, mem, bmc, ctg


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def check_one(yosys, path, top, inc_dirs, timeout):
    label = os.path.basename(path) if isinstance(path, str) else os.path.basename(path[0])
    files = path if isinstance(path, list) else [path]
    inc = " ".join("-I" + d for d in inc_dirs)
    extra_a2s, extra_mem, bmc, ctg = prep_extras(files[0])
    script = PREP.format(inc=inc, files=" ".join(files),
                         top=" -top " + top if top else "",
                         extra_a2s=extra_a2s, extra_mem=extra_mem, bmc=bmc, ctg=ctg)
    ys = os.path.join(tempfile.gettempdir(), f"ic3_{os.getpid()}.ys")
    with open(ys, "w") as fh:
        fh.write(script)
    try:
        proc = run(["zsh", "-f", "-c", 'ulimit -s 65520; exec "$1" "$2"', "y", yosys, ys],
                   timeout=timeout)
    except subprocess.TimeoutExpired:
        print(f"{label}: timed out after {timeout}s")
        return None
    blob = proc.stdout + "\n" + proc.stderr
    if proc.returncode != 0:
        err = ""
        for line in blob.splitlines():
            if "ERROR:" in line:
                err = line.split("ERROR:", 1)[1].strip()
        print(f"{label}: prep-failed: {err[:120]}")
        return None

    conv = re.search(r"Converted (\d+) \$check cells, (\d+) unsupported", blob)
    results = {}
    for m in re.finditer(r"ic3: (\S+): (proved|disproved|undecided)(?: \(depth (\d+)\))?", blob):
        name, status, depth = m.group(1), m.group(2), m.group(3)
        results["bad_" + name] = (status, int(depth) if depth else None)

    summary = re.search(r"ic3: (\d+) properties: proved=(\d+) disproved=(\d+) undecided=(\d+)", blob)
    if not summary:
        print(f"{label}: ic3 produced no summary")
        return None
    total, n_proved, n_dis, n_und = (int(summary.group(i)) for i in range(1, 5))
    if len(results) != total and total != 0:
        print(f"{label}: ic3 reported {total} properties but {len(results)} verdict lines")
    print(f"{label}: {len(results)} properties: proved={n_proved} disproved={n_dis} undecided={n_und}"
          + (f" (sva2check: {conv.group(0)})" if conv else ""), flush=True)
    for name in sorted(results):
        status, depth = results[name]
        extra = f" (frame {depth + 1})" if depth is not None else ""
        print(f"  {status:10s} {name}{extra}", flush=True)
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+", help=".sv files (one design each, or one top module)")
    ap.add_argument("--top", default=None)
    ap.add_argument("--yosys", default=YOSYS)
    ap.add_argument("-I", "--include", action="append", default=[])
    ap.add_argument("--timeout", type=int, default=600)
    args = ap.parse_args()

    ok = True
    for path in args.files:
        r = check_one(args.yosys, path, args.top, args.include, args.timeout)
        if r is None:
            ok = False
            continue
        if any(s != "proved" for s, _ in r.values()):
            ok = False
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
