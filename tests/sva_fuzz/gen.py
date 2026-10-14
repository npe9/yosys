#!/usr/bin/env python3
r"""Seeded random SVA property/module generator for the sva_fuzz oracle.

Generates small LRM-legal SystemVerilog modules with exactly one
`assert property` (`p0`, `@(posedge clk)`, optional `disable iff (rst)`,
no assumes), plus the random per-step input stimulus (.yw) the sim leg
replays.

Every construct emitted here was probe-verified against the in-tree
frontend + sva2check (see tests/sva_fuzz/README.md).  Known lowering gaps
that the LRM allows (e.g. sequence-level `and`) are emitted at low rate on
purpose: a frontend/sva2check reject of genuinely legal syntax is a
first-class result (bug class C), not a generator bug.

CLI:
    python3 gen.py --seed N --count M --out DIR [--bound B]

Programmatic:
    import gen
    sv_text, stim_text, meta = gen.build_case(seed, index, bound)

Fully deterministic per (seed, index).
"""

import argparse
import json
import os
import random

MAX_DEPTH = 5          # sequence/property nesting cap (mission: <= 5)
NODE_BUDGET = 26       # total grammar-node cap per property (smtbmc speed)


class CaseGen:
    def __init__(self, rng):
        self.r = rng
        self.inputs = ["a", "b", "c"] + (["d"] if rng.random() < 0.35 else [])
        self.has_cnt = rng.random() < 0.75
        self.cnt_w = rng.choice([2, 2, 2, 3])
        self.sigs = list(self.inputs)
        if self.has_cnt:
            self.sigs += ["cnt[%d]" % i for i in range(self.cnt_w)]
        self.nodes = 0
        self.cnt_gated = self.has_cnt and rng.random() < 0.15
        # free input driving `disable iff (rst)` (never used in the
        # property body itself)
        self.has_rst = rng.random() < 0.30
        # free input driving property `case` selectors (1- or 2-bit,
        # never referenced outside the case selector)
        self.sel_w = self.r.choice([0, 0, 1, 2])
        # property declarations emitted before p0 (declared-property
        # instances with local variables / formals)
        self.decls = []

    def vexpr(self, v, w, d=1):
        """boolean expression over a w-bit local v and module inputs"""
        def wconst():
            return "%d'd%d" % (w, self.r.randint(0, (1 << w) - 1))
        def wexp():
            if w == 1:
                return self.r.choice(self.inputs)
            ins = [self.r.choice(self.inputs) for _ in range(w)]
            return "{" + ", ".join(ins) + "}"
        if d <= 0 or self.r.random() < 0.5:
            roll = self.r.random()
            if roll < 0.4:
                return "(%s == %s)" % (v, wconst())
            if roll < 0.8:
                return "(%s == %s)" % (v, wexp())
            return "(%s[%d] == 1'b%d)" % (v, self.r.randint(0, w - 1),
                                         self.r.randint(0, 1))
        op = self.r.choice(["&&", "||"])
        return "(%s %s %s)" % (self.vexpr(v, w, d - 1), op,
                               self.vexpr(v, w, d - 1))

    def declared_p0(self):
        """With ~30% probability build p0 as an instance of a declared
        property exercising the local-variable slots machinery (60%) or
        formal-argument instance binding (40%).  Returns the p0 assert
        line, or None for the normal inline body."""
        if self.r.random() > 0.30:
            return None
        if self.r.random() < 0.6:
            # local variable: capture at a bounded antecedent end, check
            # in the consequent.  sva2check rejects locals over unbounded
            # sequences, so the antecedent stays shallow (atoms/## only).
            w = self.r.choice([1, 2, 3])
            v = "v0"
            ins = self.r.sample(self.inputs, min(w, len(self.inputs)))
            while len(ins) < w:
                ins.append(self.r.choice(self.inputs))
            cap = ins[0] if w == 1 else "{" + ", ".join(ins[:w]) + "}"
            ant = self.seq(self.r.randint(0, 1), allow_eq=False)
            impl = "|->" if self.r.random() < 0.3 else "|=>"
            con = self.vexpr(v, w)
            self.decls.append(
                "  property pd0;\n"
                "    logic [%d:0] %s;\n"
                "    @(posedge clk) (%s, %s = %s) %s (%s);\n"
                "  endproperty" % (w - 1, v, ant, v, cap, impl, con))
            return "p0: assert property (@(posedge clk) pd0);"
        # formals: instance actuals bind to module inputs; the body
        # references the formal parameters.  Untyped formal list (the
        # parser rejects the SV-2012 `input logic` prefix).
        nf = self.r.randint(1, 2)
        formals = ["f%d" % i for i in range(nf)]
        args = [self.r.choice(self.inputs) for _ in range(nf)]
        ant = "(%s ##%d %s)" % (self.r.choice(formals),
                                self.r.randint(0, 2), self.atom())
        con = "(%s %s %s)" % (self.r.choice(formals),
                              self.r.choice(["&&", "||"]),
                              self.bexpr(0))
        self.decls.append(
            "  property pd0(%s);\n"
            "    @(posedge clk) %s |-> %s;\n"
            "  endproperty" % (", ".join(formals), ant, con))
        return "p0: assert property (@(posedge clk) pd0(%s));" % ", ".join(args)

    def stim_signals(self):
        """(name, width) pairs the .yw stimulus drives, in port order."""
        sigs = [(s, 1) for s in self.inputs]
        if self.has_rst:
            sigs.append(("rst", 1))
        if self.sel_w:
            sigs.append(("sel", self.sel_w))
        return sigs

    # -- boolean expressions ------------------------------------------------

    def atom(self):
        """Plain boolean atom; occasionally wrapped in a sampled-value fn."""
        if self.r.random() < 0.03:
            return self.r.choice(["1'b0", "1'b1"])
        sig = self.r.choice(self.sigs)
        roll = self.r.random()
        if roll < 0.84:
            return sig
        if roll < 0.87:
            return "$rose(%s)" % sig
        if roll < 0.90:
            return "$fell(%s)" % sig
        if roll < 0.925:
            return "$stable(%s)" % sig
        if roll < 0.95:
            return "$past(%s,1)" % sig
        if roll < 0.97:
            return "$past(%s,2)" % sig
        # $stable/$changed of a small parenthesized boolean (probed);
        # the inner expr is plain signals only, no nested sampled fns
        s2 = self.r.choice(self.sigs)
        op = self.r.choice(["&&", "||"])
        fn = "stable" if roll < 0.985 else "changed"
        return "$%s((%s %s %s))" % (fn, sig, op, s2)

    def bexpr(self, d, plain=False):
        # plain: operand of `not`, where sva2check treats ->/<-> as temporal
        # property operators and rejects the negation ("not of a temporal
        # property is not a safety property").
        self.nodes += 1
        if d <= 0 or self.nodes > NODE_BUDGET:
            return self.atom()
        roll = self.r.random()
        if roll < 0.45:
            return self.atom()
        if roll < 0.55:
            return "(!%s)" % self.bexpr(d - 1, plain)
        ops = ["&&", "||"] if plain else ["&&", "||", "->", "<->"]
        op = self.r.choice(ops)
        return "(%s %s %s)" % (self.bexpr(d - 1, plain), op,
                               self.bexpr(d - 1, plain))

    # -- sequences -----------------------------------------------------------

    def rep(self, atom, allow_eq, no_single=False):
        roll = self.r.random()
        if allow_eq:
            # Antecedent context: exercise repetitions over small bounded
            # sequences heavily ([*]/[->]/[=] all accept a ## concat body;
            # sva2check rejects them only in property/consequent position,
            # probed).  sva2check accepts it anywhere inside an antecedent
            # sequence.
            if roll < 0.30:
                return self.star(atom, single=True)
            if roll < 0.55:
                return self.star(atom, single=False)
            return self.noncontig(atom)
        if roll < 0.5 and not no_single:
            return "%s[*%d]" % (atom, self.r.randint(1, 4))
        lo = self.r.randint(1, 3)
        return "%s[*%d:%d]" % (atom, lo, lo + self.r.randint(1, 4 - lo))

    def seq_body(self, atom):
        # small bounded sequence body for repetition operands
        if self.r.random() < 0.40:
            return "(%s ##%d %s)" % (self.atom(), self.r.randint(1, 2),
                                     self.atom())
        return atom

    def star(self, atom, single):
        body = self.seq_body(atom)
        if single:
            return "%s[*%d]" % (body, self.r.randint(1, 3))
        lo = self.r.randint(1, 2)
        return "%s[*%d:%d]" % (body, lo, lo + self.r.randint(1, 3 - lo))

    def noncontig(self, atom):
        # [->N]/[=N] and range forms; the operand is usually a plain atom,
        # sometimes a small bounded sequence -- exercises the seq_end-based
        # occurrence-counting lowering (battery tnoncons1).
        body = self.seq_body(atom)
        op = self.r.choice(["[->", "[=", "[->", "[="])
        if self.r.random() < 0.25:
            lo = self.r.randint(1, 2)
            return "%s%s%d:%d]" % (body, op, lo, lo + self.r.randint(1, 2))
        return "%s%s%d]" % (body, op, self.r.randint(1, 3))

    def seq(self, d, allow_eq=False, strict_win=False, plain=False,
            no_single_rep=False):
        # strict_win: inside a within/throughout RHS, where sva2check's
        # window accounting rejects or/and/intersect wherever they appear
        # below, and a bare top-level repetition reads as unbounded (a rep
        # nested under ## is fine) -- keep only the statically-bounded
        # constructs there.
        # plain: operand of `not`, which additionally rejects or/and and
        # any repetition below it ("not of a temporal property").
        # no_single_rep: sequence used in property position (always body,
        # implication consequent, strong/s_always operand), where
        # sva2check reads a single-bound [*N] anywhere below as unbounded;
        # range reps and [=N] stay admissible.
        self.nodes += 1
        if d <= 0 or self.nodes > NODE_BUDGET:
            return self.atom()
        roll = self.r.random()
        if roll < 0.28:
            return self.atom()
        if roll < 0.56:
            return "(%s ##%d %s)" % (
                self.seq(d - 1, allow_eq, strict_win, plain, no_single_rep),
                self.r.randint(1, 3),
                self.seq(d - 1, allow_eq, strict_win, plain, no_single_rep))
        if roll < 0.61 and not strict_win and not plain:
            return "(%s or %s)" % (self.seq(d - 1, no_single_rep=no_single_rep),
                                   self.seq(d - 1, no_single_rep=no_single_rep))
        if roll < 0.615 and not strict_win and not plain:
            # sequence `and` is LRM-legal but a known sva2check gap: emit
            # rarely so the failure corpus registers it without flooding.
            return "(%s and %s)" % (self.seq(d - 1, no_single_rep=no_single_rep),
                                    self.seq(d - 1, no_single_rep=no_single_rep))
        if roll < 0.74 and not strict_win and not plain:
            return "(%s intersect %s)" % (
                self.seq(d - 1, no_single_rep=no_single_rep),
                self.seq(d - 1, no_single_rep=no_single_rep))
        if roll < 0.78 and not strict_win and not plain:
            return "(%s within %s)" % (self.seq(d - 1, no_single_rep=no_single_rep),
                                       self.seq(d - 1, strict_win=True,
                                                no_single_rep=no_single_rep))
        if roll < 0.84:
            return "(%s throughout (%s))" % (
                self.bexpr(d - 1),
                self.seq(d - 1, strict_win=True, no_single_rep=no_single_rep))
        if roll < 0.94:
            if strict_win or plain:
                # bare rep reads as unbounded in these two contexts
                return self.atom()
            # [*]/[+] single-bound still apply to a plain atom only
            # (sva2check rejects repetition of a general sequence there);
            # [->]/[=] operands may be small bounded sequences via rep().
            return self.rep(self.atom(), allow_eq, no_single_rep)
        return "first_match(%s)" % self.seq(
            d - 1, strict_win=strict_win, plain=plain,
            no_single_rep=no_single_rep)

    # -- properties ----------------------------------------------------------

    def prop(self, d, operand=False):
        # operand=True: position directly under always/not/s_always where
        # this parser rejects strong/weak property_expr corners (nexttime,
        # abort and case are fine there parenthesized -- probed).
        self.nodes += 1
        if d <= 0 or self.nodes > NODE_BUDGET:
            return self.bexpr(0)
        roll = self.r.random()
        if roll < 0.44:
            op = "|->" if self.r.random() < 0.7 else "|=>"
            # [=N] is only admissible as the direct antecedent of |->/|=>
            return "(%s %s %s)" % (self.seq(d - 1, allow_eq=True), op,
                                     self.conseq(d - 1))
        if roll < 0.54:
            # `not` of a boolean or a plain sequence is safety; sva2check
            # rejects `not` of temporal operators (->/<-> below, sequence
            # or/and, repetitions, implications), so keep the operand
            # plain.
            opnd = self.bexpr(d - 1, plain=True) if self.r.random() < 0.5 \
                else self.seq(d - 1, plain=True)
            return "(not %s)" % opnd
        if roll < 0.575 and not operand:
            lo = self.r.randint(1, 2)
            return "s_always[%d:%d] (%s)" % (lo, lo + self.r.randint(0, 2),
                                             self.prop_or_seq(d - 1))
        if roll < 0.61 and not operand:
            kw = "strong" if self.r.random() < 0.7 else "weak"
            return "%s (%s)" % (kw, self.seq(d - 1, no_single_rep=True))
        if roll < 0.66:
            return self.nexttime(d)
        if roll < 0.71:
            # abort operators (LRM 16.12.2): nesting them is explicitly
            # legal; the condition stays a sampled-fn-free boolean.
            kw = self.r.choice(["accept_on", "reject_on", "sync_accept_on",
                                "sync_reject_on"])
            if self.r.random() < 0.5:
                cond = self.r.choice(self.inputs)
            else:
                cond = "(%s %s %s)" % (self.r.choice(self.inputs),
                                       self.r.choice(["&&", "||"]),
                                       self.r.choice(self.inputs))
            return "(%s (%s) %s)" % (kw, cond, self.prop(d - 1))
        if roll < 0.75 and self.sel_w:
            # property case (LRM 16.12.5): every item needs a trailing
            # semicolon; values cover the selector space plus a default.
            nvals = self.sel_w + 1
            items = ["%d: %s;" % (v, self.prop(d - 1))
                     for v in range(nvals)]
            items.append("default: %s;" % self.prop(d - 1))
            body = "case (sel) %s endcase" % " ".join(items)
            # parenthesized case parses in operand position too (probed)
            return "(%s)" % body if operand else body
        return self.seq(d, no_single_rep=True)

    def nexttime(self, d):
        # nexttime/s_nexttime [k] (LRM 16.12.3).  The parser takes a
        # sequence-level operand (sva_not), so an implication/boolean/
        # nested-nexttime operand lowers and a raw sequence child hits a
        # clean sva2check reject (prop_fail re-dispatches the child
        # without prop_dispatch's sequence route, surfacing the child's
        # top sequence operator as "unsupported property operator").
        # The sequence flavor is LRM-legal (a sequence is a
        # property_expr) so it is kept at low rate to register the gap;
        # it is pinned to `ATOM ##N ATOM` so every hit dedupes to the
        # single '##' signature (one root cause).
        kw = "nexttime" if self.r.random() < 0.6 else "s_nexttime"
        if self.r.random() < 0.06:
            return "(%s (%s ##%d %s))" % (kw, self.atom(),
                                          self.r.randint(1, 3), self.atom())
        k = self.r.randint(1, 2)
        idx = "[%d] " % k if k > 1 else ""
        pick = self.r.random()
        if pick < 0.40:
            opnd = self.bexpr(d - 1, plain=True)
        elif pick < 0.60:
            opnd = "(not %s)" % self.bexpr(d - 1, plain=True)
        elif pick < 0.85 and d > 1:
            opnd = "(%s |-> %s)" % (self.seq(d - 2, allow_eq=True),
                                    self.conseq(d - 2))
        elif d > 1:
            opnd = self.nexttime(d - 1)
        else:
            opnd = self.bexpr(d - 1, plain=True)
        return "%s %s%s" % (kw, idx, opnd)

    def prop_or_seq(self, d):
        # operand contexts that accept either a property or a sequence
        # (s_always / strong / weak bodies): suppress further strong/weak
        # corners the parser rejects in operand position.
        return self.prop(d, operand=True) if self.r.random() < 0.5 \
            else self.seq(d, no_single_rep=True)

    def conseq(self, d):
        # `if (b) p [else q]` is only emitted as the direct consequent of
        # |->/|=> or as the top-level body (generate()); bare `always if`
        # is a syntax error (parenthesized `always (if ...)` parses, but
        # the top level emits the bare `if` form instead).
        roll = self.r.random()
        if roll < 0.09:
            return "(if (%s) %s else %s)" % (self.bexpr(1), self.prop(d - 1),
                                             self.prop(d - 1))
        if roll < 0.12:
            return "(if (%s) %s)" % (self.bexpr(1), self.prop(d - 1))
        return self.prop(d)

    # -- whole case ----------------------------------------------------------

    def dut(self, body):
        ports = ["input clk"] + ["input %s" % s for s in self.inputs]
        if self.has_rst:
            ports.append("input rst")
        if self.sel_w:
            ports.append("input sel" if self.sel_w == 1 else
                         "input [%d:0] sel" % (self.sel_w - 1))
        lines = ["module top(%s);" % ", ".join(ports)]
        if self.has_cnt:
            lines.append("  logic [%d:0] cnt;" % (self.cnt_w - 1))
            if self.cnt_gated:
                lines.append("  always_ff @(posedge clk)")
                lines.append("    if (a) cnt <= cnt + 1;")
            else:
                lines.append("  always_ff @(posedge clk) cnt <= cnt + 1;")
        spec = "@(posedge clk) "
        if self.has_rst:
            spec += "disable iff (rst) "
        lines.extend(self.decls)
        if self.p0_line is not None:
            # p0 is an instance of a declared property (locals/formals):
            # the spec lives inside the declaration (battery tslot style)
            lines.append("  " + self.p0_line)
        else:
            lines.append("  p0: assert property (%s%s);" % (spec, body))
        lines.append("endmodule")
        return "\n".join(lines) + "\n"

    def generate(self):
        # `always if ...` / `always case ...` are parser syntax errors, so
        # decide the wrapper first and generate the body in operand
        # position when wrapped.  A property-level `if` can also head the
        # bare body (no always); `always (if ...)` would parse but the
        # bare form is the verified shape.
        self.p0_line = self.declared_p0()
        use_always = self.p0_line is None and self.r.random() < 0.70
        if not use_always and self.r.random() < 0.10:
            cond = self.bexpr(1)
            if self.r.random() < 0.75:
                body = "(if (%s) %s else %s)" % (
                    cond, self.prop(MAX_DEPTH - 1), self.prop(MAX_DEPTH - 1))
            else:
                body = "(if (%s) %s)" % (cond, self.prop(MAX_DEPTH - 1))
        else:
            body = self.prop(MAX_DEPTH, operand=use_always)
            if use_always:
                body = "always " + body
        return self.dut(body)


