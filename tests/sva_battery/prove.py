#!/usr/bin/env python3
r"""Proof orchestrator ("conductor"): run the SMT bounded-model-checking
lane (run_bmc.py --prove) and the native IC3 lane (the yosys `ic3` pass,
same prep as run_ic3.py) in PARALLEL on one file's SVA safety properties,
first definitive per-property verdict wins.

Why: run_ic3.py's internal sequence is bmc-pre-pass -> k-induction ->
IC3 frames, all sequential in one process, and the battery's two legs
run one after the other.  Running the lanes concurrently and merging
verdicts as they arrive costs max(lane times) instead of sum(lane
times), and the per-property engine attribution is reported so "who
proved it" stays auditable.  Which lane wins a given property is a
wall-clock race (for a depth-0 counterexample ic3's -kind base case is
exactly as fast as one BMC round) -- the flag is recorded, not presumed.

Lanes:
  smt: run_bmc.py --prove --stream <events>   (BMC rounds; temporal
       induction upgrades survivors to proved; BMC-SKIP files emit SKIP)
  ic3: yosys <run_ic3 PREP + pragmas>         (per-property verdict
       lines stream from the pass as properties decide)

Merge policy per property (first definitive wins):
  cex@N (smt)             -> DIS@N   (sound disproof)
  proved  (ic3 or smt)    -> PROVED
  disproved depth D (ic3) -> DIS@D   (D = frame-1 = cex step)
A settled property is never re-opened; if the other lane later reports
a conflicting definitive verdict, or the two lanes disagree on the cex
depth, the conductor reports CONFLICT and exits 1 -- both engines are
sound on the identical prep, so that indicates an engine bug (the
battery's sequential cross-check is the referee).  After both lanes
finish, unsettled properties fall back to the best soft verdict: smt
'holds' or ic3 'undecided' -> holds-to-bound (inconclusive).

Expectations: the same per-property '// PROVED' / '// DIS @N' comments
as the battery (liveness DIS without @N matches any ic3 depth).  Exit 0
iff every expected property received its expected verdict, no conflicts
were observed, and all --expect-engine attributions hold.

Usage: python3 prove.py <file.sv>... [--timeout N] [--expect-engine name=lane]
"""

import argparse
import os
import queue
import re
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

YOSYS = os.environ.get("YOSYS", os.path.join(ROOT, "build/yosys"))

import run_bmc  # expectations(), PREP
import run_ic3  # PREP, prep_extras()


def norm(line_name):
    """Split a logged property name into (label, leg): ic3 logs the safety
    leg of a liveness check under the bare label and the two-phase
    liveness result under '<label>$live' (run_battery.py's convention)."""
    name = line_name.lstrip("\\")
    leg = "safety"
    if name.endswith("$live"):
        name = name[:-5]
        leg = "live"
    if name.startswith("bad_"):
        name = name[4:]
    return name, leg


def spawn(cmd):
    # own process group so kill() can take the lane's current tool child
    # along (run_bmc execs yosys/zsh; the group covers grandchildren)
    return subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                            start_new_session=True)


class SmtLane:
    """run_bmc.py --prove as a subprocess; events via its --stream file."""

    def __init__(self, path, top, include, timeout, workdir):
        self.events = queue.Queue()
        self.stream = os.path.join(workdir, "smt_events.txt")
        self.done = False
        self.offset = 0
        cmd = [sys.executable, os.path.join(HERE, "run_bmc.py"), path,
               "--prove", "--timeout", str(timeout), "--stream", self.stream]
        if top:
            cmd += ["--top", top]
        for d in include:
            cmd += ["-I", d]
        self.proc = spawn(cmd)

    def poll(self):
        """Drain new stream lines into the event queue; True at DONE."""
        try:
            with open(self.stream) as fh:
                fh.seek(self.offset)
                lines = fh.read().splitlines()
                self.offset = fh.tell()
        except OSError:
            lines = []
        for line in lines:
            m = re.match(r"VERDICT (\S+) (\w+)@(\d+)", line)
            if m:
                name, _ = norm(m.group(1))
                self.events.put((m.group(2), name, int(m.group(3)), "safety"))
            elif line == "SKIP":
                self.events.put(("skip", None, None, None))
            elif line.startswith("ERROR"):
                self.events.put(("error", line, None, None))
            elif line == "DONE":
                self.done = True
        if self.proc.poll() is not None and not self.done:
            # process gone without DONE (crash/kill) -- surface it once
            self.events.put(("error", "smt lane exited rc=%d without DONE"
                             % self.proc.returncode, None))
            self.done = True
        return self.done

    def kill(self):
        if self.proc.poll() is None:
            try:
                os.killpg(self.proc.pid, 9)
            except ProcessLookupError:
                pass
            self.proc.wait()


