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


def op_mux(s, a, b, pes=False, poison_w=None):
    if s == "1":
        return a
    if s == "0":
        return b
    if pes:
        # -xprop=pessimistic: any x in the select poisons the mux -- but
        # only at the cell's self width; the zero-extension to the
        # assignment context stays definite (the real $mux cell is
        # max(arm self widths) wide and zx-ed at the connect)
        w = len(a) if poison_w is None else poison_w
        return zx("x" * w, len(a))
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


def ctxify(node, w, widths):
    """Re-elaborate a self_expr subtree as if generated under context width
    w -- push w through context-propagating nodes (and/or/xor/xnor/add/sub/
    not/neg, mux arms). Self-determined roots (shifts, comparisons,
    reductions, logic_not, concat) ignore context and stay; eval's zx pads
    their results exactly like the cells' Y_WIDTH padding.  A mux node's
    width field is its CELL width (max arm produced widths under the mux's
    own hint), so pushing a new context recomputes the cell -- the
    context reaches the cell only through the 1-bit-unary/concat arm
    kinds (arm_w)."""
    k = node[0]
    if k == "zx":
        # push the context through the extension: the wrapped subtree
        # re-elaborates at w (a ("sub", ..., 1) under ("zx", _, 3)
        # becomes ("sub", ..., 3) -- cells compute at the context width)
        return ("zx", ctxify(node[1], w, widths), w)
    if k in ("and", "or", "xor", "xnor", "add", "sub"):
        return (k, ctxify(node[1], w, widths), ctxify(node[2], w, widths), w)
    if k in ("not", "neg"):
        return (k, ctxify(node[1], w, widths), w)
    if k == "mux":
        cw = max(arm_w(node[2], w, widths), arm_w(node[3], w, widths))
        return (k, node[1], ctxify(node[2], cw, widths),
                ctxify(node[3], cw, widths), cw)
    if k in ("shl", "shr"):
        return (k, ctxify(node[1], w, widths), node[2], w)
    return node


def arm_w(node, hint, widths):
    """genRTLIL-produced width of a ternary arm generated under
    width_hint=hint (genrtlil.cc AST_TERNARY: cell WIDTH = max of the
    arm produced sizes; the result is then extended to hint at the
    connect).  Leaves/consts ignore the hint; the 1-bit unary/comparison
    ops emit Y at max(hint,1) (probe-verified: $eq, $reduce_*,
    $logic_not arms all carry Y_WIDTH = context); concat extends to the
    full hint; context-propagating ops (and/or/xor/xnor/add/sub/not/
    neg, shifts) recurse with the same hint (the comparison-operand
    extension pushes the max-operand width INSIDE each operand, so a
    unary arm under a wider sibling emits at the sibling width --
    verified: `A != (s ? -d : r1)` with 4-bit A gives $neg Y_WIDTH 4)."""
    k = node[0]
    if k == "zx":
        return arm_w(node[1], -1, widths)
    if k == "leaf":
        return widths[node[1]]
    if k == "const":
        return len(node[1])
    if k == "bool":
        # $reduce_bool: arg wider than 1 -> cell at max(hint,1);
        # 1-bit arg is returned as-is
        sw = arm_w(node[1], -1, widths)
        return max(hint, 1) if sw > 1 else sw
    if k in ("reduce", "logic_not", "eq", "ne", "lt", "le"):
        return max(hint, 1)
    if k == "mux":
        return node[4]
    if k == "concat":
        # concat children are self-determined, but the node extends to
        # the full hint at the end (genrtlil.cc AST_CONCAT)
        s = arm_w(node[1], -1, widths) + arm_w(node[2], -1, widths)
        return max(s, hint) if hint > 0 else s
    # context-propagating ops (and/or/xor/xnor/add/sub/not/neg, shifts):
    # the cell emits at max(hint, operand produced widths), operands
    # receiving the same hint (genrtlil.cc lines 1955-2023)
    if k in ("and", "or", "xor", "xnor", "add", "sub"):
        w = max(arm_w(node[1], hint, widths), arm_w(node[2], hint, widths))
    else:
        w = arm_w(node[1], hint, widths)
    return max(w, hint) if hint > 0 else w


