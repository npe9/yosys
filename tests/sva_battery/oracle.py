#!/usr/bin/env python3
"""Expectation provenance for the battery (register v3 C4): check each
`// PROVED` / `// DIS @N` expectation against the INDEPENDENT reference
model of the property-based tester (tests/sva_pbt/ref_sva.py, written from
IEEE 1800-2017 clause 16, never from sva2check).

Only the temporal semantics come from the model.  The Boolean leaves of a
property (any maximal non-temporal subexpression) are evaluated by the
ordinary RTL path: the file is copied with every leaf added as a plain
wire (`wire __oa_i = (<leaf>);`, sampled-value functions as explicit
zero-initialized history registers), the assertion cells are deleted, and
`sim` produces the leaf values per step (step k = the values the k-th
rising edge samples; calibrated: VCD time 10k).  Nothing of the SVA
lowering is involved.

Per expectation:
  DIS @N   replay: ic3's own counterexample witness is replayed; the model
           must fire at exactly step N on it        -> replay-confirmed
           (fires elsewhere / never                  -> REPLAY-REFUTED)
  PROVED   sampling: K random input traces (assumptions respected by
           truncating a trace at the first step an assume fires); the
           model must never fire                    -> model-sampled
           (fires                                   -> SAMPLE-REFUTED)
  DIS @N   sampling as well: no sampled trace may fire before N
           (fires earlier                           -> SAMPLE-REFUTED)
  anything outside the model's grammar             -> unsupported: <why>

A REFUTED line is a finding: the expectation, the engine, the model or
this instrumentation is wrong, and which one is decided by hand.

    oracle.py [files...] [--traces 200] [--len 24] [--seed 1] [--json out]
"""

import argparse
import contextlib
import io
import json
import os
import random
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "sva_pbt"))
import run_ic3  # noqa: E402
import run_battery  # noqa: E402
import ref_sva  # noqa: E402
from ref_sva import N, Trace  # noqa: E402

YOSYS = os.environ.get("YOSYS", run_battery.YOSYS)


class Unsupported(Exception):
    pass


# ------------------------------------------------------------------ lexing
TEMPORAL_KW = {"and", "or", "not", "intersect", "within", "throughout", "first_match",
               "if", "else", "nexttime", "always", "s_eventually", "until", "until_with",
               "implies"}
UNSUPPORTED_KW = {"s_until", "s_until_with", "eventually", "s_nexttime", "s_always", "strong",
                  "weak", "accept_on", "reject_on", "sync_accept_on", "sync_reject_on", "iff",
                  "case", "matched", "triggered", "expect", "followed_by"}
TOK_RE = re.compile(r"""
    (?P<ws>\s+)
  | (?P<op>\|->|\|=>|\#\#|\[\*|\[->|\[=|\[\+\]|\#-\#|\#=\#)
  | (?P<id>\$?[A-Za-z_][A-Za-z0-9_$]*(?:::[A-Za-z_][A-Za-z0-9_$]*)*)
  | (?P<num>\d+'[sS]?[bBoOdDhH][0-9a-fA-FxXzZ_?]+|'[01xXzZ]|\d+)
  | (?P<punct>&&|\|\||==|!=|===|!==|<=|>=|<<|>>|[()\[\]{}:,;!~&|^+\-*/%<>?.=@'$])
""", re.X)


def lex(text):
    out, pos = [], 0
    while pos < len(text):
        m = TOK_RE.match(text, pos)
        if not m:
            raise Unsupported("cannot lex at %r" % text[pos:pos + 12])
        pos = m.end()
        if m.lastgroup == "ws":
            continue
        out.append(m.group(0))
    return out