class Ic3Lane:
    """yosys ic3 pass as a subprocess; per-property lines from stdout."""

    def __init__(self, path, top, include, workdir):
        self.events = queue.Queue()
        self.done = False
        inc = " ".join("-I" + d for d in include)
        extra_a2s, extra_mem, bmc, ctg = run_ic3.prep_extras(path)
        script = run_ic3.PREP.format(inc=inc, files=path,
                                     top=" -top " + top if top else "",
                                     extra_a2s=extra_a2s, extra_mem=extra_mem,
                                     bmc=bmc, ctg=ctg)
        ys = os.path.join(workdir, "prove_ic3.ys")
        with open(ys, "w") as fh:
            fh.write(script)
        self.proc = subprocess.Popen(
            ["zsh", "-f", "-c", 'ulimit -s 65520; exec "$1" "$2"',
             "y", YOSYS, ys],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            bufsize=1, start_new_session=True)
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()

    def _reader(self):
        try:
            for line in self.proc.stdout:
                m = re.search(r"ic3: (\S+): (proved|disproved|undecided)"
                              r"(?: \(depth (\d+)\))?", line)
                if m:
                    name, leg = norm(m.group(1))
                    self.events.put((m.group(2), name,
                                     int(m.group(3)) if m.group(3) else None,
                                     leg))
                elif "ERROR:" in line:
                    self.events.put(("error", line.strip(), None, None))
        finally:
            self.done = True

    def poll(self):
        return self.done

    def kill(self):
        if self.proc.poll() is None:
            try:
                os.killpg(self.proc.pid, 9)
            except ProcessLookupError:
                pass
            self.proc.wait()


