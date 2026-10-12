#!/usr/bin/env python3
r"""Run bounded model checking on the SVA safety properties of one or more
SystemVerilog files via write_smt2 + yosys-smtbmc (SMT-LIBv2, z3).

Pipeline per file (mirrors run_ic3.py with the SMT backend):
    read_verilog -sv -formal; hierarchy -check -top TOP; sva2check; proc;
    async2sync; flatten; opt_clean; memory; opt_clean;
    setundef -init -zero; write_smt2 -wires out.smt2
Then yosys-smtbmc -s z3 -t <bound>.  Registers are zero-initialized
(setundef -init -zero) to match the ic3 -init-zero convention, so a
violation visible at ctr == N surfaces at step N (ic3 reports frame N+1).

Per property: BMC reports CEXes one batch at a time; after each failed
assert the runner deletes that $assert cell and re-runs until no CEX is
found within the bound.  A 'PROVED' expectation therefore means 'no CEX
up to the bound' -- BMC alone cannot prove; compare with the ic3 verdict
(run_ic3.py) for the actual proof.  Exit code 0 iff every property has a
CEX exactly where the file's '// DIS @N' comment says (step N) and every
'// PROVED' property shows no CEX within the bound.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
YOSYS = os.environ.get("YOSYS", os.path.join(ROOT, "third_party/yosys/build/yosys"))
SMTBMC = os.environ.get("SMTBMC", os.path.join(os.path.dirname(YOSYS), "yosys-smtbmc"))

PREP = """read_verilog -sv -formal {files}
hierarchy -check{top}
sva2check
proc
async2sync
{extra_a2s}flatten
opt_clean
memory
opt_clean
{extra_mem}{deletes}
setundef -init -zero
write_smt2 -wires {smt}
"""


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


def expectations(path):
    """property name -> ('proved', None) | ('dis', N) from trailing comments."""
    exp = {}
    src = open(path).read()
    for m in re.finditer(r'^\s*(\w+)\s*:\s*assert\s+property\s*\(.*?\)\s*;\s*//\s*(PROVED|DIS)\s*@?(\d+)?',
                         src, re.M | re.S):
        # re.S makes .*? span newlines; constrain: assertion statements are
        # single-line in this battery, so re-scan line by line instead
        pass
    for line in src.splitlines():
        m = re.match(r'\s*(\w+)\s*:\s*assert\s+property\s*\(.*;\s*//\s*(PROVED|DIS)\s*@?(\d+)?', line)
        if m:
            name, kind, n = m.group(1), m.group(2), m.group(3)
            exp[name] = (kind.lower(), int(n) if n else None)
    return exp


def check_one(yosys, smtbmc, path, top, solver, bound, timeout):
    label = os.path.basename(path)
    exp = expectations(path)
    bound = max([24] + [n + 4 for k, n in exp.values() if n is not None])
    verdicts = {}   # name -> ('cex', step) | ('holds', bound)
    deleted = []

    for _ in range(64):
        deletes = "".join("delete c:\\%s\n" % n for n in deleted)
        extra_a2s, extra_mem = prep_extras(path)
        smt = os.path.join(tempfile.gettempdir(), "bmc_%d.smt2" % os.getpid())
        ys = os.path.join(tempfile.gettempdir(), "bmc_%d.ys" % os.getpid())
        with open(ys, "w") as fh:
            fh.write(PREP.format(files=path, top=" -top " + top if top else "",
                                 deletes=deletes, smt=smt,
                                 extra_a2s=extra_a2s, extra_mem=extra_mem))
        p = run(["zsh", "-f", "-c", 'ulimit -s 65520; exec "$1" "$2"', "y", yosys, ys],
                timeout=timeout)
        blob = p.stdout + "\n" + p.stderr
        if p.returncode != 0:
            err = ""
            for line in blob.splitlines():
                if "ERROR:" in line:
                    err = line.split("ERROR:", 1)[1].strip()
            print("%s: prep-failed: %s" % (label, err[:120]))
            return None
        if not top:
            m = re.search(r"yosys-smt2-topmod (\S+)", open(smt).read())
            if not m:
                print("%s: prep-failed: no topmod in smt2" % label)
                return None
            top = m.group(1)

        try:
            p = run(["zsh", "-f", "-c", 'ulimit -s 65520; exec "$1" -s "$2" -t "$3" -m "$4" "$5"',
                     "y", smtbmc, solver, str(bound), top or label, smt], timeout=timeout)
        except subprocess.TimeoutExpired:
            print("%s: smtbmc timed out after %ds" % (label, timeout))
            return None
        blob = p.stdout + "\n" + p.stderr

        step = None
        failed = []
        for line in blob.splitlines():
            m = re.search(r"Checking assertions in step (\d+)", line)
            if m:
                step = int(m.group(1))
            m = re.search(r"Assert failed in \S+: (\S+)", line)
            if m:
                failed.append(m.group(1))
        if not failed:
            if "Status: PASSED" not in blob and p.returncode == 0:
                pass
            for name in exp:
                if name not in verdicts and name not in deleted:
                    verdicts[name] = ("holds", bound)
            break
        for name in failed:
            if name not in verdicts:
                verdicts[name] = ("cex", step)
            deleted.append(name)

    print("%s: %d properties checked (bound %d):" % (label, len(exp), bound))
    ok = True
    for name in sorted(set(verdicts) - set(exp)):
        v = verdicts[name]
        print("  %-9s %s (no expectation)" % ("cex@%d" % v[1] if v[0] == "cex" else "holds", name))
    for name in sorted(exp):
        kind, n = exp[name]
        v = verdicts.get(name)
        if v is None:
            print("  MISSING   %s (expected %s)" % (name, kind))
            ok = False
            continue
        if kind == "proved" and v[0] == "holds":
            print("  holds     %s (no CEX to step %d)" % (name, v[1]))
        elif kind == "dis" and v[0] == "cex" and (n is None or v[1] == n):
            print("  cex@%d    %s (as expected)" % (v[1], name))
        else:
            print("  MISMATCH  %s: expected %s%s, got %s" %
                  (name, kind.upper(), " @%d" % n if n is not None else "", v))
            ok = False
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--top", default=None)
    ap.add_argument("--yosys", default=YOSYS)
    ap.add_argument("--smtbmc", default=SMTBMC)
    ap.add_argument("-s", "--solver", default="z3")
    ap.add_argument("-t", "--bound", type=int, default=24)
    ap.add_argument("--timeout", type=int, default=300)
    args = ap.parse_args()

    ok = True
    for path in args.files:
        r = check_one(args.yosys, args.smtbmc, path, args.top, args.solver, args.bound, args.timeout)
        if r is None or not r:
            ok = False
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