# ------------------------------------------------------------------ parsing
class Parser:
    """Property text -> ref_sva AST.  Boolean leaves become ("sig",
    "__oa_i") over atom texts collected in self.atoms; sampled-value
    functions become history registers collected in self.history."""

    def __init__(self, toks):
        self.t = toks
        self.i = 0
        self.atoms = []        # leaf texts (index = atom number)
        self.history = []      # (expr text, depth) registers __oh_j

    def peek(self, k=0):
        return self.t[self.i + k] if self.i + k < len(self.t) else None

    def eat(self, tok=None):
        cur = self.peek()
        if tok is not None and cur != tok:
            raise Unsupported("expected %r, got %r" % (tok, cur))
        self.i += 1
        return cur

    def at_end(self):
        return self.i >= len(self.t)

    # --- integer constants inside ## / [* ] / ranges
    def intval(self):
        tok = self.eat()
        if tok is None or not re.match(r"^\d+$", tok):
            raise Unsupported("non-literal bound %r" % tok)
        return int(tok)

    def rng(self):
        """`[m:n]` or `[n]` after ## / always / s_eventually; returns (lo, hi)."""
        self.eat("[")
        lo = self.intval()
        hi = lo
        if self.peek() == ":":
            self.eat(":")
            if self.peek() == "$":
                raise Unsupported("unbounded range")
            hi = self.intval()
        self.eat("]")
        return lo, hi

    # --- the property grammar, lowest precedence first
    def prop(self):
        return self.p_implies()

    def p_implies(self):
        left = self.p_impl()
        if self.peek() == "implies":
            self.eat()
            right = self.p_implies()
            return N("implies", left, right)
        return left

    def p_impl(self):
        kw = self.peek()
        if kw == "if":
            self.eat()
            self.eat("(")
            c = self.boolean_until(")")
            self.eat(")")
            p1 = self.p_impl()
            p2 = N("const", 1)
            if self.peek() == "else":
                self.eat()
                p2 = self.p_impl()
            return N("pif", c, p1, p2)
        if kw == "not":
            self.eat()
            return N("pnot", self.p_impl())
        if kw == "nexttime":
            self.eat()
            n = 1
            if self.peek() == "[":
                self.eat("[")
                n = self.intval()
                self.eat("]")
            return N("nexttime", self.p_impl(), n)
        if kw in ("always", "s_eventually"):
            self.eat()
            if self.peek() != "[":
                raise Unsupported("unbounded %s" % kw)
            lo, hi = self.rng()
            return N(kw, self.p_impl(), lo, hi)
        left = self.p_or()
        op = self.peek()
        if op in ("|->", "|=>"):
            self.eat()
            if not is_seq(left):
                raise Unsupported("implication antecedent is not a sequence")
            right = self.p_impl()
            return N("impl", left, right, op == "|=>")
        if op in ("until", "until_with"):
            self.eat()
            right = self.p_or()
            if not (is_bool(left) and is_bool(right)):
                raise Unsupported("until over non-Boolean operands")
            return N("until", left, right, op == "until_with")
        return left

    def p_or(self):
        left = self.p_and()
        while self.peek() == "or":
            self.eat()
            right = self.p_and()
            left = N("sor" if is_seq(left) and is_seq(right) else "por", left, right)
        return left

    def p_and(self):
        left = self.s_intersect()
        while self.peek() == "and":
            self.eat()
            right = self.s_intersect()
            left = N("sand" if is_seq(left) and is_seq(right) else "pand", left, right)
        return left

    def s_intersect(self):
        left = self.s_within()
        while self.peek() == "intersect":
            self.eat()
            right = self.s_within()
            left = N("intersect", left, right)
        return left

    def s_within(self):
        left = self.s_throughout()
        while self.peek() == "within":
            self.eat()
            right = self.s_throughout()
            left = N("within", left, right)
        return left

    def s_throughout(self):
        left = self.s_delay()
        if self.peek() == "throughout":
            self.eat()
            if not is_bool(left):
                raise Unsupported("throughout with a non-Boolean left side")
            right = self.s_throughout()
            return N("throughout", left, right)
        return left

    def s_delay(self):
        if self.peek() == "##":
            # leading ##n: `##n s` == `1 ##n s`
            left = N("const", 1)
        else:
            left = self.s_rep()
        while self.peek() == "##":
            self.eat()
            if self.peek() == "[":
                lo, hi = self.rng()
            else:
                lo = hi = self.intval()
            right = self.s_rep()
            left = N("delay", left, lo, hi, right)
        return left

    def s_rep(self):
        base = self.primary()
        op = self.peek()
        if op in ("[*", "[->", "[="):
            self.eat()
            lo = self.intval()
            hi = lo
            if self.peek() == ":":
                self.eat(":")
                if self.peek() == "$":
                    raise Unsupported("unbounded repetition")
                hi = self.intval()
            self.eat("]")
            if not is_bool(base):
                raise Unsupported("repetition of a non-Boolean")
            if op == "[*":
                return N("rep", base, lo, hi)
            if lo != hi:
                raise Unsupported("ranged %s" % op)
            return N("goto" if op == "[->" else "noncons", base, lo)
        if op == "[+]":
            raise Unsupported("[+]")
        return base

    def primary(self):
        tok = self.peek()
        if tok is None:
            raise Unsupported("unexpected end")
        if tok in UNSUPPORTED_KW:
            raise Unsupported("operator %s" % tok)
        if tok == "first_match":
            self.eat()
            self.eat("(")
            s = self.prop()
            self.eat(")")
            if not is_seq(s):
                raise Unsupported("first_match of a property")
            return N("first_match", s)
        if tok == "(":
            # a parenthesized group: temporal inside -> recurse; else a leaf
            j, depth, temporal = self.i, 0, False
            while j < len(self.t):
                x = self.t[j]
                if x in ("(", "[", "{"):
                    depth += 1
                elif x in (")", "]", "}"):
                    depth -= 1
                    if depth == 0:
                        break
                elif is_temporal_tok(x):
                    temporal = True
                j += 1
            if temporal:
                self.eat("(")
                p = self.prop()
                self.eat(")")
                return p
        return self.boolean_leaf()

    def boolean_until(self, stop):
        """A Boolean leaf up to `stop` at depth 0 (if/disable iff conditions)."""
        start = self.i
        depth = 0
        while not self.at_end():
            x = self.peek()
            if depth == 0 and x == stop:
                break
            if x in ("(", "[", "{"):
                depth += 1
            elif x in (")", "]", "}"):
                depth -= 1
            elif is_temporal_tok(x):
                raise Unsupported("temporal operator in a Boolean context")
            self.eat()
        return self.make_leaf(self.t[start:self.i])

    def boolean_leaf(self):
        """Maximal Boolean expression: tokens up to a temporal token or an
        unbalanced closer at depth 0."""
        start = self.i
        depth = 0
        while not self.at_end():
            x = self.peek()
            if depth == 0 and (is_temporal_tok(x) or x in (")", ",", ";")):
                break
            if x in ("(", "[", "{"):
                depth += 1
            elif x in (")", "]", "}"):
                depth -= 1
            self.eat()
        if self.i == start:
            raise Unsupported("empty Boolean at %r" % self.peek())
        return self.make_leaf(self.t[start:self.i])

    def make_leaf(self, toks):
        text = " ".join(toks)
        text = self.rewrite_sampled(toks)
        if text in self.atoms:
            idx = self.atoms.index(text)
        else:
            idx = len(self.atoms)
            self.atoms.append(text)
        return N("sig", "__oa_%d" % idx)

    def rewrite_sampled(self, toks):
        """$past/$rose/$fell/$stable/$changed -> history registers."""
        out = []
        i = 0
        while i < len(toks):
            x = toks[i]
            if x in ("$past", "$rose", "$fell", "$stable", "$changed", "$sampled"):
                if i + 1 >= len(toks) or toks[i + 1] != "(":
                    raise Unsupported("bare %s" % x)
                depth, j = 0, i + 1
                args, cur = [], []
                while j < len(toks):
                    y = toks[j]
                    if y == "(":
                        depth += 1
                        if depth == 1:
                            j += 1
                            continue
                    elif y == ")":
                        depth -= 1
                        if depth == 0:
                            break
                    if y == "," and depth == 1:
                        args.append(cur)
                        cur = []
                    else:
                        cur.append(y)
                    j += 1
                args.append(cur)
                if j >= len(toks):
                    raise Unsupported("unbalanced %s" % x)
                inner = "(" + self.rewrite_sampled(args[0]) + ")"
                if x == "$sampled":
                    out.append(inner)
                else:
                    depth_n = 1
                    if x == "$past" and len(args) >= 2:
                        if len(args) > 2 or not re.match(r"^\d+$", " ".join(args[1])):
                            raise Unsupported("$past with a gating clock / non-literal depth")
                        depth_n = int(args[1][0])
                    h = self.hist(inner, depth_n)
                    if x == "$past":
                        out.append(h)
                    elif x == "$rose":
                        out.append("((%s & 1'b1) && !(%s & 1'b1))" % (inner, h))
                    elif x == "$fell":
                        out.append("(!(%s & 1'b1) && (%s & 1'b1))" % (inner, h))
                    elif x == "$stable":
                        out.append("(%s == %s)" % (inner, h))
                    else:
                        out.append("(%s != %s)" % (inner, h))
                i = j + 1
                continue
            if x.startswith("$") and x not in ("$bits", "$countones", "$onehot", "$onehot0", "$isunknown",
                                               "$clog2", "$signed", "$unsigned", "$size", "$high", "$low",
                                               "$left", "$right"):
                raise Unsupported("system function %s" % x)
            out.append(x)
            i += 1
        return " ".join(out)

    def hist(self, expr, n):
        key = (expr, n)
        if key not in self.history:
            self.history.append(key)
        return "__oh_%d" % self.history.index(key)


