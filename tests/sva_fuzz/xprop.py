#!/usr/bin/env python3
r"""Differential X-propagation fuzzer: yosys `sim` vs a Python 4-state
reference implementing the kernel's own const_* semantics (kernel/calc.cc).

Each case is a random small circuit: uninitialized registers (the X
source) + expression trees over {inputs, regs, constants} drawn from ops
whose yosys semantics were read directly from calc.cc:

  and/or/xor/xnor/not/neg  computed at the cell Y_WIDTH (context width);
                           any-x operand -> ALL result bits x (big2const)
  shl/shr                  left operand is CONTEXT-DETERMINED like any other
                           binop operand: genrtlil passes width_hint into the
                           left subtree (verified: $shl A_WIDTH = context for
                           logic/comparison/reduce/concat/nested-shift lefts;
                           a plain leaf keeps its self width as A_WIDTH and
                           the kernel zx/truncs it to Y_WIDTH at eval, which
                           is the same value); only the shift AMOUNT is
                           self-determined; x amount -> all-x
  mux                      x select -> per-bit merge (a==b ? a : x)
  eq/ne                    definite bit mismatch -> 0/1; else x if any x
  lt/le                    any x -> x
  reductions               fold of the per-bit tables
  logic_not                definite 1 anywhere -> 0 (const2big isZero);
                           else x if any x; else 1
  concat/slice             structural

Expression trees follow SystemVerilog self-determined widths bottom-up
(self_expr), wrapped in zx where a context extends them -- the same
elaboration genrtlil performs (verified against $cell A/B/Y_WIDTH dumps).

A mismatch means sim diverges from the kernel's documented semantics (a
sim bug) or the reference misread calc.cc -- both are reported.

Usage: python3 xprop.py --seed N --count M [--out DIR]
"""

import argparse
import json
import os
import random
import subprocess
import sys
import tempfile

YOSYS = os.environ.get("YOSYS", os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "build/yosys"))


# ---------------------------------------------------------------------------
# 4-state values: strings MSB-first over '0', '1', 'x'

def zx(v, w):
    v = v[-w:] if len(v) > w else "0" * (w - len(v)) + v
    return v


def has_x(v):
    return "x" in v


def op_and(a, b):
    return "".join("0" if (x == "0" or y == "0") else
                   "1" if (x == "1" and y == "1") else "x"
                   for x, y in zip(a, b))


def op_or(a, b):
    return "".join("1" if (x == "1" or y == "1") else
                   "0" if (x == "0" and y == "0") else "x"
                   for x, y in zip(a, b))


def op_xor(a, b):
    return "".join("x" if (x not in "01" or y not in "01") else
                   ("1" if x != y else "0") for x, y in zip(a, b))


def op_not(a):
    return "".join("1" if x == "0" else "0" if x == "1" else "x" for x in a)


def op_addsub(a, b, sub, w):
    # calc.cc const_add/const_sub: any x -> all-x (big2const)
    if has_x(a) or has_x(b):
        return "x" * w
    av, bv = int(a, 2), int(b, 2)
    r = av - bv if sub else av + bv
    return format(r % (1 << w), "0%db" % w)


def op_neg(a, w):
    # calc.cc const_neg: 0 - a at result width; any x -> all-x
    return op_addsub("0" * w, a, True, w)


def op_shift(a, amt, w, left):
    # calc.cc const_shift_worker: x amount -> all-x; x data shifts, fill 0
    if has_x(amt):
        return "x" * w
    n = int(amt, 2)
    a = zx(a, w)
    if n >= w:
        return "0" * w
    if left:
        return a[n:] + "0" * n
    return "0" * n + a[:w - n]


def op_mux(s, a, b):
    if s == "1":
        return a
    if s == "0":
        return b
    return "".join(x if x == y else "x" for x, y in zip(a, b))


