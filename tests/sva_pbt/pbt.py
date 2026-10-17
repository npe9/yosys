#!/usr/bin/env python3
r"""QuickCheck-style property-based tester for the SVA framework.

Model:    ref_sva.py (independent LRM semantics over finite traces).
Generator: pbt_gen.py (random properties over a fixed design shell,
           random stimulus traces, syntax mutants).
System under test: read_verilog -sv -formal + sva2check + sim (sim leg),
           and + write_smt2 + yosys-smtbmc (BMC leg).

Per case (seed, index):
  1. generate property P and K random traces of T steps
  2. model: fire steps of P on each trace
  3. sim leg: replay each trace, compare fire steps     -> SIM-VERDICT / SIM-STEP
  4. BMC leg to bound T:
       UNSAT but the model fired on some trace          -> BMC-FALSE-PROOF
       SAT at j: replay the cex through the model; must fire exactly at j
                                                       -> BMC-CEX-WRONG
       SAT at j but the model fires earlier on a trace  -> BMC-LATE
  5. frontend/sva2check error on legal syntax           -> UNSUPPORTED
  6. (every N-th case) syntax mutants must be rejected  -> ACCEPTS-INVALID
On a sim-leg mismatch the property AST and the trace are shrunk
(pbt_gen.shrink_candidates, trace truncation) while the mismatch persists;
the smallest repro is saved under failures/.

Correct and incorrect properties: whether P holds on a trace is decided
by the model, so every case exercises both directions (traces that fire
and traces that do not); cases where no sampled trace fires are the
"correct" population and the BMC leg must agree.

Usage: python3 pbt.py --seed 1 --count 100 [--bound 10] [--traces 3] [--jobs 6]
       python3 pbt.py --selftest      (model vs the 10 calibrated cases)
"""
import argparse, json, os, random, re, shutil, subprocess, sys, tempfile, threading
from concurrent.futures import ThreadPoolExecutor
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ref_sva, pbt_gen
from ref_sva import N, Trace

ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
YOSYS = os.environ.get("YOSYS", os.path.join(HERE, "..", "..", "build", "yosys"))
SMTBMC = os.environ.get("SMTBMC", "yosys-smtbmc")
INPUTS = [("a", 1), ("b", 1), ("c", 1), ("d", 1), ("rst", 1)]