def is_temporal_tok(x):
    return x in TEMPORAL_KW or x in UNSUPPORTED_KW or x in ("|->", "|=>", "##", "[*", "[->", "[=", "[+]",
                                                            "#-#", "#=#")


BOOL_KINDS = {"sig", "const", "not", "and", "or", "past", "rose", "fell", "stable"}
SEQ_KINDS = BOOL_KINDS | {"delay", "sor", "sand", "intersect", "throughout", "within", "first_match",
                          "rep", "goto", "noncons"}


def is_bool(n):
    return n.k in BOOL_KINDS


def is_seq(n):
    return n.k in SEQ_KINDS


# ------------------------------------------------------------------ source scan
ASSERT_RE = re.compile(r"^\s*(?P<name>[A-Za-z_]\w*)\s*:\s*(?P<kind>assert|assume)\s+property\s*\((?P<body>.*)\)\s*;"
                       r"\s*(?://.*)?$")
CLOCK_RE = re.compile(r"^@\s*\(\s*posedge\s+([A-Za-z_]\w*)\s*\)\s*(.*)$", re.S)
DEFCLK_RE = re.compile(r"default\s+clocking\s+(?:\w+\s+)?@\s*\(\s*posedge\s+([A-Za-z_]\w*)\s*\)")
DISABLE_RE = re.compile(r"^disable\s+iff\s*\((.*)$", re.S)