def op_eq(a, b, ne):
    # calc.cc const_eq: definite mismatch -> 0; else x if any x; else 1
    for x, y in zip(a, b):
        if (x, y) in (("0", "1"), ("1", "0")):
            return "1" if ne else "0"
    if has_x(a) or has_x(b):
        return "x"
    return "0" if ne else "1"


def op_lt(a, b, le):
    # calc.cc const_lt/le: any x -> x
    if has_x(a) or has_x(b):
        return "x"
    av, bv = int(a, 2), int(b, 2)
    return "1" if (av <= bv if le else av < bv) else "0"


def op_reduce(a, kind):
    if kind == "and":
        r = "1"
        for x in a:
            r = op_and(r, x)
    else:
        r = "0"
        fn = op_or if kind == "or" else op_xor
        for x in a:
            r = fn(r, x)
    return r


def op_logic_not(a):
    # calc.cc const_logic_not: const2big + isZero -- a definite 1 forces
    # 0 even when other bits are x; x only if no 1 and some x; else 1
    if "1" in a:
        return "0"
    if has_x(a):
        return "x"
    return "1"


def ctxify(node, w):
    """Re-elaborate a self_expr subtree as if generated under context width
    w -- push w through context-propagating nodes (and/or/xor/xnor/add/sub/
    not/neg, mux arms). Self-determined roots (shifts, comparisons,
    reductions, logic_not, concat) ignore context and stay; eval's zx pads
    their results exactly like the cells' Y_WIDTH padding."""
    k = node[0]
    if k == "zx":
        # push the context through the extension: the wrapped subtree
        # re-elaborates at w (a ("sub", ..., 1) under ("zx", _, 3)
        # becomes ("sub", ..., 3) -- cells compute at the context width)
        return ("zx", ctxify(node[1], w), w)
    if k in ("and", "or", "xor", "xnor", "add", "sub"):
        return (k, ctxify(node[1], w), ctxify(node[2], w), w)
    if k in ("not", "neg"):
        return (k, ctxify(node[1], w), w)
    if k == "mux":
        return (k, node[1], ctxify(node[2], w), ctxify(node[3], w), w)
    if k in ("shl", "shr"):
        return (k, ctxify(node[1], w), node[2], w)
    return node


def eval_node(node, env):
    k = node[0]
    if k == "zx":
        return zx(eval_node(node[1], env), node[2])
    if k == "leaf":
        return env[node[1]]
    if k == "const":
        return node[1]
    if k == "not":
        return op_not(zx(eval_node(node[1], env), node[2]))
    if k in ("and", "or", "xor", "xnor"):
        a = zx(eval_node(node[1], env), node[3])
        b = zx(eval_node(node[2], env), node[3])
        if k == "and":
            return op_and(a, b)
        if k == "or":
            return op_or(a, b)
        if k == "xor":
            return op_xor(a, b)
        return op_not(op_xor(a, b))
    if k in ("add", "sub"):
        return op_addsub(zx(eval_node(node[1], env), node[3]),
                         zx(eval_node(node[2], env), node[3]),
                         k == "sub", node[3])
    if k == "neg":
        return op_neg(zx(eval_node(node[1], env), node[2]), node[2])
    if k in ("shl", "shr"):
        return op_shift(eval_node(node[1], env), eval_node(node[2], env),
                        node[3], k == "shl")
    if k == "mux":
        return op_mux(zx(eval_node(node[1], env), 1),
                      zx(eval_node(node[2], env), node[4]),
                      zx(eval_node(node[3], env), node[4]))
    if k in ("eq", "ne"):
        a = eval_node(node[1], env)
        b = eval_node(node[2], env)
        w = max(len(a), len(b))
        return op_eq(zx(a, w), zx(b, w), k == "ne")
    if k in ("lt", "le"):
        a = eval_node(node[1], env)
        b = eval_node(node[2], env)
        w = max(len(a), len(b))
        return op_lt(zx(a, w), zx(b, w), k == "le")
    if k == "reduce":
        return op_reduce(eval_node(node[1], env), node[2])
    if k == "bool":
        # mux select: yosys ReduceBool -- fold of the per-bit or table
        # (or-fold of a single bit from S0 is the bit itself)
        return op_reduce(eval_node(node[1], env), "or")
    if k == "logic_not":
        return op_logic_not(eval_node(node[1], env))
    if k == "concat":
        return eval_node(node[1], env) + eval_node(node[2], env)
    if k == "slice":
        v = eval_node(node[1], env)
        hi, lo = node[2], node[3]
        return v[len(v) - 1 - hi: len(v) - lo]
    raise ValueError("bad node %r" % (node,))