def eval_node(node, env, pes=False):
    k = node[0]
    if k == "zx":
        return zx(eval_node(node[1], env, pes), node[2])
    if k == "leaf":
        return env[node[1]]
    if k == "const":
        return node[1]
    if k == "not":
        return op_not(zx(eval_node(node[1], env, pes), node[2]))
    if k in ("and", "or", "xor", "xnor"):
        a = zx(eval_node(node[1], env, pes), node[3])
        b = zx(eval_node(node[2], env, pes), node[3])
        if k == "and":
            return op_and(a, b)
        if k == "or":
            return op_or(a, b)
        if k == "xor":
            return op_xor(a, b)
        return op_not(op_xor(a, b))
    if k in ("add", "sub"):
        return op_addsub(zx(eval_node(node[1], env, pes), node[3]),
                         zx(eval_node(node[2], env, pes), node[3]),
                         k == "sub", node[3])
    if k == "neg":
        return op_neg(zx(eval_node(node[1], env, pes), node[2]), node[2])
    if k in ("shl", "shr"):
        return op_shift(eval_node(node[1], env, pes),
                        eval_node(node[2], env, pes),
                        node[3], k == "shl")
    if k == "mux":
        if node[2] == node[3]:
            # identical arms: genrtlil emits the $mux cell but proc's
            # bundled opt_expr -keepdc folds A==B muxes before sim sees
            # them (x-safe under LRM merge -- x?a:a = a -- but NOT under
            # pessimism, so the reference must model the fold to match
            # the actual pipeline in both modes)
            return zx(eval_node(node[2], env, pes), node[4])
        return op_mux(zx(eval_node(node[1], env, pes), 1),
                      zx(eval_node(node[2], env, pes), node[4]),
                      zx(eval_node(node[3], env, pes), node[4]), pes,
                      poison_w=node[4])
    if k in ("eq", "ne"):
        a = eval_node(node[1], env, pes)
        b = eval_node(node[2], env, pes)
        w = max(len(a), len(b))
        return op_eq(zx(a, w), zx(b, w), k == "ne")
    if k in ("lt", "le"):
        a = eval_node(node[1], env, pes)
        b = eval_node(node[2], env, pes)
        w = max(len(a), len(b))
        return op_lt(zx(a, w), zx(b, w), k == "le")
    if k == "reduce":
        return op_reduce(eval_node(node[1], env, pes), node[2])
    if k == "bool":
        # mux select: yosys ReduceBool -- fold of the per-bit or table
        # (or-fold of a single bit from S0 is the bit itself)
        return op_reduce(eval_node(node[1], env, pes), "or")
    if k == "logic_not":
        return op_logic_not(eval_node(node[1], env, pes))
    if k == "concat":
        return eval_node(node[1], env, pes) + eval_node(node[2], env, pes)
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
        self.widths = {name: w for name, w in self.regs}
        for s in self.inputs:
            self.widths[s] = 1
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
            return (op, ctxify(a, w, self.widths), ctxify(b, w, self.widths), w), w
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
            w = max(arm_w(a, -1, self.widths), arm_w(b, -1, self.widths))
            return ("mux", ("bool", s), ctxify(a, w, self.widths),
                    ctxify(b, w, self.widths), w), w
        if roll < 0.81:
            a, wa = self.self_expr(d - 1)
            b, wb = self.self_expr(d - 1)
            w = max(wa, wb)
            # comparison operands are evaluated at max(self widths):
            # extend-then-operate INSIDE the operand subtree (yosys
            # detectSignWidthWorker + genRTLIL(width_hint); e.g.
            # 2'b00 != ~1'b1 is TRUE because ~ evaluates at 2 bits -> 2'b10)
            return (self.r.choice(["eq", "ne", "lt", "le"]),
                    ctxify(a, w, self.widths), ctxify(b, w, self.widths)), 1
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
            return (op, ctxify(a, w, self.widths), ctxify(b, w, self.widths), w), w
        if roll < 0.44:
            a, wa = self.expr_w(d - 1, tw)
            w = max(tw, wa)
            return ("not", ctxify(a, w, self.widths), w), w
        if roll < 0.52:
            a, wa = self.expr_w(d - 1, tw)
            w = max(tw, wa)
            return ("neg", ctxify(a, w, self.widths), w), w
        if roll < 0.62:
            # shift: left operand is context-determined (genrtlil passes
            # width_hint into the left subtree, verified by $shl probes);
            # only the amount is self-determined; the shift computes at
            # max(context, left width)
            a, wa = self.expr_w(d - 1, tw)
            b, wb = self.self_expr(d - 1)
            w = max(tw, wa)
            return (self.r.choice(["shl", "shr"]), ctxify(a, w, self.widths), b, w), w
        if roll < 0.70:
            # select is self-determined (own width, then reduce_bool);
            # the ternary CELL width is max of the arm produced widths
            # under this hint (arm_w) and the node records it; the
            # result is extended to tw at the connect, so the produced
            # width is max(cell, tw) (genrtlil.cc AST_TERNARY)
            s, _ = self.self_expr(d - 1)
            a, wa = self.expr_w(d - 1, tw)
            b, wb = self.expr_w(d - 1, tw)
            w = max(arm_w(a, tw, self.widths), arm_w(b, tw, self.widths))
            return ("mux", ("bool", s), ctxify(a, w, self.widths),
                    ctxify(b, w, self.widths), w), max(w, tw)
        if roll < 0.80:
            a, wa = self.self_expr(d - 1)
            b, wb = self.self_expr(d - 1)
            w = max(wa, wb)
            return (self.r.choice(["eq", "ne", "lt", "le"]),
                    ctxify(a, w, self.widths), ctxify(b, w, self.widths)), 1
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