def split_disable(body):
    """`disable iff (c) p` -> (c text, p text)."""
    m = DISABLE_RE.match(body)
    if not m:
        return None, body
    rest = m.group(1)
    depth = 1
    for i, ch in enumerate(rest):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return rest[:i], rest[i + 1:]
    raise Unsupported("unbalanced disable iff")


NAMED_DECL_RE = re.compile(r"^\s*(sequence|property)\s+([A-Za-z_]\w*)\s*(\(([^)]*)\))?\s*;(.*?)\bend\1\b",
                           re.S | re.M)
OTHER_NAME_RE = re.compile(r"^\s*(?:let|checker|function)\s+(?:[\w\[\]: ]+\s+)?([A-Za-z_]\w*)\s*[(=;]", re.M)
ANY_ASSUME_RE = re.compile(r"\b(assume|restrict)\s+property\b")


def named_decls(src):
    """name -> body text for argument-free sequence/property declarations;
    name -> None for declarations with ports and for let/checker/function
    names (an instance of those is outside the model)."""
    out = {}
    for m in NAMED_DECL_RE.finditer(src):
        body = m.group(5).strip().rstrip(";").strip()
        has_args = m.group(3) is not None and m.group(4).strip() != ""
        if body.startswith("@") or "disable iff" in body:
            out[m.group(2)] = None
        else:
            out[m.group(2)] = None if has_args else body
    for m in OTHER_NAME_RE.finditer(src):
        out.setdefault(m.group(1), None)
    return out


