#!/usr/bin/env python3
"""Reference SVA evaluator (the *model* of the property-based tester).

An independent, deliberately naive implementation of IEEE 1800-2017
clause 16 sequence/property semantics over a FINITE trace, written
straight from the LRM text and never from sva2check.  It is the oracle:
if the framework and this disagree, one of them is wrong and the case is
shrunk and saved.

Trace convention (calibrated against the framework's sim and BMC legs,
tests/sva_pbt/README.md): steps k = 0..T-1; every signal has a sampled
value per step; an assertion attempt starts at every step t; the attempt
FIRES at the first step d at which its verdict is definitely false
(3-valued evaluation over the prefix 0..d).  $past(x, n) reads x(k-n), or
0 before time zero; $rose/$fell/$stable use the same past.  `disable iff
(c)` with c true at any step in [t, d] turns the attempt into a pass.

Semantics as sets of match END steps.  A sequence evaluated from start
step s over the prefix 0..L-1 yields (ends, open): `ends` are the
definite end steps (< L) of matches; `open` says a match could still
complete at or after L (so the result is not final).  Empty matches
([*0]) end at s-1 and are represented by the special END_EMPTY marker
relative to s.

Supported grammar (v1):
  bool   : sig | !b | b && b | b || b | $past(sig[,n]) | $rose(sig) |
           $fell(sig) | $stable(sig) | cnt == K | cnt[i] | 1'b0 | 1'b1
  seq    : bool | seq ##n seq | seq ##[m:n] seq | seq or seq | seq and seq |
           seq intersect seq | bool throughout seq | seq within seq |
           first_match(seq) | bool[*n] | bool[*m:n] | bool[->n] | bool[=n]
  prop   : seq | seq |-> prop | seq |=> prop | not prop | prop and prop |
           prop or prop | if (bool) prop else prop | nexttime[k] prop |
           always[m:n] prop | s_eventually[m:n] prop | bool until bool |
           bool until_with bool | prop implies prop
  top    : [disable iff (bool)] prop
"""
import itertools

# ---------------------------------------------------------------- AST
class N:
    """Generic AST node: kind + fields.  Kinds listed in the docstring."""
    __slots__ = ("k", "a")
    def __init__(self, k, *a):
        self.k, self.a = k, list(a)
    def __repr__(self):
        return "%s(%s)" % (self.k, ", ".join(repr(x) for x in self.a))
    def __eq__(self, o):
        return isinstance(o, N) and self.k == o.k and self.a == o.a
    def __hash__(self):
        return hash((self.k, tuple(hash(x) if isinstance(x, N) else x for x in self.a)))

T, F, U = True, False, None   # 3-valued: U = undetermined (needs more trace)

def k_and(x, y):
    if x is F or y is F: return F
    if x is T and y is T: return T
    return U
def k_or(x, y):
    if x is T or y is T: return T
    if x is F and y is F: return F
    return U
def k_not(x):
    return U if x is U else (not x)

# ---------------------------------------------------------------- trace
class Trace:
    """sig -> list of ints per step.  Registers derived by the harness."""
    def __init__(self, sigs):
        self.sigs = sigs
        self.T = len(next(iter(sigs.values())))
    def val(self, name, k):
        if k < 0:
            return 0
        return self.sigs[name][k]

# ---------------------------------------------------------------- booleans
def eval_bool(b, tr, k):
    """int (0/1) value of boolean expression b at step k (k < tr.T)."""
    kind = b.k
    if kind == "sig":   return tr.val(b.a[0], k) & 1
    if kind == "const": return b.a[0] & 1
    if kind == "not":   return 1 - eval_bool(b.a[0], tr, k)
    if kind == "and":   return eval_bool(b.a[0], tr, k) & eval_bool(b.a[1], tr, k)
    if kind == "or":    return eval_bool(b.a[0], tr, k) | eval_bool(b.a[1], tr, k)
    if kind == "past":  # past(sig, n)
        return tr.val(b.a[0], k - b.a[1]) & 1
    if kind == "rose":  return 1 if (tr.val(b.a[0], k) & 1) and not (tr.val(b.a[0], k - 1) & 1) else 0
    if kind == "fell":  return 1 if (not (tr.val(b.a[0], k) & 1)) and (tr.val(b.a[0], k - 1) & 1) else 0
    if kind == "stable": return 1 if (tr.val(b.a[0], k) & 1) == (tr.val(b.a[0], k - 1) & 1) else 0
    if kind == "cnteq": return 1 if tr.val("cnt", k) == b.a[0] else 0
    if kind == "cntbit": return (tr.val("cnt", k) >> b.a[0]) & 1
    raise ValueError("bad bool node %r" % b)