SIM_SCRIPT = """read_verilog -sv -formal case.sv
sva2check
hierarchy -check -top top
proc
flatten
sim -r {yw} -zinit -summary {summary}
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

# ---------------------------------------------------------------- traces
def random_trace(rng, T):
    sigs = {n: [rng.randint(0, 1) for _ in range(T)] for n, _ in INPUTS[:4]}
    sigs["rst"] = [1 if rng.random() < 0.08 else 0 for _ in range(T)]
    return derive(sigs)

def derive(sigs):
    T = len(sigs["a"])
    sigs = dict(sigs)
    sigs["r"] = [0] + [sigs["a"][k - 1] for k in range(1, T)]
    sigs["cnt"] = [k % 4 for k in range(T)]
    return Trace(sigs)

def yw_text(tr):
    steps = []
    for k in range(tr.T):
        bits = []
        for name, w in reversed(INPUTS):
            bits.append(str(tr.sigs[name][k] & 1))
        steps.append({"bits": "".join(bits)})
    return json.dumps({"format": "Yosys Witness Trace", "generator": "sva-pbt",
                       "clocks": [{"path": ["\\clk"], "edge": "posedge", "offset": 0}],
                       "signals": [{"path": ["\\" + n], "width": w, "offset": 0, "init_only": False} for n, w in INPUTS],
                       "steps": steps}, indent=1) + "\n"

def trace_from_yw(path, T):
    """Decode a Yosys witness (e.g. smtbmc --dump-yw) into a Trace of the
    design inputs; registers are re-derived.  Bit order per kernel/yw.cc:
    signals in list order occupy the bit string from the END."""
    yw = json.load(open(path))
    sigs = {n: [0] * T for n, _ in INPUTS}
    fields = []
    pos = 0
    for s in yw["signals"]:
        fields.append((s["path"][-1].lstrip("\\"), s["width"], pos, s.get("init_only", False)))
        pos += s["width"]
    total = pos
    last = {n: 0 for n, _ in INPUTS}
    for k, st in enumerate(yw["steps"][:T]):
        bits = st["bits"]
        for name, w, off, init_only in fields:
            if name not in sigs:
                continue
            # field occupies bits[total-off-w : total-off], bit 0 at the highest index
            seg = bits[total - off - w: total - off] if len(bits) == total else None
            if seg is None or not seg.strip():
                v = last[name]
            else:
                ch = seg[-1]
                v = last[name] if ch not in "01" else int(ch)
            sigs[name][k] = v
            last[name] = v
    return derive(sigs)

# ---------------------------------------------------------------- tool legs
def run(cmd, cwd, timeout):
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
        return p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        return -9, "TIMEOUT"

def first_error(blob):
    for line in blob.splitlines():
        if "ERROR" in line:
            return line.strip()[:200]
    return ""

def sim_leg(workdir, tr, idx, timeout):
    yw = "stim%d.yw" % idx; summ = "sum%d.json" % idx
    open(os.path.join(workdir, yw), "w").write(yw_text(tr))
    open(os.path.join(workdir, "sim%d.ys" % idx), "w").write(SIM_SCRIPT.format(yw=yw, summary=summ))
    rc, out = run([YOSYS, "-q", "sim%d.ys" % idx], workdir, timeout)
    if rc != 0:
        return None, first_error(out) or ("rc=%d" % rc)
    data = json.load(open(os.path.join(workdir, summ)))
    return sorted(set(a["step"] for a in data.get("assertions", []))), ""

def bmc_leg(workdir, bound, timeout):
    open(os.path.join(workdir, "formal.ys"), "w").write(FORMAL_SCRIPT)
    rc, out = run([YOSYS, "-q", "formal.ys"], workdir, timeout)
    if rc != 0:
        return None, None, first_error(out) or ("rc=%d" % rc)
    rc, out = run([SMTBMC, "-s", "z3", "-t", str(bound), "--dump-yw", "cex.yw", "-m", "top", "out.smt2"], workdir, timeout)
    if "TIMEOUT" in out and rc == -9:
        return None, None, "smtbmc timeout"
    step = None; failed = None
    for line in out.splitlines():
        m = re.search(r"Checking assertions in step (\d+)", line)
        if m: step = int(m.group(1))
        if re.search(r"Assert failed in \S+: p0\b", line) and failed is None:
            failed = step
    if failed is None and rc not in (0,):
        return None, None, "smtbmc rc=%d: %s" % (rc, out[-200:].strip())
    return failed, (os.path.join(workdir, "cex.yw") if failed is not None else None), ""

# ---------------------------------------------------------------- one case
def check_case(top, traces, workdir, bound, timeout, want_bmc=True):
    """-> (cls, detail).  cls in: OK, UNSUPPORTED, SIM-VERDICT, SIM-STEP,
    BMC-FALSE-PROOF, BMC-CEX-WRONG, BMC-LATE, INFRA."""
    sv = pbt_gen.sv_text(top)
    open(os.path.join(workdir, "case.sv"), "w").write(sv)
    model = [ref_sva.fire_steps(top, tr) for tr in traces]
    for i, tr in enumerate(traces):
        fired, err = sim_leg(workdir, tr, i, timeout)
        if fired is None:
            if "TIMEOUT" in err or "rc=" in err:
                return "INFRA", "sim: " + err
            return "UNSUPPORTED", err
        if fired != model[i]:
            if bool(fired) != bool(model[i]):
                return "SIM-VERDICT", "trace %d: model fires %s, sim fires %s" % (i, model[i], fired)
            return "SIM-STEP", "trace %d: model fires %s, sim fires %s" % (i, model[i], fired)
    if not want_bmc:
        return "OK", "sim-only"
    step, cex, err = bmc_leg(workdir, bound, timeout)
    if err:
        if "timeout" in err or "rc=" in err:
            return "INFRA", "bmc: " + err
        return "UNSUPPORTED", err
    earliest = min([m[0] for m in model if m], default=None)
    if step is None:
        if earliest is not None:
            return "BMC-FALSE-PROOF", "BMC UNSAT to %d but model fires at %s on a sampled trace" % (bound, earliest)
        return "OK", "unsat+quiet"
    if earliest is not None and step > earliest:
        return "BMC-LATE", "BMC first cex at %d, model fires at %d on a sampled trace" % (step, earliest)
    # the witness is exactly as long as smtbmc needed; the model must
    # fire at `step` within that prefix (weak semantics beyond it)
    ctr = trace_from_yw(cex, len(json.load(open(cex))["steps"]))
    cf = ref_sva.fire_steps(top, ctr)
    if step not in cf:
        # smtbmc's witness may leave inputs unconstrained after the failing
        # step; the model only needs the fire at `step`
        return "BMC-CEX-WRONG", "BMC cex at %d, model on that trace fires %s" % (step, cf)
    return "OK", "cex@%d confirmed" % step

# ---------------------------------------------------------------- shrinking
def shrink(top, traces, cls, bound, timeout, limit=60):
    """Greedy shrink of (property, traces) keeping the sim-leg class."""
    best = top; best_traces = traces
    tried = 0
    improved = True
    while improved and tried < limit:
        improved = False
        for cand in pbt_gen.shrink_candidates(best):
            tried += 1
            if tried > limit: break
            with tempfile.TemporaryDirectory() as wd:
                c, _ = check_case(cand, best_traces, wd, bound, timeout, want_bmc=False)
            if c == cls:
                best = cand; improved = True; break
        if not improved and best_traces[0].T > 3:
            # try truncating traces
            T2 = best_traces[0].T - 1
            tr2 = [derive({n: tr.sigs[n][:T2] for n, _ in INPUTS}) for tr in best_traces]
            with tempfile.TemporaryDirectory() as wd:
                c, _ = check_case(best, tr2, wd, bound, timeout, want_bmc=False)
            if c == cls:
                best_traces = tr2; improved = True
    return best, best_traces

# ---------------------------------------------------------------- syntax negatives
def syntax_leg(top, workdir, timeout):
    sv = pbt_gen.sv_text(top)
    bad = []
    for name, text in pbt_gen.syntax_mutants(sv):
        open(os.path.join(workdir, "bad.sv"), "w").write(text)
        rc, out = run([YOSYS, "-q", "-p", "read_verilog -sv -formal bad.sv; hierarchy -check -top top; sva2check"], workdir, timeout)
        if rc == 0:
            bad.append(name)
    return bad

# ---------------------------------------------------------------- driver
lock = threading.Lock()

def one_case(seed, index, args):
    rng = random.Random("pbt:%d:%d" % (seed, index))
    g = pbt_gen.Gen(rng, max_depth=args.depth, budget=args.budget)
    top = g.top()
    traces = [random_trace(rng, args.bound) for _ in range(args.traces)]
    wd = tempfile.mkdtemp(prefix="pbt_")
    try:
        cls, detail = check_case(top, traces, wd, args.bound, args.timeout)
        shrunk = None
        if cls in ("SIM-VERDICT", "SIM-STEP") and not args.no_shrink:
            s_top, s_tr = shrink(top, traces, cls, args.bound, args.timeout)
            shrunk = (s_top, s_tr)
        bad_syntax = []
        if index % args.syntax_every == 0:
            bad_syntax = syntax_leg(top, wd, args.timeout)
        rec = {"seed": seed, "index": index, "class": cls, "detail": detail,
               "prop": pbt_gen.ptop(top), "size": pbt_gen.size(top),
               "accepts_invalid": bad_syntax}
        if shrunk:
            rec["shrunk_prop"] = pbt_gen.ptop(shrunk[0]); rec["shrunk_T"] = shrunk[1][0].T
        if cls not in ("OK",) or bad_syntax:
            save_failure(rec, top, traces, shrunk, wd, args)
        return rec
    finally:
        shutil.rmtree(wd, ignore_errors=True)

def save_failure(rec, top, traces, shrunk, wd, args):
    d = os.path.join(HERE, "failures", "%s_s%d_i%d" % (rec["class"] if rec["class"] != "OK" else "ACCEPTS-INVALID", rec["seed"], rec["index"]))
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "case.sv"), "w").write(pbt_gen.sv_text(top, tag="seed=%d index=%d" % (rec["seed"], rec["index"])))
    for i, tr in enumerate(traces):
        open(os.path.join(d, "stim%d.yw" % i), "w").write(yw_text(tr))
        rec["model_fires_%d" % i] = ref_sva.fire_steps(top, tr)
    if shrunk:
        open(os.path.join(d, "shrunk.sv"), "w").write(pbt_gen.sv_text(shrunk[0], tag="shrunk"))
        for i, tr in enumerate(shrunk[1]):
            open(os.path.join(d, "shrunk_stim%d.yw" % i), "w").write(yw_text(tr))
            rec["shrunk_model_fires_%d" % i] = ref_sva.fire_steps(shrunk[0], tr)
    for f in ("cex.yw", "out.smt2"):
        if os.path.exists(os.path.join(wd, f)):
            shutil.copy(os.path.join(wd, f), d)
    json.dump(rec, open(os.path.join(d, "notes.json"), "w"), indent=1)

def selftest():
    """Model vs the 10 calibrated cases (fire steps measured on the
    framework's sim leg, tests/sva_pbt/README.md)."""
    S = lambda n: N("sig", n)
    def tr(**kw):
        T = 8
        base = {"a": [1] * T, "b": [1] * T, "c": [1] * T, "d": [0] * T, "rst": [0] * T}
        base.update(kw); return derive(base)
    cases = [
        ("a", N("top", None, S("a")), tr(a=[1,1,1,0,1,1,1,1]), [3]),
        ("a |-> ##1 b", N("top", None, N("impl", S("a"), N("delay", N("const",1), 1, 1, S("b")), False)), tr(a=[0,1,0,0,0,0,0,0], b=[1,1,0,1,1,1,1,1]), [2]),
        ("a |=> b", N("top", None, N("impl", S("a"), S("b"), True)), tr(a=[0,1,0,0,0,0,0,0], b=[1,1,0,1,1,1,1,1]), [2]),
        ("b |-> $past(a)", N("top", None, N("impl", S("b"), N("past","a",1), False)), tr(a=[1,0,1,1,1,1,1,1], b=[0,0,1,0,0,0,0,0]), [2]),
        ("disable iff (rst) a", N("top", S("rst"), S("a")), tr(a=[1,1,1,0,1,1,1,1], rst=[0,0,0,1,0,0,0,0]), []),
        ("a ##1 b |-> c", N("top", None, N("impl", N("delay", S("a"),1,1,S("b")), S("c"), False)), tr(a=[0,1,0,0,0,0,0,0], b=[0,0,1,0,0,0,0,0], c=[1,1,0,1,1,1,1,1]), [2]),
        ("a |-> ##[1:2] b", N("top", None, N("impl", S("a"), N("delay", N("const",1),1,2,S("b")), False)), tr(a=[0,1,0,0,0,0,0,0], b=[1,1,0,0,1,1,1,1]), [3]),
        ("r == $past(a)", N("top", None, N("or", N("and", S("r"), N("past","a",1)), N("and", N("not",S("r")), N("not",N("past","a",1))))), tr(a=[1,0,1,0,1,0,1,0]), []),
        ("cnt==2 |-> a", N("top", None, N("impl", N("cnteq",2), S("a"), False)), tr(a=[1,1,0,1,1,1,0,1]), [2,6]),
        ("$rose(a) |-> b", N("top", None, N("impl", N("rose","a"), S("b"), False)), tr(a=[0,1,1,0,1,1,1,1], b=[1,0,1,1,1,1,1,1]), [1]),
    ]
    bad = 0
    for name, top, t, want in cases:
        got = ref_sva.fire_steps(top, t)
        ok = got == want
        bad += not ok
        print("%-22s model=%s expected=%s %s" % (name, got, want, "ok" if ok else "MISMATCH"))
    print("selftest:", "PASS" if not bad else "FAIL (%d)" % bad)
    return 1 if bad else 0

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--count", type=int, default=50)
    ap.add_argument("--bound", type=int, default=10)
    ap.add_argument("--traces", type=int, default=3)
    ap.add_argument("--depth", type=int, default=3)
    ap.add_argument("--budget", type=int, default=18)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("--syntax-every", type=int, default=10)
    ap.add_argument("--no-shrink", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        sys.exit(selftest())
    recs = []
    with ThreadPoolExecutor(args.jobs) as ex:
        for rec in ex.map(lambda i: one_case(args.seed, i, args), range(args.count)):
            recs.append(rec)
            if rec["class"] != "OK" or rec["accepts_invalid"]:
                print("[%s] seed=%d index=%d %s :: %s%s" % (rec["class"], rec["seed"], rec["index"], rec["detail"],
                      rec["prop"], (" | ACCEPTS-INVALID %s" % rec["accepts_invalid"]) if rec["accepts_invalid"] else ""))
                if "shrunk_prop" in rec:
                    print("      shrunk: %s  (T=%d)" % (rec["shrunk_prop"], rec["shrunk_T"]))
    from collections import Counter
    c = Counter(r["class"] for r in recs)
    inv = sum(1 for r in recs if r["accepts_invalid"])
    print("sva_pbt seed=%d: %s; accepts-invalid cases: %d" % (args.seed, dict(c), inv))
    severe = sum(v for k, v in c.items() if k not in ("OK", "UNSUPPORTED", "INFRA")) + inv
    sys.exit(1 if severe else 0)

if __name__ == "__main__":
    main()