def inline_named(text, decls, depth=0):
    """Replace argument-free named sequence/property references by their
    bodies; refuse instances with arguments and other named constructs."""
    if depth > 8:
        raise Unsupported("named sequence nesting")
    toks = lex(text)
    out = []
    for i, x in enumerate(toks):
        if x in decls:
            if decls[x] is None:
                raise Unsupported("instance of named construct %s" % x)
            if i + 1 < len(toks) and toks[i + 1] == "(":
                raise Unsupported("named sequence %s with arguments" % x)
            out.append("( " + inline_named(decls[x], decls, depth + 1) + " )")
        else:
            out.append(x)
    return " ".join(out)


def scan(path):
    """-> list of dict(name, kind, clock, top: N, parser) for the
    single-line assertions/assumptions of the file; unsupported ones carry
    'why'."""
    src = open(path, errors="ignore").read()
    dm = DEFCLK_RE.search(src)
    defclk = dm.group(1) if dm else None
    decls = named_decls(src)
    out = []
    shared = Parser([])     # one atom/history table per file
    lines = src.splitlines()
    last_mod = max((i for i, l in enumerate(lines) if re.match(r"^\s*module\b", l)), default=0)
    labeled_assumes = 0
    file_why = None
    if re.search(r"\bdefault\s+disable\s+iff\b", src):
        file_why = "default disable iff"
    for ln, line in enumerate(lines):
        m = ASSERT_RE.match(line)
        if not m:
            continue
        rec = dict(name=m.group("name"), kind=m.group("kind"))
        if m.group("kind") == "assume":
            labeled_assumes += 1
        body = m.group("body").strip()
        if file_why:
            rec["why"] = file_why
            out.append(rec)
            continue
        if ln < last_mod:
            rec["why"] = "assertion outside the last module of the file"
            out.append(rec)
            continue
        try:
            body = inline_named(body, decls)
            cm = CLOCK_RE.match(body)
            if cm:
                clock, body = cm.group(1), cm.group(2).strip()
            elif defclk:
                clock = defclk
            else:
                raise Unsupported("no clock")
            dis_text, ptext = split_disable(body)
            p = Parser(lex(ptext))
            p.atoms, p.history = shared.atoms, shared.history
            prop = p.prop()
            if not p.at_end():
                raise Unsupported("trailing %r" % " ".join(p.t[p.i:p.i + 4]))
            dis = None
            if dis_text is not None:
                q = Parser(lex(dis_text))
                q.atoms, q.history = shared.atoms, shared.history
                dis = q.boolean_until(None)
            rec.update(clock=clock, top=N("top", dis, prop))
        except Unsupported as e:
            rec["why"] = str(e)
        out.append(rec)
    # an assumption the scanner did not take (unlabeled, multi-line,
    # restrict) would be silently ignored by the sampling: refuse the file
    if len(ANY_ASSUME_RE.findall(src)) != labeled_assumes:
        for rec in out:
            rec.setdefault("why", "an assumption outside the scanner (unlabeled or multi-line)")
    return out, shared, src