# ---------------------------------------------------------------- sequences
EMPTY = object()   # marker: empty match from start s (ends at s-1)

def seq_match(s, tr, start, L):
    """-> (ends, open).  ends: set of end steps e (start <= e < L) or EMPTY.
    open: a match may still complete at step >= L."""
    k = s.k
    if k in ("sig", "const", "not", "and", "or", "past", "rose", "fell", "stable", "cnteq", "cntbit"):
        if start >= L:
            return set(), True
        return ({start} if eval_bool(s, tr, start) else set()), False
    if k == "delay":          # delay(s1, lo, hi, s2)   s1 ##[lo:hi] s2
        s1, lo, hi, s2 = s.a
        ends1, open1 = seq_match(s1, tr, start, L)
        ends, opn = set(), open1
        for e1 in ends1:
            for d in range(lo, hi + 1):
                # LRM 16.9.2: s1 ##d s2 starts s2 at (end of s1) + d;
                # ##0 overlaps the end step.  After an EMPTY s1 (ends at
                # start-1) the same rule gives start + d - 1, except
                # ##0 after an empty match, which starts s2 at start.
                if e1 is EMPTY:
                    s2start = start if d == 0 else start + d - 1
                else:
                    s2start = e1 + d
                ends2, open2 = seq_match(s2, tr, s2start, L)
                for e2 in ends2:
                    if e2 is EMPTY:
                        # empty s2: match ends where s1 ended (only legal for ##0)
                        ends.add(e1 if e1 is not EMPTY else EMPTY)
                    else:
                        ends.add(e2)
                if open2:
                    opn = True
        return ends, opn
    if k == "sor":
        e1, o1 = seq_match(s.a[0], tr, start, L); e2, o2 = seq_match(s.a[1], tr, start, L)
        return e1 | e2, o1 or o2
    if k == "sand":           # both match from start; end = later end
        e1, o1 = seq_match(s.a[0], tr, start, L); e2, o2 = seq_match(s.a[1], tr, start, L)
        ends = set()
        for x in e1:
            for y in e2:
                if x is EMPTY: ends.add(y)
                elif y is EMPTY: ends.add(x)
                else: ends.add(max(x, y))
        opn = (o1 and (o2 or bool(e2))) or (o2 and (o1 or bool(e1)))
        return ends, opn
    if k == "intersect":      # same start and same end
        e1, o1 = seq_match(s.a[0], tr, start, L); e2, o2 = seq_match(s.a[1], tr, start, L)
        # a common end at or beyond L needs BOTH operands to still be
        # able to end there: an end of one operand before L is already in
        # its end set, so it can only pair with an end of the other that
        # is known too (LRM 16.9.6).  (This used to be `o1 or o2`, which
        # left an attempt pending forever once one operand was exhausted:
        # PBT s2_i27, oracle tdollar i_b, 2026-10-19.)
        return e1 & e2, (o1 and o2)
    if k == "throughout":     # b throughout s: b at every step of the match
        b, sq = s.a
        ends, opn = seq_match(sq, tr, start, L)
        out = set()
        for e in ends:
            last = start - 1 if e is EMPTY else e
            if all(eval_bool(b, tr, j) for j in range(start, last + 1)):
                out.add(e)
        # open only if b has held on the whole known prefix from start
        if opn and not all(eval_bool(b, tr, j) for j in range(start, L)):
            opn = False
        return out, opn
    if k == "within":         # s1 within s2: s2 matches [start,e2], s1 matches inside
        s1, s2 = s.a
        e2s, o2 = seq_match(s2, tr, start, L)
        out = set()
        for e2 in e2s:
            if e2 is EMPTY:
                continue
            for st in range(start, e2 + 1):
                e1s, _ = seq_match(s1, tr, st, e2 + 1)
                if any(x is not EMPTY and x <= e2 for x in e1s):
                    out.add(e2); break
        return out, o2
    if k == "first_match":
        ends, opn = seq_match(s.a[0], tr, start, L)
        real = [e for e in ends if e is not EMPTY]
        if EMPTY in ends:
            return {EMPTY}, False
        if real:
            return {min(real)}, False
        return set(), opn
    if k == "rep":            # rep(b, lo, hi): consecutive repetition of a boolean
        b, lo, hi = s.a
        ends, opn = set(), False
        if lo == 0:
            ends.add(EMPTY)
        j = start
        count = 0
        while count < hi:
            if j >= L:
                opn = True; break
            if not eval_bool(b, tr, j):
                break
            count += 1
            if count >= lo:
                ends.add(j)
            j += 1
        return ends, opn
    if k == "goto":           # b[->n]: ends at the n-th occurrence of b
        b, n = s.a
        c = 0
        for j in range(start, L):
            if eval_bool(b, tr, j):
                c += 1
                if c == n:
                    return {j}, False
        return set(), True
    if k == "noncons":        # b[=n]: n occurrences then any non-b steps
        b, n = s.a
        c = 0; ends = set()
        for j in range(start, L):
            if eval_bool(b, tr, j):
                c += 1
                if c == n + 1:
                    return ends, False
                if c == n:
                    ends.add(j)
            elif c == n:
                ends.add(j)
        return ends, True
    raise ValueError("bad seq node %r" % s)

