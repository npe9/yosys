#!/usr/bin/env python3
"""Random property generator, SystemVerilog printer and shrinker for the
SVA property-based tester.  Grammar == ref_sva.py's.  Deterministic per
random.Random instance.

Design shell every case uses (the reference knows these registers):
    inputs  a b c d rst   (free, driven by the stimulus witness)
    reg r      <= a          (so  r == $past(a)  is a tautology)
    reg [1:0] cnt <= cnt+1   (cnt(k) = k mod 4 under -zinit)
"""
import random
from ref_sva import N

SIGS = ["a", "b", "c", "d", "r"]

class Gen:
    def __init__(self, rng, max_depth=3, budget=18):
        self.r = rng
        self.max_depth = max_depth
        self.budget = budget
        self.nodes = 0

    def spend(self):
        self.nodes += 1
        return self.nodes > self.budget

    # ---------------------------------------------------------- booleans
    def boolean(self, d=0):
        self.spend()
        r = self.r.random()
        if d >= 2 or r < 0.45:
            return self.leaf()
        if r < 0.6:
            return N("not", self.boolean(d + 1))
        if r < 0.8:
            return N("and", self.boolean(d + 1), self.boolean(d + 1))
        return N("or", self.boolean(d + 1), self.boolean(d + 1))

    def leaf(self):
        r = self.r.random()
        if r < 0.55:
            return N("sig", self.r.choice(SIGS))
        if r < 0.65:
            return N("past", self.r.choice(SIGS[:4]), self.r.choice([1, 1, 2]))
        if r < 0.72:
            return N("rose", self.r.choice(SIGS[:4]))
        if r < 0.78:
            return N("fell", self.r.choice(SIGS[:4]))
        if r < 0.84:
            return N("stable", self.r.choice(SIGS[:4]))
        if r < 0.94:
            return N("cnteq", self.r.randint(0, 3))
        if r < 0.97:
            return N("cntbit", self.r.randint(0, 1))
        return N("const", self.r.randint(0, 1))

    # ---------------------------------------------------------- sequences
    def seq(self, d=0):
        if self.spend() or d >= self.max_depth:
            return self.boolean()
        r = self.r.random()
        if r < 0.30:
            return self.boolean()
        if r < 0.55:
            lo = self.r.choice([0, 1, 1, 2, 3])
            hi = lo if self.r.random() < 0.6 else lo + self.r.randint(1, 2)
            return N("delay", self.seq(d + 1), lo, hi, self.seq(d + 1))
        if r < 0.62:
            return N("sor", self.seq(d + 1), self.seq(d + 1))
        if r < 0.67:
            return N("sand", self.seq(d + 1), self.seq(d + 1))
        if r < 0.71:
            return N("intersect", self.seq(d + 1), self.seq(d + 1))
        if r < 0.76:
            return N("throughout", self.boolean(), self.seq(d + 1))
        if r < 0.80:
            return N("within", self.seq(d + 1), self.seq(d + 1))
        if r < 0.85:
            return N("first_match", self.seq(d + 1))
        if r < 0.92:
            lo = self.r.choice([0, 1, 1, 2, 3])
            hi = lo if self.r.random() < 0.5 else lo + self.r.randint(1, 2)
            return N("rep", self.boolean(), lo, hi)
        if r < 0.96 and d > 0:
            return N("goto", self.boolean(), self.r.randint(1, 3))
        if d > 0:
            return N("noncons", self.boolean(), self.r.randint(1, 2))
        return self.boolean()

    # ---------------------------------------------------------- properties
    def prop(self, d=0):
        if self.spend() or d >= self.max_depth:
            return self.seq(d)
        r = self.r.random()
        if r < 0.12:
            return self.seq(d)
        if r < 0.45:
            return N("impl", self.seq(d + 1), self.prop(d + 1), self.r.random() < 0.5)
        if r < 0.52:
            # `not` of a temporal property is not a safety property (the
            # framework rejects it by design); negate booleans only
            return N("pnot", self.boolean())
        if r < 0.58:
            return N("pand", self.prop(d + 1), self.prop(d + 1))
        if r < 0.64:
            return N("por", self.prop(d + 1), self.prop(d + 1))
        if r < 0.69:
            return N("implies", self.prop(d + 1), self.prop(d + 1))
        if r < 0.75:
            return N("pif", self.boolean(), self.prop(d + 1), self.prop(d + 1))
        if r < 0.82:
            return N("nexttime", self.prop(d + 1), self.r.randint(1, 2))
        if r < 0.88:
            lo = self.r.randint(0, 2); hi = lo + self.r.randint(0, 2)
            return N("always", self.prop(d + 1), lo, hi)
        if r < 0.93:
            lo = self.r.randint(0, 2); hi = lo + self.r.randint(1, 2)
            return N("s_eventually", self.prop(d + 1), lo, hi)
        return N("until", self.boolean(), self.boolean(), self.r.random() < 0.4)

    def top(self):
        dis = self.boolean() if self.r.random() < 0.25 else None
        if dis is not None and self.r.random() < 0.6:
            dis = N("sig", "rst")
        return N("top", dis, self.prop())