# ------------------------------------------------------------------ instrumented design
def instrument(src, shared, clock, hist_width=None):
    """Copy of the source with the leaves and history registers added to the
    module that holds the assertions (the last `endmodule` before... the
    battery puts one design per file: we add to the last module)."""
    decl = ["", "  // ---- oracle.py instrumentation (leaves of the properties) ----"]
    for j, (expr, n) in enumerate(shared.history):
        # hist_width: a fixed width when $bits() cannot be evaluated at
        # read time (interface members resolve only in hierarchy)
        w = str(hist_width) if hist_width else "$bits(%s)" % expr
        prev = expr
        for d in range(1, n + 1):
            decl.append("  (* keep *) logic [%s-1:0] __oh_%d_%d = '0;" % (w, j, d))
            decl.append("  always_ff @(posedge %s) __oh_%d_%d <= %s;" % (clock, j, d, prev))
            prev = "__oh_%d_%d" % (j, d)
        decl.append("  (* keep *) wire [%s-1:0] __oh_%d = %s;" % (w, j, prev))
    for i, text in enumerate(shared.atoms):
        decl.append("  (* keep *) wire __oa_%d = |(%s);" % (i, text))
    k = src.rfind("endmodule")
    if k < 0:
        raise Unsupported("no endmodule")
    return src[:k] + "\n".join(decl) + "\n" + src[k:]


def run(cmd, timeout=300):
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def sim_values(workdir, design, top, witness, nleaf, steps):
    """Replay a witness on the instrumented design; -> {__oa_i: [v per step]}."""
    vcd = os.path.join(workdir, "o.vcd")
    script = ("read_verilog -sv -formal %s %s\nhierarchy -top %s\nproc\n"
              "delete t:$check t:*sva* t:*past* t:*rose* t:*fell* t:*stable* t:*changed* t:*sampled*\nopt_clean -purge\nsim -r %s -zinit -vcd %s\n") % (
                  run_ic3.read_flags(design), design, top, witness, vcd)
    ys = os.path.join(workdir, "o.ys")
    open(ys, "w").write(script)
    rc, log = run([YOSYS, "-q", ys])
    if rc != 0:
        raise Unsupported("sim: " + next((l for l in log.splitlines() if "ERROR" in l), log[-200:]))
    return read_vcd(vcd, ["__oa_%d" % i for i in range(nleaf)], steps)


def read_vcd(path, names, steps):
    ids, cur, at = {}, {}, {}
    t = None
    for line in open(path):
        line = line.strip()
        m = re.match(r"\$var \S+ \d+ (\S+) (\S+)", line)
        if m:
            ids.setdefault(m.group(1), []).append(m.group(2).split(".")[-1])
            continue
        if line.startswith("#"):
            if t is not None:
                at[t] = dict(cur)
            t = int(line[1:])
            continue
        m = re.match(r"^([01xz])(\S+)$", line) or re.match(r"^b([01xz]+) (\S+)$", line)
        if m and m.group(2) in ids:
            for n in ids[m.group(2)]:
                cur[n] = m.group(1)
    if t is not None:
        at[t] = dict(cur)
    out = {n: [] for n in names}
    times = sorted(at)
    for k in range(steps):
        # the last VCD time <= 10k (values persist between changes)
        tk = max((x for x in times if x <= 10 * k), default=None)
        snap = at.get(tk, {})
        for n in names:
            v = snap.get(n, "x")
            out[n].append(1 if v == "1" else 0)
    return out


def input_ports(workdir, design, top, clock):
    js = os.path.join(workdir, "o.json")
    rc, log = run([YOSYS, "-p", "read_verilog -sv -formal %s %s; hierarchy -top %s; proc; write_json %s" %
                   (run_ic3.read_flags(design), design, top, js)])
    if rc != 0:
        raise Unsupported("read: " + next((l for l in log.splitlines() if "ERROR" in l), log[-200:]))
    imp = [l for l in log.splitlines() if "implicitly declared" in l]
    if imp:
        raise Unsupported("a leaf names an undeclared identifier: " + imp[0].split("Identifier")[-1][:60])
    mod = json.load(open(js))["modules"][top]
    return [(n, len(p["bits"])) for n, p in mod["ports"].items() if p["direction"] == "input" and n != clock]