def prove_file(path, args, workdir):
    t0 = time.time()
    exp = run_bmc.expectations(path)
    smt = SmtLane(path, args.top, args.include, args.timeout, workdir)
    ic3 = Ic3Lane(path, args.top, args.include, workdir)
    settled = {}   # name -> (verdict, step, engine, elapsed)
    legs = {}      # name -> {leg: (kind, step, engine)}
    soft = {}      # name -> (kind, step, engine)  non-definitive bests
    problems = []

    def combined(name):
        """Definitive combined verdict from the legs seen so far, or None.
        Mirrors run_battery.py: a liveness check (one with a $live leg) is
        disproved if ANY leg disproved (safety leg's depth first, matching
        the battery's @N convention for liveness DIS), proved only when
        every present leg proved.  Safety-only checks settle on the safety
        leg alone."""
        lg = legs.get(name, {})
        s = lg.get("safety")
        l = lg.get("live")
        if s is None and l is None:
            return None
        if l is not None:
            if s is not None and s[0] == "disproved":
                return ("dis@%d" % s[1], s[1])
            if l[0] == "disproved":
                return ("dis@%d" % l[1], l[1])
            if s is not None and s[0] == "proved" and l[0] == "proved":
                return ("proved", None)
            return None
        if s[0] == "proved":
            return ("proved", None)
        if s[0] == "disproved":
            return ("dis@%d" % s[1], s[1])
        return None  # undecided / holds -- soft only

    def handle(evt, engine):
        kind, name, step, leg = evt
        if kind == "skip":
            return
        if kind == "error":
            problems.append("lane error (%s): %s" % (engine, name))
            return
        # map lane verdicts onto leg kinds
        lk = {"cex": "disproved", "holds": "undecided"}.get(kind, kind)
        lg = legs.setdefault(name, {})
        if leg in lg and lg[leg][2] != engine:
            pk, ps, pe = lg[leg]
            clash = None
            if pk != lk:
                clash = "%s/%s after %s by %s" % (leg, lk, pk, pe)
            elif lk == "disproved" and ps is not None and step is not None \
                    and ps != step:
                clash = "%s/%s@%d after %s@%d by %s" % (leg, lk, step, lk, ps, pe)
            if clash:
                problems.append("%s: %s conflicts: %s" % (name, engine, clash))
        else:
            lg.setdefault(leg, (lk, step, engine))
        prev = settled.get(name)
        if prev is not None:
            # name already settled: a late leg/verdict is a conflict only
            # if it changes the combined verdict (per-leg opposites like
            # safety-disproved + live-proved are normal for liveness)
            now = combined(name)
            if now is not None and now[0] != prev[0]:
                problems.append("%s: %s late %s/%s changes settled verdict %s"
                                % (name, engine, leg, lk, prev[0]))
        else:
            verdict = combined(name)
            if verdict is not None:
                settled[name] = (verdict[0], verdict[1], engine,
                                 time.time() - t0)
            elif lk == "undecided":
                soft.setdefault(name, (lk, step, engine))

    early = False
    while time.time() - t0 < args.timeout:
        while not smt.events.empty():
            handle(smt.events.get(), "smt")
        while not ic3.events.empty():
            handle(ic3.events.get(), "ic3")
        if smt.poll():
            while not smt.events.empty():
                handle(smt.events.get(), "smt")
        if ic3.poll():
            while not ic3.events.empty():
                handle(ic3.events.get(), "ic3")
        if smt.done and ic3.done:
            break
        if not args.no_early_exit and exp and \
                all(n in settled for n in exp):
            early = True
            break
        time.sleep(0.1)
    timed_out = not (smt.done and ic3.done) and not early
    smt.kill()
    ic3.kill()

    label = os.path.basename(path)
    note = " (early exit: expectations settled)" if early else \
           " (deadline hit)" if timed_out else ""
    print("%s: conductor wall %.1fs%s" % (label, time.time() - t0, note))
    verdicts = {}
    for name, (v, step, engine, el) in settled.items():
        verdicts[name] = (v, step, engine, el)
    for name, (v, step, engine) in soft.items():
        if name not in verdicts:
            verdicts[name] = (v, step, engine, None)
    ok = True
    for name, (v, step, engine, el) in sorted(verdicts.items()):
        tag = {"proved": "PROVED", "holds": "holds-to-bound",
               "undecided": "UNDECIDED"}.get(v, v)
        print("  %-16s %-14s (%s%s)" % (tag, name, engine,
                                        ", %.1fs" % el if el is not None else ""))
    for name in sorted(set(exp) - set(verdicts)):
        print("  MISSING          %s" % name)
        ok = False
    for name, (kind, n) in sorted(exp.items()):
        v = verdicts.get(name)
        if v is None:
            continue
        verdict, step = v[0], v[1]
        if kind == "proved" and verdict == "proved":
            pass
        elif kind == "dis" and verdict.startswith("dis") and \
                (n is None or step == n):
            pass
        else:
            print("  MISMATCH %s: expected %s%s, got %s (step %s)"
                  % (name, kind.upper(), " @%d" % n if n is not None else "",
                     verdict.upper(), step))
            ok = False
    for p in problems:
        print("  CONFLICT " + p)
        ok = False
    for want in args.expect_engine:
        name, lane = want.split("=", 1)
        got = verdicts.get(name)
        if got is None or got[2] != lane:
            print("  ENGINE-MISMATCH %s: expected %s lane, got %s"
                  % (name, lane, got[2] if got else "no verdict"))
            ok = False
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--top", default=None)
    ap.add_argument("-I", "--include", action="append", default=[])
    ap.add_argument("--timeout", type=int, default=300,
                    help="per-file wall budget for BOTH lanes (default 300)")
    ap.add_argument("--no-early-exit", action="store_true",
                    help="let both lanes run to completion even after all "
                         "expectations are settled (weakens the wall-clock "
                         "win but lets the loser cross-check depths)")
    ap.add_argument("--expect-engine", action="append", default=[],
                    help="per-property lane attribution check, name=smt|ic3")
    args = ap.parse_args()

    ok = True
    for path in args.files:
        with tempfile.TemporaryDirectory(prefix="prove_") as workdir:
            if not prove_file(path, args, workdir):
                ok = False
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