def sim_vcd(svf, ywf, vcdf, mode, techmap=False):
    """Run yosys sim on svf with witness ywf, return parsed posedge slots or
    (class, detail) on infrastructure failure."""
    xprop = [] if mode == "merge" else ["-xprop", mode]
    prep = "proc; techmap" if techmap else "proc"
    script = ("read_verilog -sv %s; %s; sim %s -r %s -vcd %s"
              % (svf, prep, " ".join(xprop), ywf, vcdf))
    p = subprocess.run([YOSYS, "-p", script], capture_output=True, text=True,
                       timeout=60)
    if p.returncode != 0 or not os.path.exists(vcdf):
        return ("infra", (p.stderr + p.stdout)[-300:])
    return parse_vcd(vcdf)


def check_gate_containment(proc_slots, gate_slots, nsteps, names):
    """RTL-vs-gate drift check.  Soundness invariant: wherever the
    coarse-cell (proc) sim is definite, the techmapped gate netlist must
    agree definite.  proc x -> gate may be x OR definite (techmap is
    structurally less pessimistic: per-bit gates beat big2const, ripple
    carries contaminate upward only, barrel-shifter mux trees merge
    over a bounded set of positions).  definite-vs-definite mismatch or
    gate x where proc is definite is a real soundness finding."""
    viol = []
    drift = 0
    # slot 0: the dirty scheduler can leave either side's outputs
    # unevaluated; the interesting legs start at slot 1
    for k in range(1, min(nsteps, len(proc_slots), len(gate_slots))):
        for name in names:
            a = proc_slots[k].get(name)
            b = gate_slots[k].get(name)
            if a is None or b is None:
                continue
            for i in range(len(a)):
                ab, bb = a[i], b[i]
                if ab in "01" and bb in "01" and ab != bb:
                    viol.append((k, name, a, b))
                    break
                if ab in "01" and bb == "x":
                    viol.append((k, name, a, b))
                    break
                if ab == "x" and bb in "01":
                    drift += 1
    return viol, drift