# ---------------------------------------------------------------------------
# case generation: self-determined widths bottom-up, zx at context edges

class Gen:
    LOGIC = ["and", "or", "xor", "xnor"]

    def __init__(self, rng):
        self.r = rng
        self.n_in = rng.choice([2, 3, 4])
        self.inputs = ["a", "b", "c", "d"][:self.n_in]
        self.regs = [("r%d" % i, rng.choice([1, 2, 3, 4]))
                     for i in range(rng.choice([1, 2, 3]))]
        self.n_out = rng.choice([2, 3, 4])

    def self_leaf(self):
        roll = self.r.random()
        if roll < 0.4:
            return ("leaf", self.r.choice(self.inputs)), 1
        if roll < 0.55:
            bits = self.r.choice(["0", "1", "00", "11", "101", "1010"])
            return ("const", bits), len(bits)
        name, w = self.r.choice(self.regs)
        return ("leaf", name), w

    def self_expr(self, d):
        """-> (node, width) at SystemVerilog self-determined widths"""
        if d <= 0 or self.r.random() < 0.3:
            return self.self_leaf()
        roll = self.r.random()
        if roll < 0.36:
            op = self.r.choice(self.LOGIC + ["add", "sub"])
            a, wa = self.self_expr(d - 1)
            b, wb = self.self_expr(d - 1)
            w = max(wa, wb)
            # arms compute AT w (extend-then-operate), not extend-result:
            # yosys gives each arm genRTLIL(w) -- e.g. (0 - (~1'b1)) at
            # w=2 is 10 - 11 = 11, not zx(0-1, 2) = 01
            return (op, ctxify(a, w), ctxify(b, w), w), w
        if roll < 0.46:
            a, w = self.self_expr(d - 1)
            return ("not", ("zx", a, w), w), w
        if roll < 0.53:
            a, w = self.self_expr(d - 1)
            return ("neg", ("zx", a, w), w), w
        if roll < 0.63:
            a, wa = self.self_expr(d - 1)
            b, wb = self.self_expr(d - 1)
            return (self.r.choice(["shl", "shr"]), a, b, wa), wa
        if roll < 0.71:
            s, _ = self.self_expr(d - 1)
            a, wa = self.self_expr(d - 1)
            b, wb = self.self_expr(d - 1)
            w = max(wa, wb)
            return ("mux", ("bool", s), ctxify(a, w), ctxify(b, w), w), w
        if roll < 0.81:
            a, wa = self.self_expr(d - 1)
            b, wb = self.self_expr(d - 1)
            w = max(wa, wb)
            # comparison operands are evaluated at max(self widths):
            # extend-then-operate INSIDE the operand subtree (yosys
            # detectSignWidthWorker + genRTLIL(width_hint); e.g.
            # 2'b00 != ~1'b1 is TRUE because ~ evaluates at 2 bits -> 2'b10)
            return (self.r.choice(["eq", "ne", "lt", "le"]),
                    ctxify(a, w), ctxify(b, w)), 1
        if roll < 0.87:
            a, w = self.self_expr(d - 1)
            return ("reduce", a, self.r.choice(["and", "or", "xor"])), 1
        if roll < 0.93:
            a, w = self.self_expr(d - 1)
            return ("logic_not", a), 1
        # concat of two self-determined arms
        a, wa = self.self_expr(d - 1)
        b, wb = self.self_expr(d - 1)
        return ("concat", a, b), wa + wb

    def expr_w(self, d, tw):
        """context-width expression -> (node, width actually produced).

        Context-propagating ops (and/or/xor/xnor/add/sub/not/neg, mux
        arms, shift left operands) compute at max(tw, operand widths) --
        a fat concat arm lifts the whole tree to its width, truncated
        only at the assignment (genWidthRTLIL detect pass). Comparison
        operands evaluate at max(operand self widths) with the context
        pushed INSIDE each operand (extend-then-operate; xnor and ~ give
        1-bits above the operand width, so result-extension would be
        wrong). Self-determined ops (shift amounts, mux selects,
        reductions, logic_not, concat) keep their own widths.
        """
        if d <= 0 or self.r.random() < 0.25:
            n, w = self.self_leaf()
            return ("zx", n, tw), tw
        roll = self.r.random()
        if roll < 0.34:
            op = self.r.choice(self.LOGIC + ["add", "sub"])
            a, wa = self.expr_w(d - 1, tw)
            b, wb = self.expr_w(d - 1, tw)
            # like genWidthRTLIL: detectSignWidthWorker lifts the context
            # to max(context, operand widths) -- a fat concat arm makes
            # the WHOLE binop compute at its width, truncated only at the
            # assignment (verified: r1 + ({r1,r2} >> d) -> $add width 7)
            w = max(tw, wa, wb)
            return (op, ctxify(a, w), ctxify(b, w), w), w
        if roll < 0.44:
            a, wa = self.expr_w(d - 1, tw)
            w = max(tw, wa)
            return ("not", ctxify(a, w), w), w
        if roll < 0.52:
            a, wa = self.expr_w(d - 1, tw)
            w = max(tw, wa)
            return ("neg", ctxify(a, w), w), w
        if roll < 0.62:
            # shift: left operand is context-determined (genrtlil passes
            # width_hint into the left subtree, verified by $shl probes);
            # only the amount is self-determined; the shift computes at
            # max(context, left width)
            a, wa = self.expr_w(d - 1, tw)
            b, wb = self.self_expr(d - 1)
            w = max(tw, wa)
            return (self.r.choice(["shl", "shr"]), ctxify(a, w), b, w), w
        if roll < 0.70:
            # select is self-determined (own width, then reduce_bool);
            # arms take the context like any operand (verified by probe:
            # s ? 1'b0 : (~1'b1) at ctx 6 -> 111110, and reductions as
            # arms get Y_WIDTH = context)
            s, _ = self.self_expr(d - 1)
            a, wa = self.expr_w(d - 1, tw)
            b, wb = self.expr_w(d - 1, tw)
            w = max(tw, wa, wb)
            return ("mux", ("bool", s), ctxify(a, w), ctxify(b, w), w), w
        if roll < 0.80:
            a, wa = self.self_expr(d - 1)
            b, wb = self.self_expr(d - 1)
            w = max(wa, wb)
            return (self.r.choice(["eq", "ne", "lt", "le"]),
                    ctxify(a, w), ctxify(b, w)), 1
        if roll < 0.86:
            a, w = self.self_expr(d - 1)
            return ("reduce", a, self.r.choice(["and", "or", "xor"])), 1
        if roll < 0.92:
            a, w = self.self_expr(d - 1)
            return ("logic_not", a), 1
        # concat of two self-determined arms
        a, wa = self.self_expr(d - 1)
        b, wb = self.self_expr(d - 1)
        return ("concat", a, b), wa + wb

    def expr(self, d, tw):
        """context-width wrapper: result exactly tw wide"""
        node, w = self.expr_w(d, tw)
        return node if w == tw else ("zx", node, tw)

    def expr_sv(self, n):
        k = n[0]
        if k == "zx":
            return self.expr_sv(n[1])
        if k == "leaf":
            return n[1]
        if k == "const":
            return "%d'b%s" % (len(n[1]), n[1])
        if k == "not":
            return "(~%s)" % self.expr_sv(n[1])
        if k in ("and", "or", "xor", "xnor", "add", "sub", "eq", "ne",
                 "lt", "le"):
            sym = {"and": "&", "or": "|", "xor": "^", "xnor": "~^",
                   "add": "+", "sub": "-", "eq": "==", "ne": "!=",
                   "lt": "<", "le": "<="}[k]
            return "(%s %s %s)" % (self.expr_sv(n[1]), sym, self.expr_sv(n[2]))
        if k == "neg":
            return "(-%s)" % self.expr_sv(n[1])
        if k in ("shl", "shr"):
            return "(%s %s %s)" % (self.expr_sv(n[1]),
                                   "<<" if k == "shl" else ">>",
                                   self.expr_sv(n[2]))
        if k == "mux":
            return "(%s ? %s : %s)" % (self.expr_sv(n[1][1]),
                                       self.expr_sv(n[2]), self.expr_sv(n[3]))
        if k == "reduce":
            return "(%s%s)" % ({"and": "&", "or": "|", "xor": "^"}[n[2]],
                               self.expr_sv(n[1]))
        if k == "logic_not":
            return "(!%s)" % self.expr_sv(n[1])
        if k == "concat":
            return "{%s, %s}" % (self.expr_sv(n[1]), self.expr_sv(n[2]))
        raise ValueError(k)

    def case(self):
        outs = [self.expr(self.r.randint(1, 3), 6)
                for _ in range(self.n_out)]
        dnets = []
        for j, (name, w) in enumerate(self.regs):
            if j > 0 and self.r.random() < 0.5:
                dnets.append(("zx", ("leaf", self.regs[self.r.randrange(j)][0]),
                              w))
            else:
                d = ("zx", ("leaf", self.r.choice(self.inputs)), 1)
                for _ in range(w - 1):
                    d = ("concat", d,
                         ("zx", ("leaf", self.r.choice(self.inputs)), 1))
                dnets.append(d)
        sv = ["module top(input clk, %s);" %
              ", ".join("input %s" % s for s in self.inputs)]
        for name, w in self.regs:
            sv.append("  logic [%d:0] %s;" % (w - 1, name))
        for i, e in enumerate(outs):
            sv.append("  logic [5:0] o%d;" % i)
            sv.append("  assign o%d = %s;" % (i, self.expr_sv(e)))
        for (name, w), d in zip(self.regs, dnets):
            sv.append("  always_ff @(posedge clk) %s <= %s;"
                      % (name, self.expr_sv(d)))
        sv.append("endmodule")
        return "\n".join(sv) + "\n", outs, dnets


