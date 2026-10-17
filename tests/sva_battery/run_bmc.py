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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run_ic3  # lowering_tags()

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


def check_one(yosys, smtbmc, path, top, solver, bound, timeout, prove,
              stream=None, dump_yw=None):
    label = os.path.basename(path)
    exp = expectations(path)

    def sline(msg):
        # incremental verdict events for the prove.py conductor
        if stream:
            with open(stream, "a") as fh:
                fh.write(msg + "\n")

    if "BMC-SKIP" in open(path).read():
        # ic3-only features write_smt2 cannot encode ($fair cells today):
        # the expectation lives in run_battery's ic3 leg
        print("%s: skipped (BMC-SKIP pragma)" % label)
        sline("SKIP")
        sline("DONE")
        return True
    bound = max([24] + [n + 4 for k, n in exp.values() if n is not None])
    verdicts = {}   # name -> ('cex', step) | ('holds', bound)
    deleted = []
    skipped = set()  # liveness-flavored $check cells write_smt2 cannot
                     # encode (warning at prep) -- no assertion exists in
                     # the SMT, so 'holds'/'proved' for them would be
                     # vacuous; their verdict comes from the ic3 leg

    for rnd in range(64):
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
        for m in re.finditer(r"skipping liveness-flavored \$check cell \S+\.(\S+)",
                             blob):
            skipped.add(m.group(1))
        if p.returncode != 0:
            err = ""
            for line in blob.splitlines():
                if "ERROR:" in line:
                    err = line.split("ERROR:", 1)[1].strip()
            print("%s: prep-failed: %s" % (label, err[:120]))
            sline("ERROR prep-failed: %s" % err[:120])
            sline("DONE")
            return None
        if not top:
            m = re.search(r"yosys-smt2-topmod (\S+)", open(smt).read())
            if not m:
                print("%s: prep-failed: no topmod in smt2" % label)
                return None
            top = m.group(1)

        try:
            # options must precede the .smt2 file: smtbmc parses with
            # plain getopt and stops at the first non-option argument
            cmd = [smtbmc, "-s", solver, "-t", str(bound), "-m", top or label]
            yw_name = None
            if dump_yw:
                # one witness per round (smtbmc stops at the first failing
                # step by default); the conductor replays it with sim -r
                yw_name = "smt_r%d.yw" % rnd
                cmd += ["--dump-yw", os.path.join(dump_yw, yw_name)]
            cmd.append(smt)
            p = run(["zsh", "-f", "-c", 'ulimit -s 65520; exec "$@"', "y"] + cmd, timeout=timeout)
        except subprocess.TimeoutExpired:
            print("%s: smtbmc timed out after %ds" % (label, timeout))
            sline("ERROR smtbmc-timeout %d" % timeout)
            sline("DONE")
            return None
        blob = p.stdout + "\n" + p.stderr
        # smtbmc's exit code is not a reliable error signal (it is nonzero
        # for ordinary cex/induction-failed outcomes); a real invocation
        # failure never reaches the "Solver:" progress banner.
        if "Solver:" not in blob:
            err = ""
            for line in blob.splitlines():
                if "error" in line.lower() or "ERROR" in line:
                    err = line.strip()[:120]
            print("%s: smtbmc failed: %s" % (label, err or "exit %d, no output" % p.returncode))
            sline("ERROR smt-failed: %s" % (err or "exit %d" % p.returncode))
            sline("DONE")
            return None

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
                if name not in verdicts and name not in deleted and \
                        name not in skipped:
                    verdicts[name] = ("holds", bound)
            break
        for name in failed:
            if name not in verdicts:
                verdicts[name] = ("cex", step)
                sline("VERDICT %s cex@%d" % (name, step))
                if yw_name:
                    sline("WITNESS %s %s" % (name, yw_name))
            deleted.append(name)

    # proof upgrade: with --prove, run one temporal-induction pass over the
    # surviving properties. base case was already checked to bound above
    # (per-property, with cex steps); this pass only upgrades "holds" to
    # "proved" on "Temporal induction successful." -- its Assert-failed /
    # FAILED output for a spurious step-case cex is ignored by design.
    if prove:
        remaining = [n for n in exp if n in verdicts and verdicts[n][0] == "holds"
                     and n not in skipped]
        if remaining:
            cmd = [smtbmc, "-s", solver, "-t", str(bound), "-m", top or label, "-i", smt]
            try:
                p = run(["zsh", "-f", "-c", 'ulimit -s 65520; exec "$@"', "y"] + cmd, timeout=timeout)
            except subprocess.TimeoutExpired:
                p = None
            if p is not None and "Temporal induction successful." in (p.stdout + p.stderr):
                for name in remaining:
                    verdicts[name] = ("proved", bound)

    for name in exp:
        if name not in verdicts:
            continue
        kind, vbound = verdicts[name]
        if kind == "cex":
            continue  # streamed when found
        sline("VERDICT %s %s@%d" % (name, kind, vbound))
    sline("DONE")

    print("%s: %d properties checked (bound %d):" % (label, len(exp), bound))
    tags = run_ic3.lowering_tags(yosys, [path], top)
    lt = lambda name: " [lowering=%s]" % tags.get(name, "untagged")
    ok = True
    for name in sorted(set(verdicts) - set(exp)):
        v = verdicts[name]
        print("  %-9s %s (no expectation)%s" % ("cex@%d" % v[1] if v[0] == "cex" else "holds", name, lt(name)))
    for name in sorted(exp):
        kind, n = exp[name]
        if name in skipped:
            # write_smt2 dropped the liveness-flavored $check: no assertion
            # in the SMT, so this leg has nothing to check (ic3 leg decides)
            print("  skipped   %s (liveness $check not SMT-encodable, ic3-only)%s"
                  % (name, lt(name)))
            continue
        if kind == "dis" and n is None:
            # liveness-class expectation (s_until/s_eventually family): the
            # bmc leg checks the safety half only, the F-half is ic3-only
            # (write_smt2 skips the liveness legs) -- not a mismatch, just
            # not bmc-checkable
            print("  skipped   %s (liveness DIS, ic3-only)%s" % (name, lt(name)))
            continue
        v = verdicts.get(name)
        if v is None:
            print("  MISSING   %s (expected %s)" % (name, kind))
            ok = False
            continue
        if kind == "proved" and v[0] == "proved":
            print("  proved    %s (temporal induction)%s" % (name, lt(name)))
        elif kind == "proved" and v[0] == "holds":
            print("  holds     %s (no CEX to step %d)%s" % (name, v[1], lt(name)))
        elif kind == "dis" and v[0] == "cex" and (n is None or v[1] == n):
            print("  cex@%d    %s (as expected)%s" % (v[1], name, lt(name)))
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
    ap.add_argument("--prove", action="store_true",
                    help="also run smtbmc temporal induction (-i); properties "
                         "with successful induction are reported as proved")
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--stream", default=None,
                    help="append incremental VERDICT/ERROR/DONE lines for "
                         "the prove.py conductor")
    ap.add_argument("--dump-yw", default=None,
                    help="directory for per-round .yw counterexample "
                         "witnesses (one per BMC round, smt_rN.yw); the "
                         "conductor replays them with sim -r")
    args = ap.parse_args()

    ok = True
    for path in args.files:
        r = check_one(args.yosys, args.smtbmc, path, args.top, args.solver,
                      args.bound, args.timeout, args.prove, stream=args.stream,
                      dump_yw=args.dump_yw)
        if r is None or not r:
            ok = False
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