def run_case(seed, index, workdir, mode="merge", techmap=False):
    rng = random.Random("x%d:%d" % (seed, index))
    g = Gen(rng)
    sv, outs, dnets = g.case()
    stim, nsteps = make_stim(rng, g.n_in)
    svf = os.path.join(workdir, "case.sv")
    ywf = os.path.join(workdir, "stim.yw")
    vcdf = os.path.join(workdir, "out.vcd")
    open(svf, "w").write(sv)
    open(ywf, "w").write(stim)
    # NB: no flatten here. proc runs opt_expr -keepdc, which preserves
    # X-semantics (same-operand folds like x^x -> 0 are 2-state-isms
    # gated behind -keepdc / fixed to be x-aware); proc alone normalizes
    # the design for sim
    slots = sim_vcd(svf, ywf, vcdf, mode)
    if isinstance(slots, tuple):
        return slots
    if len(slots) < nsteps:
        return ("infra", "only %d posedge slots (%d steps)"
                % (len(slots), nsteps))
    drift = 0
    if techmap:
        gvcdf = os.path.join(workdir, "out_gate.vcd")
        gslots = sim_vcd(svf, ywf, gvcdf, mode, techmap=True)
        if isinstance(gslots, tuple):
            return gslots
        viol, drift = check_gate_containment(slots, gslots, nsteps,
                                             ["o%d" % i
                                              for i in range(g.n_out)])
        if viol:
            return ("gate-violation", viol)
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
            ref = {"o%d" % i: zx(eval_node(e, env, pes=(mode == "pessimistic")), 6)
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
    return ("agree", drift)


GOLDEN_SV = """\
module top(input clk, input a, input b);
  reg sel;                          // uninitialized -> x until first edge
  wire o = sel ? a : b;             // arms equal at time 0 (a=b=1)
  always @(posedge clk) sel <= a;
endmodule
"""
GOLDEN_STIM = """\
{"format": "Yosys Witness Trace", "generator": "golden",
 "clocks": [{"path": ["\\\\clk"], "edge": "posedge", "offset": 0}],
 "signals": [{"path": ["\\\\a"], "width": 1, "offset": 0, "init_only": false},
             {"path": ["\\\\b"], "width": 1, "offset": 0, "init_only": false}],
 "steps": [{"bits": "11"}, {"bits": "10"}, {"bits": "01"}]}
"""


def run_golden(workdir):
    """Deterministic pin of the merge-vs-pessimistic divergence: with the
    select x and both arms 1, LRM merge yields 1 and pessimistic yields
    x.  Also pins the RTL->gate containment on the same design."""
    svf = os.path.join(workdir, "golden.sv")
    ywf = os.path.join(workdir, "golden.yw")
    open(svf, "w").write(GOLDEN_SV)
    open(ywf, "w").write(GOLDEN_STIM)
    fails = []
    for mode, want in (("merge", "1"), ("pessimistic", "x")):
        vcdf = os.path.join(workdir, "golden_%s.vcd" % mode)
        slots = sim_vcd(svf, ywf, vcdf, mode)
        if isinstance(slots, tuple):
            fails.append("golden %s: %s" % (mode, slots[1]))
            continue
        # slots[0] is the t=0 pre-edge slot: sel is still x there and the
        # witness drives a=b=1 at step 0 -> arms equal -> merge=1, pes=x
        got = slots[0].get("o") if slots else None
        if got != want:
            fails.append("golden %s: slot0 o=%s want %s" % (mode, got, want))
    gvcdf = os.path.join(workdir, "golden_gate.vcd")
    gslots = sim_vcd(svf, ywf, gvcdf, "merge", techmap=True)
    pslots = sim_vcd(svf, ywf, os.path.join(workdir, "golden_rtl.vcd"),
                     "merge")
    if isinstance(gslots, tuple) or isinstance(pslots, tuple):
        fails.append("golden techmap leg: infra failure")
    else:
        viol, _ = check_gate_containment(pslots, gslots, 3, ["o"])
        if viol:
            fails.append("golden techmap containment violated: %r" % viol[:2])
    if fails:
        for f in fails:
            print("golden: %s" % f)
        return False
    print("golden: merge slot0 o=1, pessimistic slot0 o=x, gate containment ok")
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--count", type=int, default=100)
    ap.add_argument("--out", default=None)
    ap.add_argument("--modes", default="merge",
                    help="comma list: merge, pessimistic")
    ap.add_argument("--techmap", action="store_true",
                    help="also run the proc-vs-techmap gate-level "
                         "containment leg per case")
    ap.add_argument("--golden", action="store_true",
                    help="run the deterministic mux-x divergence pin "
                         "(no fuzz cases)")
    args = ap.parse_args()

    outdir = args.out or tempfile.mkdtemp(prefix="xprop_")
    os.makedirs(outdir, exist_ok=True)
    if args.golden:
        sys.exit(0 if run_golden(outdir) else 1)

    modes = [m.strip() for m in args.modes.split(",") if m.strip()]
    ok = 0
    bad = []
    total = 0
    drift_total = 0
    for index in range(args.count):
        for mode in modes:
            total += 1
            workdir = os.path.join(outdir, "case_%04d_%s" % (index, mode))
            os.makedirs(workdir, exist_ok=True)
            cls, detail = run_case(args.seed, index, workdir, mode,
                                   techmap=args.techmap)
            if cls == "agree":
                ok += 1
                drift_total += detail
            else:
                bad.append((index, mode, cls, detail))
        if (index + 1) % 25 == 0:
            print("[%d/%d] ok=%d bad=%d" % (index + 1, args.count, ok,
                                            len(bad)), flush=True)
    print("summary: %d/%d agreement, %d findings, %d expected-drift bits"
          % (ok, total, len(bad), drift_total))
    for index, mode, cls, detail in bad[:10]:
        print("case %d (%s): %s" % (index, mode, cls))
        if cls in ("mismatch", "gate-violation"):
            for m in detail[:6]:
                print("   step %d %s ref=%s sim=%s" % m)
        else:
            print("   %s" % detail)
    sys.exit(0 if not bad else 1)


if __name__ == "__main__":
    main()
