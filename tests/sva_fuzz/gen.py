#!/usr/bin/env python3
r"""Seeded random SVA property/module generator for the sva_fuzz oracle.

Generates small LRM-legal SystemVerilog modules with exactly one
`assert property` (`p0`, `@(posedge clk)`, no disable iff, no assumes),
plus the random per-step input stimulus (.yw) the sim leg replays.

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

    # -- boolean expressions ------------------------------------------------

    def atom(self):
        """Plain boolean atom; occasionally wrapped in a sampled-value fn."""
        if self.r.random() < 0.03:
            return self.r.choice(["1'b0", "1'b1"])
        sig = self.r.choice(self.sigs)
        roll = self.r.random()
        if roll < 0.88:
            return sig
        if roll < 0.92:
            return "$rose(%s)" % sig
        if roll < 0.95:
            return "$fell(%s)" % sig
        if roll < 0.975:
            return "$stable(%s)" % sig
        return "$past(%s,1)" % sig

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
        if roll < 0.5 and not no_single:
            return "%s[*%d]" % (atom, self.r.randint(1, 4))
        if roll < 0.85 or not allow_eq:
            lo = self.r.randint(1, 3)
            return "%s[*%d:%d]" % (atom, lo, lo + self.r.randint(1, 4 - lo))
        # [=N] has an unbounded lookback window: sva2check only accepts it
        # as the direct antecedent of |->/|=> (battery t7 usage); anywhere
        # else it errors "no finite bound", so it is gated by allow_eq.
        return "%s[=%d]" % (atom, self.r.randint(1, 4))

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
            # repetitions apply to a plain atom only: sva2check rejects
            # repetition of a general sequence ([*N] single-bound form
            # probes as unbounded there), so keep this conservative.
            return self.rep(self.atom(), allow_eq, no_single_rep)
        return "first_match(%s)" % self.seq(
            d - 1, strict_win=strict_win, plain=plain,
            no_single_rep=no_single_rep)

    # -- properties ----------------------------------------------------------

    def prop(self, d, operand=False):
        # operand=True: position directly under always/not/s_always where
        # this parser rejects strong/weak property_expr corners.
        self.nodes += 1
        if d <= 0 or self.nodes > NODE_BUDGET:
            return self.bexpr(0)
        roll = self.r.random()
        if roll < 0.48:
            op = "|->" if self.r.random() < 0.7 else "|=>"
            # [=N] is only admissible as the direct antecedent of |->/|=>
            return "(%s %s %s)" % (self.seq(d - 1, allow_eq=True), op,
                                     self.conseq(d - 1))
        if roll < 0.58:
            # `not` of a boolean or a plain sequence is safety; sva2check
            # rejects `not` of temporal operators (->/<-> below, sequence
            # or/and, repetitions, implications), so keep the operand
            # plain.
            opnd = self.bexpr(d - 1, plain=True) if self.r.random() < 0.5 \
                else self.seq(d - 1, plain=True)
            return "(not %s)" % opnd
        if roll < 0.62 and not operand:
            lo = self.r.randint(1, 2)
            return "s_always[%d:%d] (%s)" % (lo, lo + self.r.randint(0, 2),
                                             self.prop_or_seq(d - 1))
        if roll < 0.66 and not operand:
            kw = "strong" if self.r.random() < 0.7 else "weak"
            return "%s (%s)" % (kw, self.seq(d - 1, no_single_rep=True))
        return self.seq(d, no_single_rep=True)

    def prop_or_seq(self, d):
        # operand contexts that accept either a property or a sequence
        # (s_always / strong / weak bodies): suppress further strong/weak
        # corners the parser rejects in operand position.
        return self.prop(d, operand=True) if self.r.random() < 0.5 \
            else self.seq(d, no_single_rep=True)

    def conseq(self, d):
        # `if (b) p else q` is only emitted as the direct consequent of
        # |->/|=> (verified shape); `always if ...` is a syntax error and
        # `if` in antecedent position is a lowering gap.
        if self.r.random() < 0.15:
            return "(if (%s) %s else %s)" % (self.bexpr(1), self.prop(d - 1),
                                             self.prop(d - 1))
        return self.prop(d)

    # -- whole case ----------------------------------------------------------

    def dut(self, body):
        ports = ", ".join(["input clk"] +
                          ["input %s" % s for s in self.inputs])
        lines = ["module top(%s);" % ports]
        if self.has_cnt:
            lines.append("  logic [%d:0] cnt;" % (self.cnt_w - 1))
            if self.cnt_gated:
                lines.append("  always_ff @(posedge clk)")
                lines.append("    if (a) cnt <= cnt + 1;")
            else:
                lines.append("  always_ff @(posedge clk) cnt <= cnt + 1;")
        lines.append("  p0: assert property (@(posedge clk) %s);" % body)
        lines.append("endmodule")
        return "\n".join(lines) + "\n"

    def generate(self):
        # `always if ...` is a parser syntax error, so decide the wrapper
        # first and generate the body in operand position when wrapped.
        # (`if` is only ever emitted as an implication consequent, so the
        # body can never start with it.)
        use_always = self.r.random() < 0.70
        body = self.prop(MAX_DEPTH, operand=use_always)
        if use_always:
            body = "always " + body
        return self.dut(body)


def make_stim(rng, inputs, bound):
    """Random per-step input stimulus as a Yosys witness text.

    Exactly `bound` steps (indices 0..bound-1, matching the steps smtbmc
    checks under `-t bound`); index 0 is the initial-settle slot.  One
    signal per data input, in the given order; within a step's bit string
    the k-th signal's LSB sits at the highest string index (kernel/yw.cc
    get_bits convention).  B+1 steps here would let the sim leg observe
    the assertion one step past the formal leg's bound and manufacture
    false class-A findings at the top step.
    """
    steps = []
    for _ in range(bound):
        bits = ["0"] * len(inputs)
        for k in range(len(inputs)):
            if rng.random() < 0.5:
                bits[len(inputs) - 1 - k] = "1"
        steps.append({"bits": "".join(bits)})
    yw = {
        "format": "Yosys Witness Trace",
        "generator": "sva-fuzz",
        "clocks": [{"path": ["\\clk"], "edge": "posedge", "offset": 0}],
        "signals": [{"path": ["\\" + s], "width": 1, "offset": 0,
                     "init_only": False} for s in inputs],
        "steps": steps,
    }
    return json.dumps(yw, indent=1) + "\n"


def build_case(seed, index, bound=12):
    """-> (sv_text, stim_yw_text, meta) for case `index` of batch `seed`."""
    rng = random.Random("%d:%d" % (seed, index))
    g = CaseGen(rng)
    sv = "// sva_fuzz case seed=%d index=%d\n" % (seed, index) + g.generate()
    stim = make_stim(rng, g.inputs, bound)
    meta = {"seed": seed, "index": index, "bound": bound,
            "inputs": g.inputs, "has_cnt": g.has_cnt}
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