def make_stim(rng, n_in, steps=6):
    yw = {
        "format": "Yosys Witness Trace",
        "generator": "xprop-fuzz",
        "clocks": [{"path": ["\\clk"], "edge": "posedge", "offset": 0}],
        "signals": [{"path": ["\\%s" % "abcd"[i]], "width": 1, "offset": 0,
                     "init_only": False} for i in range(n_in)],
        "steps": [],
    }
    for _ in range(steps):
        yw["steps"].append({"bits": "".join(
            "1" if rng.random() < 0.5 else "0" for _ in range(n_in))})
    return json.dumps(yw, indent=1) + "\n", steps


# ---------------------------------------------------------------------------
# VCD parsing: posedge-slot values of the outputs (MSB-first strings)

def parse_vcd(path):
    names = {}
    widths = {}
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if line.startswith("$var"):
                p = line.split()
                widths[p[3]] = int(p[2])
                names[p[3]] = p[4]
            if line.startswith("$enddefinitions"):
                break
        vals = {}
        slots = []
        cur = None
        cur_t = None
        for line in fh:
            line = line.strip()
            if line.startswith("#"):
                t = int(line[1:])
                # one witness step per 10-unit clock period: sample the
                # posedge slot only, not the negedge sub-slot
                if cur is not None and cur_t is not None and cur_t % 10 == 0:
                    vals.update(cur)
                    slots.append(dict(vals))
                cur = {}
                cur_t = t
            elif line and line[0] in "bBxX" and cur is not None:
                body, ref = line[1:].split()
                cur[ref] = body.lower()
        if cur is not None and cur_t is not None and cur_t % 10 == 0:
            vals.update(cur)
            slots.append(dict(vals))
    out = []
    for slot in slots:
        d = {}
        for ref, v in slot.items():
            if names.get(ref, "").startswith("o"):
                d[names[ref]] = zx(v, widths[ref])
        out.append(d)
    return out


