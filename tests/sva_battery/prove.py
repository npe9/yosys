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
       induction upgrades survivors to proved; BMC-SKIP files emit SKIP;
       with --cex each round also dumps its counterexample witness
       .yw via smtbmc --dump-yw and streams WITNESS name file events)
  ic3: yosys <run_ic3 PREP + pragmas>         (per-property verdict
       lines stream from the pass as properties decide; with --cex the
       pass gets -cex <dir>/ic3_ and writes <dir>/ic3_<prop>.yw per
       disproof)

With --cex <dir>, every safety-only disproof is then RE-VERIFIED by
replaying its witness: same prep as the settling lane plus
`sim -r <w> -summary <json> -vcd <vcd>`, and the summary must record
the assertion firing at exactly the recorded cex step (REPLAY-OK).
Liveness ($live leg) disproofs are lasso traces and stay manual.
This closes the loop between formal counterexamples and executable
debugging traces (register item 4, cex -> VCD export + replay).

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
       [--cex <dir>]   keep .yw witnesses + sim -r replays; re-verify
                       every safety disproof by replay
"""

import argparse
import json
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


class DummyLane:
    """Stand-in for a disabled lane (--smt-only/--ic3-only): no events,
    already done, kill is a no-op."""

    def __init__(self):
        self.events = queue.Queue()
        self.done = True

    def poll(self):
        return True

    def kill(self):
        pass


def safe_name(name):
    """ic3's witness-file sanitization: only [alnum_-] survives."""
    return "".join(ch if (ch.isalnum() or ch in "_-") else "_" for ch in name)