def random_witness(path, ports, clock, steps, rng):
    sig = [{"path": ["\\" + n], "width": w, "offset": 0, "init_only": False} for n, w in ports]
    st = []
    for _ in range(steps):
        bits = "".join(format(rng.getrandbits(w), "0%db" % w) for n, w in reversed(ports))
        st.append({"bits": bits})
    json.dump({"format": "Yosys Witness Trace", "generator": "oracle.py",
               "clocks": [{"path": ["\\" + clock], "edge": "posedge", "offset": 0}],
               "signals": sig, "steps": st}, open(path, "w"))


# ------------------------------------------------------------------ per file
def check_file(path, args, rng):
    label = os.path.basename(path)
    exp = run_battery.expectations(path)
    recs, shared, src = scan(path)
    asserts = [r for r in recs if r["kind"] == "assert" and r["name"] in exp]
    assumes = [r for r in recs if r["kind"] == "assume"]
    results = {}
    # `LRM @N` on an expectation line: the step the LRM gives where the
    # engine (and so the expectation) differs -- a documented divergence;
    # the model is checked against the LRM step instead
    lrm = {}
    for line in open(path, errors="ignore"):
        m = re.match(r"^\s*([A-Za-z_]\w*)\s*:.*//.*\bLRM @(\d+)", line)
        if m:
            lrm[m.group(1)] = int(m.group(2))
    for name, (kind, n) in exp.items():
        results[name] = dict(expect=(kind, n), oracle=[], why=None, lrm=lrm.get(name))
        if name in lrm and kind == "dis":
            results[name]["engine_step"] = n
            results[name]["expect"] = (kind, lrm[name])
    named = {r["name"]: r for r in asserts}
    for name in results:
        r = named.get(name)
        if r is None:
            results[name]["why"] = "not a single-line `name: assert property (...);`"
        elif "why" in r:
            results[name]["why"] = r["why"]
    live = [r for r in asserts if "why" not in r]
    if not live:
        return label, results
    if any("why" in a for a in assumes):
        for r in live:
            results[r["name"]]["why"] = "an assumption is outside the model: " + next(a["why"] for a in assumes if "why" in a)
        return label, results
    clocks = {r["clock"] for r in live} | {a["clock"] for a in assumes}
    if len(clocks) != 1:
        for r in live:
            results[r["name"]]["why"] = "multiple clocks"
        return label, results
    clock = clocks.pop()
    top = run_ic3.top_pragma(path) or (re.findall(r"^\s*module\s+(\w+)", src, re.M) or ["?"])[-1]
    with tempfile.TemporaryDirectory() as wd:
        design = os.path.join(wd, label)
        try:
            open(design, "w").write(instrument(src, shared, clock))
            ports = input_ports(wd, design, top, clock)
        except Unsupported as e:
            for r in live:
                results[r["name"]]["why"] = str(e)
            return label, results
        nleaf = len(shared.atoms)

        def model_fires(vals, steps):
            tr = Trace({k: v for k, v in vals.items()}) if vals else None
            # truncate at the first step an assumption fires
            limit = steps
            for a in assumes:
                f = ref_sva.fire_steps(a["top"], tr)
                if f:
                    limit = min(limit, f[0])
            if limit <= 0:
                return None, 0
            tr2 = Trace({k: v[:limit] for k, v in vals.items()})
            return {r["name"]: ref_sva.fire_steps(r["top"], tr2) for r in live}, limit

        # 1. replay ic3's own counterexamples (DIS expectations)
        cexdir = os.path.join(wd, "cex")
        os.makedirs(cexdir)
        dis = [r for r in live if results[r["name"]]["expect"][0] == "dis"]
        if dis:
            with contextlib.redirect_stdout(io.StringIO()):
                run_ic3.check_one(YOSYS, path, None, [HERE], args.timeout, cex_prefix=os.path.join(cexdir, "ic3_"))
            for r in dis:
                want = results[r["name"]]["expect"][1]
                w = os.path.join(cexdir, "ic3_%s.yw" % re.sub(r"[^A-Za-z0-9_-]", "_", r["name"]))
                if not os.path.exists(w):
                    results[r["name"]]["oracle"].append("replay: no witness")
                    continue
                steps = len(json.load(open(w))["steps"])
                try:
                    vals = sim_values(wd, design, top, w, nleaf, steps)
                except Unsupported as e:
                    results[r["name"]]["oracle"].append("replay: " + str(e))
                    continue
                fires, limit = model_fires(vals, steps)
                f = (fires or {}).get(r["name"], [])
                # `DIS` without a depth: disproved at some step
                if (want is None and f) or (want is not None and want in f and not [x for x in f if x < want]):
                    results[r["name"]]["oracle"].append("replay-confirmed")
                else:
                    results[r["name"]]["oracle"].append(
                        "REPLAY-REFUTED (model fires %s on ic3's %d-step witness, expected @%s)" % (f, steps, want))
        # 2. random sampling (both kinds)
        steps = args.len
        fired_at = {r["name"]: set() for r in live}
        used = 0
        for k in range(args.traces):
            w = os.path.join(wd, "rand.yw")
            random_witness(w, ports, clock, steps, rng)
            try:
                vals = sim_values(wd, design, top, w, nleaf, steps)
            except Unsupported as e:
                for r in live:
                    results[r["name"]]["oracle"].append("sampling: " + str(e))
                break
            fires, limit = model_fires(vals, steps)
            if fires is None:
                continue
            used += 1
            for nm, f in fires.items():
                fired_at[nm] |= set(f)
        for r in live:
            nm = r["name"]
            kind, want = results[nm]["expect"]
            if used == 0:
                results[nm]["oracle"].append("sampling: every trace violates an assumption at step 0")
                continue
            if kind == "proved":
                if fired_at[nm]:
                    results[nm]["oracle"].append("SAMPLE-REFUTED (model fires at %s on random traces)" %
                                                 sorted(fired_at[nm])[:6])
                else:
                    results[nm]["oracle"].append("model-sampled(%d)" % used)
            elif kind == "dis":
                early = sorted(x for x in fired_at[nm] if want is not None and x < want)
                if early:
                    results[nm]["oracle"].append("SAMPLE-REFUTED (model fires at %s, before @%s)" % (early[:6], want))
                else:
                    results[nm]["oracle"].append("model-sampled(%d)" % used)
    return label, results


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*")
    ap.add_argument("--traces", type=int, default=100)
    ap.add_argument("--len", type=int, default=24)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--json")
    args = ap.parse_args()
    files = args.files or sorted(os.path.join(HERE, f) for f in os.listdir(HERE)
                                 if f.endswith(".sv") and not f.startswith("x") and run_battery.expectations(os.path.join(HERE, f)))
    rng = random.Random(args.seed)
    allres = {}
    counts = {}
    for path in files:
        label, res = check_file(path, args, rng)
        allres[label] = res
        for name, r in res.items():
            if r["why"]:
                cls = "unsupported"
            elif any("REFUTED" in o for o in r["oracle"]):
                cls = "REFUTED"
            elif r.get("lrm") is not None and any(o == "replay-confirmed" for o in r["oracle"]):
                cls = "known-divergence"     # model confirms the LRM step, the engine differs
            elif any(o == "replay-confirmed" for o in r["oracle"]):
                cls = "replay-confirmed"
            elif any(o.startswith("model-sampled") for o in r["oracle"]):
                cls = "model-sampled"
            else:
                cls = "no-evidence"
            counts[cls] = counts.get(cls, 0) + 1
            kind, n = r["expect"]
            exp = "PROVED" if kind == "proved" else ("DIS @%s" % r.get("engine_step", n) if kind == "dis" else kind.upper())
            print("%-22s %-18s %-10s %-17s %s" % (label, name, exp, cls,
                                                  r["why"] if r["why"] else "; ".join(r["oracle"])))
    print("ORACLE " + " ".join("%s=%d" % kv for kv in sorted(counts.items())))
    if args.json:
        json.dump(allres, open(args.json, "w"), indent=1, default=str)
    return 1 if counts.get("REFUTED") else 0


if __name__ == "__main__":
    sys.exit(main())