def run_case(seed, index, workdir):
    rng = random.Random("x%d:%d" % (seed, index))
    g = Gen(rng)
    sv, outs, dnets = g.case()
    stim, nsteps = make_stim(rng, g.n_in)
    svf = os.path.join(workdir, "case.sv")
    ywf = os.path.join(workdir, "stim.yw")
    vcdf = os.path.join(workdir, "out.vcd")
    open(svf, "w").write(sv)
    open(ywf, "w").write(stim)
    # NB: no flatten here, and proc -noopt: opt_expr's same-operand folds
    # (x^x -> 0, a!=a -> 0, a-a -> 0) are 2-state-isms that destroy
    # X-pessimism (x^x = x per calc.cc); proc alone normalizes the design
    # for sim
    script = ("read_verilog -sv %s; proc -noopt; sim -r %s -vcd %s"
              % (svf, ywf, vcdf))
    p = subprocess.run([YOSYS, "-p", script], capture_output=True, text=True,
                       timeout=60)
    if p.returncode != 0 or not os.path.exists(vcdf):
        return ("infra", (p.stderr + p.stdout)[-300:])
    slots = parse_vcd(vcdf)
    if len(slots) < nsteps:
        return ("infra", "only %d posedge slots (%d steps)"
                % (len(slots), nsteps))
    state = {name: "x" * w for name, w in g.regs}
    steps = json.load(open(ywf))["steps"]
    mism = []
    # sim's slot 0 leaves cell outputs unevaluated (initial all-x state is
    # not propagated through cells until inputs dirty them): skip the
    # comparison there but advance state through slot 0's edge
    for k in range(nsteps):
        inputs = {}
        bitstr = steps[k]["bits"]
        for i in range(g.n_in):
            inputs["abcd"[i]] = bitstr[g.n_in - 1 - i]
        if k > 0:
            env = dict(inputs)
            env.update(state)
            ref = {"o%d" % i: zx(eval_node(e, env), 6)
                   for i, e in enumerate(outs)}
            got = slots[k]
            for name in sorted(ref):
                gval = got.get(name)
                if gval != ref[name]:
                    mism.append((k, name, ref[name], gval))
        state = {name: zx(eval_node(dnets[j], dict(inputs, **state)), w)
                 for j, (name, w) in enumerate(g.regs)}
    if mism:
        return ("mismatch", mism)
    return ("agree", None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--count", type=int, default=100)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    ok = 0
    bad = []
    outdir = args.out or tempfile.mkdtemp(prefix="xprop_")
    os.makedirs(outdir, exist_ok=True)
    for index in range(args.count):
        workdir = os.path.join(outdir, "case_%04d" % index)
        os.makedirs(workdir, exist_ok=True)
        cls, detail = run_case(args.seed, index, workdir)
        if cls == "agree":
            ok += 1
        else:
            bad.append((index, cls, detail))
        if (index + 1) % 25 == 0:
            print("[%d/%d] ok=%d bad=%d" % (index + 1, args.count, ok,
                                            len(bad)), flush=True)
    print("summary: %d/%d agreement, %d findings"
          % (ok, args.count, len(bad)))
    for index, cls, detail in bad[:10]:
        print("case %d: %s" % (index, cls))
        if cls == "mismatch":
            for m in detail[:6]:
                print("   step %d %s ref=%s sim=%s" % m)
        else:
            print("   %s" % detail)
    sys.exit(0 if not bad else 1)


if __name__ == "__main__":
    main()