def replay_confirm(yosys, path, top, include, witness, settle_engine,
                   label, step, cexdir, timeout):
    """Replay a .yw witness with `sim -r` on the settling lane's own prep
    and confirm the assertion fires exactly at the recorded cex step.
    Returns (ok, detail).  Writes <cexdir>/replay_<label>.{vcd,json}."""
    vcd = os.path.join(cexdir, "replay_%s.vcd" % safe_name(label))
    summary = os.path.join(cexdir, "replay_%s.json" % safe_name(label))
    if settle_engine == "smt":
        # the SMT witness was generated against run_bmc's prep (which adds
        # setundef -init -zero); replay against exactly that.  The deleted-
        # assert rounds don't matter: check cells are logic sinks, so the
        # undeleted design evolves identically under the same inputs.
        extra_a2s, extra_mem = run_bmc.prep_extras(path)
        script = run_bmc.PREP.format(svaflags=run_ic3.sva_flags(path), guard=run_ic3.degraded_guard(path), flags=run_ic3.read_flags(path), files=path,
                                     top=" -top " + top if top else "",
                                     deletes="", smt=os.devnull,
                                     extra_a2s=extra_a2s, extra_mem=extra_mem)
    else:
        inc = " ".join("-I" + d for d in include)
        extra_a2s, extra_mem, bmc, ctg = run_ic3.prep_extras(path)
        script = run_ic3.PREP.format(svaflags=run_ic3.sva_flags(path), flags=run_ic3.read_flags(path), inc=inc, files=path,
                                     top=" -top " + top if top else "",
                                     extra_a2s=extra_a2s, extra_mem=extra_mem,
                                     engine=run_ic3.engine_for(path) + bmc + ctg)
    script += "sim -r %s -summary %s -vcd %s\n" % (witness, summary, vcd)
    ys = os.path.join(cexdir, "replay_%s.ys" % safe_name(label))
    with open(ys, "w") as fh:
        fh.write(script)
    try:
        proc = subprocess.run(
            ["zsh", "-f", "-c", 'ulimit -s 65520; exec "$1" "$2"',
             "y", yosys, ys],
            capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return False, "replay timed out after %ds" % timeout
    if proc.returncode != 0 and not os.path.exists(summary):
        err = ""
        for line in (proc.stdout + proc.stderr).splitlines():
            if "ERROR:" in line:
                err = line.split("ERROR:", 1)[1].strip()
        return False, "replay failed: %s" % (err[:120] or "exit %d"
                                             % proc.returncode)
    try:
        doc = json.load(open(summary))
    except (OSError, ValueError) as e:
        return False, "replay produced no readable summary: %s" % e
    fired = [a["step"] for a in doc.get("assertions", [])
             if a.get("path") and a["path"][-1].lstrip("\\") == label]
    if step in fired:
        return True, "assertion fires at step %d (of %d)" % \
            (step, doc.get("steps"))
    return False, "assertion fires at %s, expected step %d" % \
        (fired or "no steps", step)


class SmtLane:
    """run_bmc.py --prove as a subprocess; events via its --stream file."""

    def __init__(self, path, top, include, timeout, workdir, dump_yw=None):
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
        if dump_yw:
            os.makedirs(dump_yw, exist_ok=True)
            cmd += ["--dump-yw", dump_yw]
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
            m = re.match(r"WITNESS (\S+) (\S+)", line)
            if m:
                name, _ = norm(m.group(1))
                self.events.put(("witness", name, m.group(2), "safety"))
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
            except PermissionError:
                # macOS: EPERM when the group leader has already exited
                # (a zombie group) -- kill the child itself instead
                try:
                    self.proc.kill()
                except ProcessLookupError:
                    pass
            self.proc.wait()


class Ic3Lane:
    """yosys ic3 pass as a subprocess; per-property lines from stdout."""

    def __init__(self, path, top, include, workdir, cex_prefix=None):
        self.events = queue.Queue()
        self.done = False
        inc = " ".join("-I" + d for d in include)
        extra_a2s, extra_mem, bmc, ctg = run_ic3.prep_extras(path)
        engine = run_ic3.engine_for(path) + bmc + ctg
        if cex_prefix:
            os.makedirs(os.path.dirname(cex_prefix) or ".", exist_ok=True)
            engine += " -cex " + cex_prefix
        script = run_ic3.PREP.format(svaflags=run_ic3.sva_flags(path), flags=run_ic3.read_flags(path), inc=inc, files=path,
                                     top=" -top " + top if top else "",
                                     extra_a2s=extra_a2s, extra_mem=extra_mem,
                                     engine=engine)
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
            except PermissionError:
                # macOS: EPERM when the group leader has already exited
                # (a zombie group) -- kill the child itself instead
                try:
                    self.proc.kill()
                except ProcessLookupError:
                    pass
            self.proc.wait()


def prove_file(path, args, workdir):
    t0 = time.time()
    exp = run_bmc.expectations(path)
    cexdir = None
    if args.cex:
        cexdir = os.path.join(args.cex, os.path.splitext(os.path.basename(path))[0])
        os.makedirs(cexdir, exist_ok=True)
    args.top = args.top or run_ic3.top_pragma(path)   # `// TOP: <module>` for multi-root files
    smt = SmtLane(path, args.top, args.include, args.timeout, workdir,
                   dump_yw=cexdir) if not args.ic3_only else DummyLane()
    ic3 = Ic3Lane(path, args.top, args.include, workdir,
                  cex_prefix=(os.path.join(cexdir, "ic3_") if cexdir else None)) \
        if not args.smt_only else DummyLane()
    settled = {}   # name -> (verdict, step, engine, elapsed)
    legs = {}      # name -> {leg: (kind, step, engine)}
    soft = {}      # name -> (kind, step, engine)  non-definitive bests
    witnesses = {}  # name -> (yw path, engine) from WITNESS events / ic3 naming
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
        if kind == "witness":
            witnesses.setdefault(name, (os.path.join(cexdir, step), engine))
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
    if cexdir and not timed_out:
        # ic3 logs the verdict line just BEFORE exporting the witness; on
        # early exit give the lane a moment to finish those exports so
        # every disproof settled by ic3 has its .yw for the replay.
        want = [n for n, (v, s, e, _el) in settled.items()
                if v.startswith("dis") and e == "ic3"]
        deadline = time.time() + 15
        while time.time() < deadline and \
                not all(os.path.exists(os.path.join(cexdir, "ic3_%s.yw" % safe_name(n)))
                        for n in want):
            time.sleep(0.1)
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
    ltags = run_ic3.lowering_tags(YOSYS, [path], args.top, args.include)
    n_const = 0
    for name, (v, step, engine, el) in sorted(verdicts.items()):
        tag = {"proved": "PROVED", "holds": "holds-to-bound",
               "undecided": "UNDECIDED"}.get(v, v)
        # register v3 B5: a proof of a constant-folded body (`exact:const`)
        # proves nothing about the design; it gets its own label, as in
        # run_battery.py, and its own count
        if v == "proved" and ltags.get(name, "").endswith(":const"):
            tag = "PROVED(const)"
            n_const += 1
        print("  %-16s %-14s (%s%s) [lowering=%s]" % (tag, name, engine,
                                        ", %.1fs" % el if el is not None else "",
                                        ltags.get(name, "untagged")))
    if n_const:
        print("  (%d of the proofs are PROVED(const): the property body folded to a constant)" % n_const)
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
    if cexdir:
        # Counterexample replay: every safety-only disproof settles with a
        # .yw witness; play it back through `sim -r` on the settling lane's
        # prep and require the assertion to fire at exactly the recorded
        # step.  Liveness ($live leg) disproofs are lasso traces and stay
        # manual for now.
        for name, (v, step, engine, _el) in sorted(settled.items()):
            if not v.startswith("dis"):
                continue
            if set(legs.get(name, {})) != {"safety"}:
                continue
            witness = None
            if engine == "ic3":
                # prefer the settling lane's own witness (ic3's filename
                # is derivable; smt's comes from the WITNESS stream event)
                cand = os.path.join(cexdir, "ic3_%s.yw" % safe_name(name))
                if os.path.exists(cand):
                    witness = (cand, engine)
            if witness is None:
                witness = witnesses.get(name)
            if witness is None:
                print("  REPLAY-MISS  %s (no %s-lane witness)"
                      % (name, engine))
                ok = False
                continue
            wpath, wengine = witness
            if not os.path.exists(wpath):
                print("  REPLAY-MISS  %s (%s missing)" % (name, wpath))
                ok = False
                continue
            rok, detail = replay_confirm(YOSYS, path, args.top, args.include,
                                         wpath, wengine, name, step, cexdir,
                                         timeout=min(180, args.timeout))
            if rok:
                print("  REPLAY-OK    %s @%d (%s witness; %s)"
                      % (name, step, wengine, detail))
            else:
                print("  REPLAY-FAIL  %s @%d: %s" % (name, step, detail))
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
    ap.add_argument("--cex", default=None,
                    help="directory kept for counterexample artifacts: "
                         "per-property .yw witnesses from both lanes plus "
                         "sim -r replays (.vcd + summary .json); every "
                         "safety disproof is re-verified by replay")
    ap.add_argument("--smt-only", action="store_true",
                    help="run only the SMT (run_bmc) lane -- bisect engine "
                         "disagreements; settles every disproof with an "
                         "smtbmc witness")
    ap.add_argument("--ic3-only", action="store_true",
                    help="run only the IC3 lane -- bisect engine "
                         "disagreements")
    args = ap.parse_args()

    if args.cex:
        os.makedirs(args.cex, exist_ok=True)

    ok = True
    for path in args.files:
        with tempfile.TemporaryDirectory(prefix="prove_") as workdir:
            if not prove_file(path, args, workdir):
                ok = False
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