# ---------------------------------------------------------------- printer
def pb(b):
    k = b.k
    if k == "sig": return b.a[0]
    if k == "const": return "1'b%d" % b.a[0]
    if k == "not": return "(!%s)" % pb(b.a[0])
    if k == "and": return "(%s && %s)" % (pb(b.a[0]), pb(b.a[1]))
    if k == "or": return "(%s || %s)" % (pb(b.a[0]), pb(b.a[1]))
    if k == "past": return "$past(%s, %d)" % (b.a[0], b.a[1]) if b.a[1] != 1 else "$past(%s)" % b.a[0]
    if k == "rose": return "$rose(%s)" % b.a[0]
    if k == "fell": return "$fell(%s)" % b.a[0]
    if k == "stable": return "$stable(%s)" % b.a[0]
    if k == "cnteq": return "(cnt == 2'd%d)" % b.a[0]
    if k == "cntbit": return "cnt[%d]" % b.a[0]
    raise ValueError(k)

BOOL_KINDS = {"sig", "const", "not", "and", "or", "past", "rose", "fell", "stable", "cnteq", "cntbit"}

def ps(s):
    k = s.k
    if k in BOOL_KINDS: return pb(s)
    if k == "delay":
        lo, hi = s.a[1], s.a[2]
        dl = "##%d" % lo if lo == hi else "##[%d:%d]" % (lo, hi)
        return "(%s %s %s)" % (ps(s.a[0]), dl, ps(s.a[3]))
    if k == "sor": return "(%s or %s)" % (ps(s.a[0]), ps(s.a[1]))
    if k == "sand": return "(%s and %s)" % (ps(s.a[0]), ps(s.a[1]))
    if k == "intersect": return "(%s intersect %s)" % (ps(s.a[0]), ps(s.a[1]))
    if k == "throughout": return "(%s throughout %s)" % (pb(s.a[0]), ps(s.a[1]))
    if k == "within": return "(%s within %s)" % (ps(s.a[0]), ps(s.a[1]))
    if k == "first_match": return "first_match(%s)" % ps(s.a[0])
    if k == "rep":
        lo, hi = s.a[1], s.a[2]
        return "%s[*%d]" % (pb(s.a[0]), lo) if lo == hi else "%s[*%d:%d]" % (pb(s.a[0]), lo, hi)
    if k == "goto": return "%s[->%d]" % (pb(s.a[0]), s.a[1])
    if k == "noncons": return "%s[=%d]" % (pb(s.a[0]), s.a[1])
    raise ValueError(k)

SEQ_KINDS = {"delay", "sor", "sand", "intersect", "throughout", "within", "first_match", "rep", "goto", "noncons"}