def make_stim(rng, sigs, bound):
    """Random per-step input stimulus as a Yosys witness text.

    Exactly `bound` steps (indices 0..bound-1, matching the steps smtbmc
    checks under `-t bound`); index 0 is the initial-settle slot.  `sigs`
    is a list of (name, width) pairs in port order; within a step's bit
    string the signals appear in reverse order and each signal's bit 0
    sits at the highest index of its own field (kernel/yw.cc get_bits
    convention).  B+1 steps here would let the sim leg observe the
    assertion one step past the formal leg's bound and manufacture false
    class-A findings at the top step.
    """
    steps = []
    for _ in range(bound):
        bits = []
        for name, w in reversed(sigs):
            for _b in range(w):
                bits.append("1" if rng.random() < 0.5 else "0")
        steps.append({"bits": "".join(bits)})
    yw = {
        "format": "Yosys Witness Trace",
        "generator": "sva-fuzz",
        "clocks": [{"path": ["\\clk"], "edge": "posedge", "offset": 0}],
        "signals": [{"path": ["\\" + n], "width": w, "offset": 0,
                     "init_only": False} for n, w in sigs],
        "steps": steps,
    }
    return json.dumps(yw, indent=1) + "\n"


def build_case(seed, index, bound=12):
    """-> (sv_text, stim_yw_text, meta) for case `index` of batch `seed`."""
    rng = random.Random("%d:%d" % (seed, index))
    g = CaseGen(rng)
    sv = "// sva_fuzz case seed=%d index=%d\n" % (seed, index) + g.generate()
    stim = make_stim(rng, g.stim_signals(), bound)
    meta = {"seed": seed, "index": index, "bound": bound,
            "inputs": g.inputs, "has_cnt": g.has_cnt,
            "has_rst": g.has_rst, "sel_w": g.sel_w}
    return sv, stim, meta


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--count", type=int, required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--bound", type=int, default=12)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    manifest = []
    for i in range(args.count):
        sv, stim, meta = build_case(args.seed, i, args.bound)
        base = os.path.join(args.out, "case_%04d" % i)
        with open(base + ".sv", "w") as fh:
            fh.write(sv)
        with open(base + ".yw", "w") as fh:
            fh.write(stim)
        manifest.append(meta)
    with open(os.path.join(args.out, "manifest.json"), "w") as fh:
        json.dump(manifest, fh, indent=1)
    print("wrote %d cases to %s (seed %d)" % (args.count, args.out, args.seed))


if __name__ == "__main__":
    main()