# ---------------------------------------------------------------- properties
def prop_eval(p, tr, t, L):
    """3-valued verdict of property p for the attempt starting at step t,
    knowing the trace prefix 0..L-1 only."""
    k = p.k
    if k in ("sig", "const", "not", "and", "or", "past", "rose", "fell", "stable", "cnteq", "cntbit"):
        if t >= L: return U
        return bool(eval_bool(p, tr, t))
    if k in ("delay", "sor", "sand", "intersect", "throughout", "within", "first_match", "rep", "goto", "noncons"):
        # sequence as a (weak) property: holds iff some NON-EMPTY match
        # exists (LRM 16.12.1: an empty match does not satisfy a
        # sequence property)
        ends, opn = seq_match(p, tr, t, L)
        if any(e is not EMPTY for e in ends): return T
        return U if opn else F
    if k == "impl":           # impl(seq, prop, nonoverlap)
        sq, q, nonov = p.a
        ends, opn = seq_match(sq, tr, t, L)
        res = T
        for e in ends:
            base = (t - 1) if e is EMPTY else e
            st = base + 1 if nonov else (t if e is EMPTY else e)
            res = k_and(res, prop_eval(q, tr, st, L))
            if res is F: return F
        if opn and res is T:
            return U
        return res
    if k == "pnot":   return k_not(prop_eval(p.a[0], tr, t, L))
    if k == "pand":   return k_and(prop_eval(p.a[0], tr, t, L), prop_eval(p.a[1], tr, t, L))
    if k == "por":    return k_or(prop_eval(p.a[0], tr, t, L), prop_eval(p.a[1], tr, t, L))
    if k == "implies": return k_or(k_not(prop_eval(p.a[0], tr, t, L)), prop_eval(p.a[1], tr, t, L))
    if k == "pif":            # pif(b, p1, p2)
        if t >= L: return U
        return prop_eval(p.a[1] if eval_bool(p.a[0], tr, t) else p.a[2], tr, t, L)
    if k == "nexttime":       # nexttime[n] p  (weak: true if trace ends first)
        n = p.a[1]
        if t + n >= tr.T: return T
        return prop_eval(p.a[0], tr, t + n, L)
    if k == "always":         # always[lo:hi] p
        q, lo, hi = p.a
        res = T
        for j in range(t + lo, t + hi + 1):
            if j >= tr.T: break          # weak: beyond the trace holds
            res = k_and(res, prop_eval(q, tr, j, L))
            if res is F: return F
        return res
    if k == "s_eventually":   # s_eventually[lo:hi] p: some j in range (strong, bounded)
        q, lo, hi = p.a
        res = F
        for j in range(t + lo, t + hi + 1):
            if j >= L:
                return U if res is F else res
            res = k_or(res, prop_eval(q, tr, j, L))
            if res is T: return T
        return res
    if k == "until":          # until(b, c, with): b holds until c (weak)
        b, c, with_ = p.a
        for j in range(t, L):
            cj = eval_bool(c, tr, j); bj = eval_bool(b, tr, j)
            if cj:
                return T if (not with_ or bj) else F
            if not bj:
                return F
        return U if L < tr.T else T    # trace ended: weak -> holds
    raise ValueError("bad prop node %r" % p)

def attempt_fire_step(top, tr, t):
    """Step at which the attempt starting at t fires, or None.
    top = ("top", disable_bool_or_None, prop)."""
    dis, p = top.a
    for d in range(t, tr.T):
        L = d + 1
        if dis is not None and any(eval_bool(dis, tr, j) for j in range(t, L)):
            return None          # disabled before/at determination
        v = prop_eval(p, tr, t, L)
        if v is F:
            return d
        if v is T:
            return None
    return None

def fire_steps(top, tr):
    """Sorted list of distinct steps at which the assertion fires."""
    out = set()
    for t in range(tr.T):
        d = attempt_fire_step(top, tr, t)
        if d is not None:
            out.add(d)
    return sorted(out)