def pp(p):
    k = p.k
    if k in BOOL_KINDS or k in SEQ_KINDS: return ps(p)
    if k == "impl": return "(%s %s %s)" % (ps(p.a[0]), "|=>" if p.a[2] else "|->", pp(p.a[1]))
    if k == "pnot": return "(not %s)" % pp(p.a[0])
    if k == "pand": return "(%s and %s)" % (pp(p.a[0]), pp(p.a[1]))
    if k == "por": return "(%s or %s)" % (pp(p.a[0]), pp(p.a[1]))
    if k == "implies": return "(%s implies %s)" % (pp(p.a[0]), pp(p.a[1]))
    if k == "pif": return "(if (%s) %s else %s)" % (pb(p.a[0]), pp(p.a[1]), pp(p.a[2]))
    if k == "nexttime": return "(nexttime[%d] %s)" % (p.a[1], pp(p.a[0]))
    if k == "always": return "(always[%d:%d] %s)" % (p.a[1], p.a[2], pp(p.a[0]))
    if k == "s_eventually": return "(s_eventually[%d:%d] %s)" % (p.a[1], p.a[2], pp(p.a[0]))
    if k == "until": return "(%s %s %s)" % (pb(p.a[0]), "until_with" if p.a[2] else "until", pb(p.a[1]))
    raise ValueError(k)

def ptop(top):
    dis, p = top.a
    body = pp(p)
    if dis is not None:
        body = "disable iff (%s) %s" % (pb(dis), body)
    return body

SHELL = """// sva_pbt case {tag}
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) {prop});
endmodule
"""

def sv_text(top, tag=""):
    return SHELL.format(tag=tag, prop=ptop(top))

# ---------------------------------------------------------------- shrinker
def children(n):
    return [x for x in n.a if isinstance(x, N)]

def shrink_candidates(top):
    """Yield smaller variants of the top node (property AST), simplest first."""
    dis, p = top.a
    if dis is not None:
        yield N("top", None, p)
    for q in shrink_node(p):
        yield N("top", dis, q)

def shrink_node(n):
    # 1. replace by a child of compatible class
    for c in children(n):
        if (n.k in BOOL_KINDS and c.k in BOOL_KINDS) or \
           (n.k in SEQ_KINDS and (c.k in SEQ_KINDS or c.k in BOOL_KINDS)) or \
           (n.k not in BOOL_KINDS and n.k not in SEQ_KINDS):
            yield c
    # 2. shrink numeric parameters
    for i, x in enumerate(n.a):
        if isinstance(x, int) and not isinstance(x, bool) and x > 0 and n.k not in ("sig",):
            a = list(n.a); a[i] = x - 1
            cand = N(n.k, *a)
            if valid_numbers(cand):
                yield cand
    # 3. recurse into children
    for i, x in enumerate(n.a):
        if isinstance(x, N):
            for y in shrink_node(x):
                a = list(n.a); a[i] = y
                yield N(n.k, *a)

def valid_numbers(n):
    if n.k in ("delay", "rep", "always"):
        return n.a[1] <= n.a[2] if n.k != "delay" else n.a[1] <= n.a[2]
    if n.k == "s_eventually":
        return n.a[1] <= n.a[2]
    if n.k in ("goto", "noncons", "past", "nexttime"):
        return n.a[1] >= 1
    return True

def size(n):
    return 1 + sum(size(c) for c in children(n))

# ---------------------------------------------------------------- negative (syntax) mutants
def syntax_mutants(sv):
    """Variants that must be REJECTED by a conforming frontend."""
    out = []
    body = sv.split("assert property (@(posedge clk) ", 1)
    if len(body) != 2:
        return out
    head, rest = body
    prop, tail = rest.split(");\nendmodule", 1)
    def mk(p): return head + "assert property (@(posedge clk) " + p + ");\nendmodule" + tail
    out.append(("double-implication", mk("a |-> |-> b")))
    out.append(("empty-delay", mk("a ## b")))
    out.append(("reversed-range", mk("a ##[3:1] b")))
    out.append(("empty-first_match", mk("first_match()")))
    out.append(("unbalanced-paren", mk("(a |-> b")))
    out.append(("past-noargs", mk("$past() |-> a")))
    out.append(("bad-rep", mk("a[*] b")))
    out.append(("prop-in-seq", mk("(a |-> b) ##1 c")))
    return out
