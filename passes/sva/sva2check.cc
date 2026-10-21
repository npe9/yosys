/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2026  The sv-yosys project
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

// Compile $sva property trees (produced by the SystemVerilog frontend for
// concurrent assertions) into synchronous monitor hardware, lowering each
// assert-flavor $check into classic EN/A form for model checkers.
//
// Supported: single-clock spec(posedge|negedge clk, disable_iff?, body) with
//   sequences: boolean exprs, true, ##const/##[m:n]/##[*]/##[+] (also as a
//     leading `@(c) ##k body` resampling delay), [*m:n]/[+]/[->m:n]/[=m:n]
//     (of booleans or sub-sequences), throughout, clocked sub-sequences on
//     the same clock (transparent), inside, weq/wne
//   properties: |->, |=>, implies, iff, or, and, not, if/ifelse, case,
//     always/s_always (optionally bounded), nexttime/s_nexttime [k],
//     until/until_with, accept_on/reject_on and sync variants,
//     bounded eventually, strong bounded sequences
//   liveness v1: unbounded eventually/s_eventually of a plain expression
//     lowers to a FLAVOR "liveness" $check (A = p, not negated, EN = 1'b1)
//     for the native ic3 liveness engine; the operand must be a plain
//     boolean expression (no temporal operators, locals, or named decls)
//   liveness v2: a |->/|=> consequent of unbounded (s_)eventually with a
//     plain boolean operand (nested-temporal response) lowers to a single
//     pending-bit FF on the property clock, D = !b && (open || pend) with
//     open = antecedent end (|=> delayed one tick onto the first window
//     tick); FLAVOR "liveness", A = !pend || b (ic3: bad = pend && !b).
//     Locals and named-decls reject as in v1
//   v1 multi-clock: the direct consequent of |->/|=> may be a `clocked`
//     sub-sequence on a different clock (optionally under strong/weak):
//     it compiles as its own monitor island on the inner clock, started by
//     the antecedent end/start pulse; the enclosing $check keeps EN=1'b1
//     and carries no CLK attributes (checked at every formal step; use the
//     clk2fflogic prep so both islands advance under interleaving semantics)
// Rejected with a clear message: any other multi-clock element (mid-sequence
//   or antecedent `clocked`, `clocked` under throughout/within/first_match,
//   or_event/bare-identifier clocking events, instance bodies), unbounded
//   eventually over a temporal operand, s_until_with over a temporal p
//   (plain s_until accepts a bounded temporal p: its per-tick failure pulse
//   feeds the weak monitor), local variable assignments, within,
//   matches/streaming.
//
// Strong until v1: p s_until q / s_until_with with q a plain boolean or a
//   bounded sequence splits into the weak-until monitor on the main $check
//   plus an F q obligation as a second $check named <name>$live with FLAVOR
//   "liveness"; the ic3 pass proves each leg independently.  p may be a
//   plain boolean or a bounded temporal property (a sequence or
//   implication with a finite horizon): it compiles to its per-tick
//   failure pulse, inverted to the "p holds" level the monitor samples.
//   s_until_with over a temporal p is rejected (the with-variant's q&p
//   completion conjunct is not defined for temporal p).
//
// Sequences compile to nondeterministic-start transducers shared across
// attempts: compile_seq(node, start, gate) returns an "ends at this tick"
// pulse (existential over parses).  Properties compile to a per-tick
// violation pulse: compile_prop(node, start) with start = 1 at the top
// level (every tick is an attempt start) or an antecedent-end pulse for
// spawned consequents.  The $check gets EN=1'b1, A = disable || !fail.

#include "kernel/sigtools.h"
#include "kernel/yosys.h"
#include <cstdio>
#include <algorithm>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

static std::string sva_op(Cell *cell)
{
	if (!cell || !cell->hasParam(ID(OP).c_str()))
		log_error("sva2check: internal: sva_op on non-$sva cell %s (type %s).\n",
				cell ? log_id(cell) : "(null)", cell ? cell->type.c_str() : "-");
	return cell->getParam(ID(OP).c_str()).decode_string();
}

// set bit i of a SigSpec (no in-place bit setter on SigSpec)
static SigSpec sigset(SigSpec s, int i, SigBit b)
{
	std::vector<SigBit> v;
	for (int k = 0; k < s.size(); k++)
		v.push_back(k == i ? b : s[k]);
	return SigSpec(v);
}

struct SvaCompiler {
	Module *mod;
	SigSpec clk;
	bool clk_pos;
	// set when the property compiled with a clock switch (multi-clock
	// consequent island): the enclosing $check monitors more than one
	// clock, so it must not carry the outer sampling clock
	bool multiclock = false;
	std::string prefix;
	int uid = 0;

	// $sva cells found in the module, keyed by their placeholder Y bit
	// (canonicalized through a SigMap so aliased connections resolve too).
	dict<SigBit, Cell *> by_y;

	// 1-bit unary drivers ($reduce_bool of a formal reference, etc.), used
	// by resolve() to see through width-normalization cells the frontend
	// wraps around identifier operands.
	dict<Wire *, Cell *> unary_drv;

	// wire -> driving cell for non-$sva cells (frontend expression trees);
	// used by clone_tree to reach inside expressions that reference local
	// variables or formal arguments.
	dict<Wire *, Cell *> expr_drv;

	// --- per-attempt local variable slots -------------------------------
	// When the compiled property samples local variables, every in-flight
	// attempt owns a slot indexed by its age (ticks since the attempt
	// started, modulo nslots).  `age` is the compile-time age vector of
	// the node currently being compiled: bit k says a parse of an attempt
	// of age k can be at this node now.  It threads through ## delays
	// (age_shift), so captures write slot[age] and reads splice a mux
	// over the slot file selected by the age vector at the read site.
	// Linear sequences keep the vector one-hot (exact); range delays make
	// several bits hot, in which case the lowest hot age wins (documented
	// approximation -- violations visible only to younger overlapping
	// attempts can be missed).
	bool slots_mode = false;
	int nslots = 0;
	SigSpec age;
	SigSpec phase;   // runtime tick counter mod nslots (slot ring address base)
	// multi-clock consequent island (v1): reads-only bridge for locals.
	// Every local known at the handoff gets a shift chain on the consequent
	// clock; stage 0 loads the outer ring's value (combinatorial at the
	// handoff tick, stable for the rest of the outer cycle) under the
	// island-start pulse, stage k shifts once per consequent tick.  Island
	// reads select the stage by the island age vector; writes are rejected.
	bool island_mode = false;
	bool in_island = false;   // compiling a multi-clock island's consequent
	dict<Wire *, std::vector<Wire *>> island_chains;
	// raw (un-spliced) gate expression set by `throughout`: re-spliced at
	// the compile-time age current at each gate consumption point, so a
	// gate referencing local variables reads the slot of the tick it
	// constrains rather than the tick compilation started at
	SigSpec dyn_gate;
	// set with slots_mode: ranged delays/repetitions split their tail
	// compiles per admissible offset (single-hot ages) so local reads
	// splice the slot of the tick their parse is at; product of split
	// spans is capped by split_budget, beyond which the merged multi-hot
	// window approximation returns (documented)
	bool split_ranges = false;
	int split_budget = 64;
	bool in_split = false;   // re-entry guard: seq_end stays merged while set

	// --- lowering provenance (register v3 A3/B3) ------------------------
	// Every documented approximation taken while compiling the current
	// check is recorded by kind (the kind names are stable: they are the
	// rows of the approximation table in tests/sva_battery/README.md),
	// warned about once per check and kind with the check's name, and
	// written to the $check as (* sva_lowering = "approx:k1,k2" *) so
	// every verdict printer can show it next to PROVED/DIS.  With -exact
	// an approximation is an error instead of a warning.
	std::string check_name;
	bool exact_mode = false;
	// report a sequence failure at the tick its last thread dies (LRM)
	// rather than at its deadline: on for assume/restrict (the late form
	// accepts prefixes the LRM already rejects -> spurious counterexamples),
	// opt-in for asserts (-early: same verdicts, LRM failure ticks, but
	// one monitor copy per attempt slot -- ~100x on ic3 proofs)
	bool early_fail = false;
	std::vector<std::string> approx_kinds;
	void approx(const std::string &kind, const std::string &where)
	{
		if (exact_mode)
			log_error("sva2check: -exact: %s needs the '%s' approximation (%s).\n",
					check_name.c_str(), kind.c_str(), where.c_str());
		if (std::find(approx_kinds.begin(), approx_kinds.end(), kind) != approx_kinds.end())
			return;
		approx_kinds.push_back(kind);
		log_warning("sva2check: %s: lowered with approximation '%s' (%s).\n",
				check_name.c_str(), kind.c_str(), where.c_str());
	}
	std::string lowering_tag() const
	{
		if (approx_kinds.empty())
			return "exact";
		std::string t = "approx:";
		for (size_t i = 0; i < approx_kinds.size(); i++)
			t += (i ? "," : "") + approx_kinds[i];
		return t;
	}
	struct SlotFile {
		int width = 0;
		std::vector<Wire *> q;                        // slot registers
		std::vector<Cell *> dff;                      // their cells (rebuilt on new writes)
		std::vector<std::vector<std::pair<SigSpec, SigSpec>>> wr;  // per-slot (we, expr) write sources
	};
	dict<Wire *, SlotFile> slotfiles;
	// same-tick chained assignments (x = e1 then y = x+1 under ##0): the
	// most recent writes, visible to reads until the age ticks
	dict<Wire *, std::pair<SigSpec, SigSpec>> pending_wr;
	// read/address memoization: valid while the age/pending context is
	// unchanged (bumped whenever they mutate)
	int slot_epoch = 0;
	dict<Wire *, std::pair<int, SigSpec>> read_cache;
	dict<int, SigSpec> addr_cache;

	// --- lazy per-node cloning of instanced declaration bodies ----------
	// clone_scope set while compiling an instance body: each tree node is
	// copied when the walk reaches it (ensure_stub), and each expression
	// port is filled when first read (cport), so local reads splice at the
	// age current at that read.  clone_lmap gives the body's local
	// variables fresh wires for this instance so distinct instance sites
	// own distinct slot files.
	Cell *clone_scope = nullptr;
	dict<Cell *, Cell *> clone_cmap;
	dict<Cell *, pool<IdString>> stubbed_ports;
	std::vector<Wire *> *clone_formals = nullptr;
	dict<Wire *, Wire *> clone_lmap;

	// Named sequence/property/let declarations: NAME -> decl cell (shared,
	// built per module).  Bodies are inlined at instance sites.
	dict<std::string, Cell *> *decls;

	// Substitution environment: formal-argument wires and captured local
	// variable wires mapped to their actual signals.
	dict<Wire *, SigSpec> subst;
	int inline_depth = 0;

	SvaCompiler(Module *mod, SigSpec clk, bool clk_pos, std::string prefix,
			dict<std::string, Cell *> *decls = nullptr) :
			mod(mod), clk(clk), clk_pos(clk_pos), prefix(prefix), decls(decls)
	{
		for (auto cell : mod->cells())
			if (cell->type == "\\$sva") {
				SigSpec y = cell->getPort(ID(Y));
				if (!y.empty())
					for (auto bit : y)
						by_y[bit] = cell;
			} else if (cell->type.in(ID($reduce_bool), ID($reduce_and), ID($reduce_or),
						ID($reduce_xor), ID($reduce_xnor), ID($reduce_nand), ID($reduce_nor))) {
				SigSpec y = cell->getPort(ID(Y));
				if (GetSize(y) >= 1 && y[0].is_wire())
					unary_drv[y[0].wire] = cell;
			} else if (cell->hasPort(ID(Y))) {
				SigSpec y = cell->getPort(ID(Y));
				for (int i = 0; i < GetSize(y); i++)
					if (y[i].is_wire()) {
						expr_drv[y[i].wire] = cell;
						break;
					}
			}
	}

	// Follow simple wire aliases and cell drivers from a bit to the $sva
	// cell whose placeholder Y it represents (used for $check A ports).
	Cell *find_sva_driver(SigBit b)
	{
		pool<SigBit> seen;
		while (b.is_wire() && seen.insert(b).second) {
			if (by_y.count(b))
				return by_y[b];
			for (auto &conn : mod->connections()) {
				bool follow = false;
				for (int i = 0; i < GetSize(conn.first); i++)
					if (conn.first[i] == b && conn.second[i].is_wire()) {
						b = conn.second[i];
						follow = true;
						break;
					}
				if (follow)
					continue;
				for (int i = 0; i < GetSize(conn.second); i++)
					if (conn.second[i] == b && conn.first[i].is_wire()) {
						b = conn.first[i];
						follow = true;
						break;
					}
				if (follow)
					break;
			}
			if (b.is_wire() && !by_y.count(b) && seen.count(b))
				return nullptr;
		}
		return nullptr;
	}

	// Resolve a leaf signal through the substitution environment, chasing
	// reduction cells ($reduce_bool and friends) that the frontend wraps
	// around identifier operands for width normalization.  Reduction cells
	// are transparent in the boolean contexts where they appear (sequence
	// operands are reduced anyway); $not/$logic_not/$pos/$neg are NOT
	// chased -- they are meaningful logic (chasing them changed `##1 !a`
	// into `##1 a`).  Ports whose bits are all one wire plus constant-zero
	// padding (the frontend pads uniform argument ports to 32 bits) reduce
	// to that wire.
	SigSpec resolve(SigSpec s)
	{
		// a local variable wire reads its per-attempt slot at the current age
		{
			Wire *rep = nullptr;
			bool single = true;
			for (int i = 0; i < s.size(); i++)
				if (s[i].is_wire()) {
					if (!rep)
						rep = s[i].wire;
					else if (s[i].wire != rep) {
						single = false;
						break;
					}
				} else if (s[i].data != State::S0) {
					single = false;
					break;
				}
			if (single && rep && slotfiles.count(rep)) {
				return local_read(rep);
			}
		}
		pool<Wire *> seen;
		while (true) {
			Wire *rep = nullptr;
			bool ok = true;
			for (int i = 0; i < s.size(); i++)
				if (s[i].is_wire()) {
					if (!rep)
						rep = s[i].wire;
					else if (s[i].wire != rep) {
						ok = false;
						break;
					}
				} else if (s[i].data != State::S0) {
					ok = false;
					break;
				}
			if (!ok || !rep || !seen.insert(rep).second)
				break;
			// a local variable wire (possibly behind reduction cells)
			// reads its per-attempt slot at the current age
			if (slotfiles.count(rep)) {
				return local_read(rep);
			}
			if (subst.count(rep)) {
				s = subst[rep];
				continue;
			}
			if (unary_drv.count(rep)) {
				s = unary_drv[rep]->getPort(ID(A));
				continue;
			}
			break;
		}
		return s;
	}

	Wire *wn(const char *role, int width = 1)
	{
		return mod->addWire(prefix + stringf("$%s$%d", role, uid++), width);
	}

	// `disable iff` condition of the check being compiled (empty when
	// none).  LRM 16.15: an attempt is abandoned when the condition is
	// true at ANY step of its evaluation, so every monitor state register
	// is synchronously cleared while it holds -- in-flight attempts die,
	// an attempt starting on a disabled step never arms.  Sampled-value
	// registers ($past chains, role "past") are data, not attempt state,
	// and are never cleared.  (Before this the condition was only ORed
	// into the final check, i.e. sampled at the firing step: sva_pbt F7.)
	SigSpec dis;

	SigSpec dis_gate(SigSpec d)
	{
		if (dis.empty())
			return d;
		return mod->Mux(NEW_ID, d, Const(0, d.size()), dis);
	}

	// Monitor registers created while ff_log is set are recorded with
	// their next-state input (register v3 D5, early failure): `safe` ones
	// are pulse delays and flags whose all-zero next state means no
	// thread survives; counters, slot files and feedback state can sit at
	// zero while a thread still waits, and disable the early check.
	std::vector<std::pair<SigSpec, bool>> *ff_log = nullptr;
	Cell *ffd(SigSpec d, SigSpec q, bool safe)
	{
		if (ff_log)
			ff_log->push_back({d, safe});
		return mod->addDff(NEW_ID, clk, d, q, clk_pos);
	}

	// State register with next-state function fed back from Q.
	SigSpec feedback(const std::function<SigSpec(SigSpec)> &f, const char *role, int width)
	{
		Wire *q = wn(role, width);
		ffd(dis_gate(f(q)), q, false);
		return q;
	}

	// e has fired at or after the last tick st was high (same tick
	// counts); st resets it.  Meaningful with one attempt in flight.
	SigSpec since(SigSpec e, SigSpec st)
	{
		Wire *q = wn("seen", 1);
		SigSpec cur = orN({e, andN({mod->Not(NEW_ID, st), SigSpec(q)})});
		ffd(dis_gate(cur), q, true);
		return cur;
	}

	SigSpec reg(SigSpec d, const char *role)
	{
		// a compile-time age bit that can never be hot stays the constant
		// 0 (monitor registers carry no init attribute, so the fold also
		// keeps a never-hot age from reading free at step 0 without
		// -init-zero); the hot-age count in local_read relies on it
		if (strcmp(role, "age") == 0 && d.is_fully_const() && d.as_const().is_fully_zero())
			return Const(0, d.size());
		Wire *q = wn(role, d.size());
		ffd(strcmp(role, "past") == 0 ? d : dis_gate(d), q, true);
		return q;
	}

	SigSpec delay(SigSpec s, int k, const char *role = "delay")
	{
		for (int i = 0; i < k; i++)
			s = reg(s, role);
		return s;
	}

	// The token of a `##k` gap carried k ticks; inside `b throughout ...`
	// (dyn_gate set) b must hold at every gap tick (LRM 16.9.9): the
	// ticks strictly between the lhs end and the tail start.  Without a
	// throughout gate this is plain delay().
	SigSpec gap_delay(SigSpec s, int k, SigSpec gate, const char *role = "delay")
	{
		if (dyn_gate.empty())
			return delay(s, k, role);
		SigSpec g = gate_now(gate);
		for (int i = 0; i < k; i++)
			s = reg(i == 0 ? s : andN({s, g}), role);
		return s;
	}

	// Lower $past/$rose/$fell/$stable/$changed cells (emitted by the
	// frontend for calls in concurrent-assertion context, where no clocked
	// block surrounds them) into register chains on this check's sampling
	// clock.  `done` carries across checks of one module; cells no check
	// claims are degraded to their data argument after the loop.
	void lower_sampled_value_cells(pool<Cell *> &done)
	{
		std::vector<Cell *> rm;
		std::vector<Cell *> scan(mod->cells().begin(), mod->cells().end());
		for (auto cell : scan) {
			if (!cell->type.in("\\$past", "\\$rose", "\\$fell", "\\$stable", "\\$changed"))
				continue;
			if (!done.insert(cell).second)
				continue;
			SigSpec a = cell->getPort(ID(A));
			SigSpec y = cell->getPort(ID(Y));
			auto past_chain = [&](SigSpec s, int steps) {
				for (int i = 0; i < steps; i++)
					s = reg(s, "past");
				return s;
			};
			if (cell->type == "\\$past") {
				int steps = 1;
				SigSpec b = cell->getPort(ID(B));
				if (!b.empty() && !const_int(b, steps))
					log_error("sva2check: non-constant $past step count in %s.\n", log_id(cell));
				if (steps < 0 || steps > 64)
					log_error("sva2check: $past step count %d out of range in %s.\n", steps, log_id(cell));
				mod->connect(y, steps == 0 ? a : past_chain(a, steps));
			} else {
				SigSpec pa = past_chain(a, 1);
				if (cell->type == "\\$rose")
					mod->connect(y, mod->And(NEW_ID, SigSpec(a[0]), mod->Not(NEW_ID, SigSpec(pa[0]))));
				else if (cell->type == "\\$fell")
					mod->connect(y, mod->And(NEW_ID, mod->Not(NEW_ID, SigSpec(a[0])), SigSpec(pa[0])));
				else if (cell->type == "\\$stable")
					mod->connect(y, mod->Eq(NEW_ID, a, pa));
				else
					mod->connect(y, mod->Ne(NEW_ID, a, pa));
			}
			rm.push_back(cell);
		}
		for (auto cell : rm) {
			// the removed cell's Y wires stay valid (lowering
			// reconnects them), but expr_drv entries pointing at the
			// removed cell dangle -- a later splice_expr chase (the
			// throughout/within dyn_gate re-splice is one) would call
			// getPort on freed memory and abort.  Drop those entries;
			// the wire then splices as opaque design logic.
			SigSpec y = cell->getPort(ID(Y));
			for (auto bit : y)
				if (bit.is_wire() && expr_drv.count(bit.wire) && expr_drv[bit.wire] == cell)
					expr_drv.erase(bit.wire);
			mod->remove(cell);
		}
	}

	// LRM 16.14.6 sequence methods: the frontend parses `s5.triggered` /
	// `s5.matched` as a hierarchical identifier and leaves an undriven wire
	// named after the method.  Drive every such wire whose stem names a
	// sequence declaration with the declaration body's end pulse attempted
	// at every tick (gate 1).  Both methods' values do not depend on the
	// starting point of the match, and at a single clock matched coincides
	// with triggered (its only extra effect is cross-clock
	// synchronization), so one lowering serves both.
	void lower_sequence_methods()
	{
		if (!decls || decls->empty())
			return;
		pool<Wire *> driven;
		for (auto c : mod->cells())
			for (auto &conn : c->connections())
				if (c->output(conn.first))
					for (auto bit : conn.second)
						if (bit.is_wire())
							driven.insert(bit.wire);
		for (auto &conn : mod->connections())
			for (auto bit : conn.first)
				if (bit.is_wire())
					driven.insert(bit.wire);
		std::vector<Wire *> scan(mod->wires().begin(), mod->wires().end());
		for (Wire *w : scan) {
			std::string nm = w->name.str();
			for (int m = 0; m < 2; m++) {
				const char *suf = m ? ".matched" : ".triggered";
				size_t sl = strlen(suf);
				if (nm.size() <= sl || nm.compare(nm.size() - sl, sl, suf) != 0)
					continue;
				std::string base = nm.substr(0, nm.size() - sl);
				if (!base.empty() && base[0] == '\\')
					base = base.substr(1);
				if (!decls->count(base))
					break;
				if (driven.count(w)) {
					log_warning("Wire `%s' matches sequence method syntax but is driven; using the wire.\n", nm.c_str());
					break;
				}
				mod->connect(w, inline_decl((*decls)[base], {}, false,
						Const(1, 1), Const(1, 1)));
				break;
			}
		}
	}

	SigSpec orN(std::initializer_list<SigSpec> args)
	{
		SigSpec r = Const(0, 1);
		for (auto &a : args)
			if (!a.empty())
				r = mod->Or(NEW_ID, r, a);
		return r;
	}

	SigSpec andN(std::initializer_list<SigSpec> args)
	{
		SigSpec r = Const(1, 1);
		for (auto &a : args)
			if (!a.empty())
				r = mod->And(NEW_ID, r, a);
		return r;
	}

	SigSpec reduce1(SigSpec s)
	{
		if (s.empty())
			return Const(1, 1);
		if (s.size() != 1)
			return mod->ReduceBool(NEW_ID, s);
		return s;
	}

	// Gate as consumed at this point of the walk: the threaded gate AND,
	// when a `throughout` set a raw gate expression, that expression
	// re-spliced at the compile-time age current here.
	SigSpec gate_now(SigSpec gate)
	{
		if (dyn_gate.empty())
			return gate;
		static std::vector<Wire *> no_formals;
		std::vector<Wire *> *f = clone_scope && clone_formals ? clone_formals : &no_formals;
		return andN({gate, reduce1(splice_expr(dyn_gate, *f, false))});
	}

	bool const_int(SigSpec s, int &v)
	{
		if (!s.is_fully_const())
			return false;
		v = s.as_int();
		return true;
	}

	// Instance bodies may carry their own spec wrapper (a declaration
	// written with an explicit clock, or a property decl whose body is a
	// spec).  Unwrap it: only the identical clock is allowed (treated as
	// transparent) and disable_iff inside an instantiated body is rejected
	// (LRM: disable_iff is not permitted in nested property instances).
	// A bare-expression body lands in bare_out.
	Cell *unwrap_spec(Cell *body, SigSpec *bare_out = nullptr)
	{
		if (bare_out)
			*bare_out = SigSpec();
		if (!body || sva_op(body) != "spec")
			return body;
		Cell *clock = nullptr, *disable = nullptr, *inner = nullptr;
		SigSpec bare;
		for (auto port : ports_of(body)) {
			SigSpec sp = body->getPort(port);
			if (sp.empty() || sp.is_fully_const())
				continue;
			Cell *drv = child_cell(sp);
			if (drv) {
				std::string op = sva_op(drv);
				if ((op == "posedge" || op == "negedge" || op == "edge" || op == "no_clock") && !clock)
					clock = drv;
				else if ((op == "disable_iff" || op == "no_disable") && !disable)
					disable = drv;
				else if (!inner && bare.empty())
					inner = drv;
			} else if (!inner && bare.empty()) {
				bare = sp;
			}
		}
		if (clock) {
			std::string clop = sva_op(clock);
			SigSpec clksig = clock->getPort(ID(A));
			if (clop != "no_clock" && (clksig.empty() || clksig[0] != clk[0] || (clop == "negedge") == clk_pos))
				log_error("sva2check: instance declaration uses a different sampling clock in %s.\n", log_id(body));
		}
		if (disable && sva_op(disable) == "disable_iff")
			log_error("sva2check: disable_iff inside an instantiated property/sequence is not supported in %s.\n", log_id(body));
		if (!inner && !bare.empty()) {
			if (bare_out)
				*bare_out = bare;
			return nullptr;
		}
		return inner ? inner : body;
	}

	// Extract the sampling clock of a `clocked` node (clocking event in
	// port A, body in port B).  Only plain posedge/negedge/edge event
	// cells are recognized; or_event lists and bare identifiers return
	// false (their clock is not a single signal).
	bool clocked_event(Cell *n, SigSpec &clksig, bool &clksig_pos)
	{
		Cell *ev = child_cell(n->getPort(ID(A)));
		if (!ev)
			return false;
		std::string eop = sva_op(ev);
		if (eop != "posedge" && eop != "negedge" && eop != "edge")
			return false;
		SigSpec s = ev->getPort(ID(A));
		if (s.empty() || !s[0].is_wire())
			return false;
		clksig = s;
		clksig_pos = eop != "negedge";
		return true;
	}

	// Is this `clocked` node on a different sampling clock than the one
	// currently compiled?  Unrecognized events conservatively count as
	// different (the caller then rejects or re-checks them).
	bool clocked_differs(Cell *n)
	{
		SigSpec cclk;
		bool cpos;
		if (!clocked_event(n, cclk, cpos))
			return false;
		return cclk[0] != clk[0] || cpos != clk_pos;
	}

	// --- tree access -------------------------------------------------------

	std::vector<IdString> ports_of(Cell *cell)
	{
		std::vector<IdString> ports;
		int nargs = cell->hasParam(ID(NARGS).c_str()) ? cell->getParam(ID(NARGS).c_str()).as_int() : 5;
		static const IdString arg_ports[] = {ID(A), ID(B), ID(C), ID(D), ID(E)};
		for (int i = 0; i < nargs; i++) {
			if (i < 5)
				ports.push_back(arg_ports[i]);
			else
				ports.push_back(stringf("\\P%d", i).c_str());
		}
		return ports;
	}

	// Port content: either a child $sva cell or a plain expression.  Child
	// references are padded to 32+ bits by the frontend, so look at the LSB.
	Cell *child_cell(SigSpec s)
	{
		if (s.empty() || !s[0].is_wire() || !by_y.count(s[0]))
			return nullptr;
		for (int i = 1; i < s.size(); i++)
			if (s[i].is_wire())
				return nullptr;
		return by_y[s[0]];
	}

	// Named declaration lookup by bare name.  Instance cells carry the
	// callee name as OP (see lookup_decl below); bare references arrive as
	// plain identifier wires instead.
	Cell *decl_by_name(const std::string &name)
	{
		if (!decls)
			return nullptr;
		std::string nm = name;
		if (!nm.empty() && nm[0] == '\\')
			nm = nm.substr(1);
		auto it = decls->find(nm);  // keys are stored bare (IdString requires \ or $)
		return it == decls->end() ? nullptr : it->second;
	}

	bool lookup_decl_nm(const std::string &name, Cell *&body, std::vector<Wire *> &formals,
			Cell **decl_out = nullptr, SigSpec *raw_out = nullptr)
	{
		if (!decls || inline_depth >= 64)
			return false;
		Cell *decl = decl_by_name(name);
		if (!decl)
			return false;
		if (decl_out)
			*decl_out = decl;
		auto dp = ports_of(decl);
		if (dp.empty())
			return false;
		SigSpec bodysig = decl->getPort(dp[0]);
		body = child_cell(bodysig);
		if (raw_out)
			*raw_out = bodysig;
		formals.clear();
		for (size_t i = 1; i < dp.size(); i++) {
			SigSpec f = decl->getPort(dp[i]);
			formals.push_back(!f.empty() && f[0].is_wire() ? f[0].wire : nullptr);
		}
		return true;
	}

	// Named declaration lookup: instance cells carry the callee name as OP.
	bool lookup_decl(Cell *n, Cell *&body, std::vector<Wire *> &formals,
			Cell **decl_out = nullptr, SigSpec *raw_out = nullptr)
	{
		return lookup_decl_nm(sva_op(n), body, formals, decl_out, raw_out);
	}

	// Lazy per-instance copy of an $sva subtree: ensure_stub creates the
	// shell (fresh Y registered in by_y); cport fills one port of the copy
	// with splice_expr when the walk first reads it, so local reads splice
	// at the age current at that read.  $sva child ports are wired
	// child-stub-Y to child-stub-Y.
	Cell *ensure_stub(Cell *c)
	{
		if (clone_cmap.count(c))
			return clone_cmap[c];
		Cell *nc = mod->addCell(NEW_ID, c->type);
		nc->parameters = c->parameters;
		nc->attributes = c->attributes;
		SigSpec oy = c->getPort(ID(Y));
		if (!oy.empty() && oy[0].is_wire()) {
			Wire *ny = mod->addWire(NEW_ID, oy[0].wire->width);
			nc->setPort(ID(Y), ny);
			by_y[ny] = nc;
		}
		clone_cmap[c] = nc;
		return nc;
	}

	// Rewrite an expression port for the current instance inline: ports
	// referencing local variables are redirected to the clone's fresh local
	// wires (clone_lmap, built per inline_decl) or to the slot read at the
	// current age (clone_locals == false); formal references resolve to
	// actuals; frontend expression cells that transitively depend on either
	// get copied.  Design logic stays shared.
	SigSpec splice_expr(SigSpec port, std::vector<Wire *> &formals, bool clone_locals)
	{
		static int depth_ctr = 0;
		if (++depth_ctr > 500)
			log_error("sva2check: runaway recursion in splice_expr.\n");
		struct DepthGuard { int *c; ~DepthGuard() { --*c; } } dg { &depth_ctr };
		if (port.empty())
			return port;
		pool<Wire *> formal_set;
		for (auto f : formals)
			if (f)
				formal_set.insert(f);
		dict<Wire *, Wire *> &lmap = clone_lmap;
		auto stop_type = [](IdString t) {
			return t.in(ID($sva), ID($dff), ID($dffe), ID($adff), ID($adffe), ID($sdff),
					ID($sdffe), ID($sr), ID($dlatch), ID($check), ID($print), ID($finish),
					ID($stop)) || t.str().rfind("$mem", 0) == 0;
		};
		dict<Cell *, SigSpec> emap;
		pool<Cell *> etried;
		std::function<SigSpec(SigSpec)> cexpr;
		std::function<SigSpec(Cell *)> cexpr_cell = [&](Cell *d) -> SigSpec {
			SigSpec y = d->getPort(ID(Y));
			if (y.empty())
				return y;
			if (emap.count(d))
				return emap[d];
			if (!etried.insert(d).second)
				return y;  // combinational loop: leave shared
			if (stop_type(d->type))
				return y;
			bool changed = false;
			dict<IdString, SigSpec> np;
			for (auto &conn : d->connections()) {
				if (conn.first == ID(Y))
					continue;
				SigSpec n = cexpr(conn.second);
				if (n != conn.second)
					changed = true;
				np[conn.first] = n;
			}
			if (!changed)
				return y;
			Cell *nc = mod->addCell(NEW_ID, d->type);
			nc->parameters = d->parameters;
			nc->attributes = d->attributes;
			Wire *ny = mod->addWire(NEW_ID, y.size());
			nc->setPort(ID(Y), ny);
			for (auto &it : np)
				nc->setPort(it.first, it.second);
			SigSpec nsig = SigSpec(ny, 0, y.size());
			emap[d] = nsig;
			// register the copy so later splices (e.g. the use-site pass in
			// seq_dispatch that turns actual local wires into slot reads)
			// can chase into it
			expr_drv[ny] = nc;
			return nsig;
		};
		cexpr = [&](SigSpec p) -> SigSpec {
			if (p.empty())
				return p;
			// whole port consisting of one formal wire: substitute actual
			Wire *only = nullptr;
			bool single = true;
			for (int i = 0; i < p.size(); i++)
				if (p[i].is_wire()) {
					if (!only)
						only = p[i].wire;
					else if (p[i].wire != only) {
						single = false;
						break;
					}
				} else {
					single = false;
					break;
				}
			if (single && only && formal_set.count(only) && subst.count(only)) {
				SigSpec a = subst[only];
				// a narrower actual (e.g. a 1-bit implicit wire local)
				// still carries the port's full padded width: zero-extend
				// like the frontend's argument padding
				if (a.size() < p.size())
					a.extend_u0(p.size(), false);
				return a;
			}
			SigSpec q;
			bool changed = false;
			for (int i = 0; i < p.size(); i++) {
				SigBit b = p[i];
				if (b.is_wire()) {
					Wire *w = b.wire;
					if (lmap.count(w)) {
						// the body's local variable: read its per-attempt
						// slot at the current age (capture writes the same
						// slot file, keyed by the fresh lmap wire)
						SigSpec rd = local_read(lmap[w]);
						b = rd[std::min(b.offset, rd.size() - 1)];
						changed = true;
					} else if (!clone_locals && slotfiles.count(w)) {
						SigSpec rd = local_read(w);
						b = rd[std::min(b.offset, rd.size() - 1)];
						changed = true;
					} else if (formal_set.count(w) && subst.count(w)) {
						SigSpec a = subst[w];
						if (!a.empty()) {
							b = a[std::min(b.offset, a.size() - 1)];
							changed = true;
						}
					} else {
						Cell *d = expr_drv.count(w) ? expr_drv[w] : nullptr;
						if (d) {
							SigSpec y = d->getPort(ID(Y));
							if (!y.empty() && y[0].wire == w && !stop_type(d->type)) {
								SigSpec ny = cexpr_cell(d);
								if (!ny.empty() && ny != y) {
									b = ny[std::min(b.offset, ny.size() - 1)];
									changed = true;
								}
							}
						}
					}
				}
				q.append(b);
			}
			return changed ? q : p;
		};
		return cexpr(port);
	}

	// Lazily fill one port of n's clone when first read, so expression ports
	// splice local reads at the age current at that read.  Data cells that
	// the walk never visits (bounds, matches children) are always read from
	// the original cell.  Without a clone the original port is returned.
	SigSpec cport(Cell *n, Cell *nc, IdString p)
	{
		if (!clone_scope || !nc)
			return n->getPort(p);
		if (!stubbed_ports[nc].count(p)) {
			stubbed_ports[nc].insert(p);
			SigSpec orig = n->getPort(p);
			Cell *ch = child_cell(orig);
			if (ch)
				nc->setPort(p, ensure_stub(ch)->getPort(ID(Y)));
			else
				nc->setPort(p, splice_expr(orig, *clone_formals, true));
		}
		return nc->getPort(p);
	}

	// Chase an actual argument through width-normalization reductions
	// ($reduce_bool wrappers) and formal substitutions to the underlying
	// local variable wire, WITHOUT reading its slot.  Returns nullptr when
	// the actual is not (a chain leading to) a local.
	Wire *underlying_local(SigSpec a)
	{
		pool<Wire *> seen;
		while (true) {
			Wire *rep = nullptr;
			bool single = true;
			for (int i = 0; i < a.size() && single; i++)
				if (a[i].is_wire()) {
					if (!rep)
						rep = a[i].wire;
					else if (a[i].wire != rep)
						single = false;
				} else if (a[i].data != State::S0)
					single = false;
			if (!single || !rep || !seen.insert(rep).second)
				return nullptr;
			if (slotfiles.count(rep))
				return rep;
			if (clone_lmap.count(rep))
				return clone_lmap[rep];
			if (subst.count(rep)) {
				a = subst[rep];
				continue;
			}
			if (unary_drv.count(rep)) {
				a = unary_drv[rep]->getPort(ID(A));
				continue;
			}
			return nullptr;
		}
	}

	// Actual arguments of a named instance.  A local variable actual stays
	// symbolic: reads inside the instance splice its slot read at the
	// use-site age, which is the only age that means anything across the
	// instance boundary.  Everything else resolves at the call site.
	std::vector<SigSpec> inst_actuals(Cell *inst)
	{
		std::vector<SigSpec> actuals;
		for (auto p : ports_of(inst)) {
			SigSpec a = inst->getPort(p);
			if (Wire *lw = underlying_local(a)) {
				actuals.push_back(SigSpec(lw));
				continue;
			}
			actuals.push_back(resolve(a));
		}
		return actuals;
	}

	struct SubstGuard {
		SvaCompiler *self;
		std::vector<std::pair<Wire *, SigSpec>> saved;
		std::vector<Wire *> added;
		SubstGuard(SvaCompiler *s) : self(s) {}
		void bind(Wire *f, SigSpec a)
		{
			if (!f)
				return;
			if (self->subst.count(f))
				saved.emplace_back(f, self->subst[f]);
			else
				added.push_back(f);
			self->subst[f] = a;
		}
		~SubstGuard()
		{
			for (auto &it : saved)
				self->subst[it.first] = it.second;
			for (auto f : added)
				self->subst.erase(f);
		}
	};

	// Does this subtree sample local variables ("assign" ops)?
	bool tree_has_locals(Cell *n, pool<Cell *> &seen)
	{
		if (!n || !seen.insert(n).second)
			return false;
		if (sva_op(n) == "assign")
			return true;
		for (auto port : ports_of(n)) {
			Cell *c = child_cell(n->getPort(port));
			if (c && tree_has_locals(c, seen))
				return true;
		}
		return false;
	}

	// true when the tree captures local variables OR reads one in an
	// expression port (a wire targeted by an `assign` cell of this tree)
	dict<Cell *, bool> tul_memo;
	pool<Cell *> tul_busy;
	bool tree_uses_locals(Cell *n)
	{
		if (!n)
			return false;
		if (tul_memo.count(n))
			return tul_memo[n];
		if (!tul_busy.insert(n).second)
			return false;
		bool r = false;
		pool<Cell *> seen;
		pool<Wire *> locals;
		std::function<void(Cell *)> scan = [&](Cell *c) {
			if (!c || !seen.insert(c).second)
				return;
			if (sva_op(c) == "assign") {
				SigSpec a = c->getPort(ID(A));
				if (!a.empty() && a[0].is_wire())
					locals.insert(a[0].wire);
			}
			for (auto pr : ports_of(c)) {
				Cell *d = child_cell(c->getPort(pr));
				if (d)
					scan(d);
			}
		};
		scan(n);
		if (!locals.empty()) {
			seen.clear();
			std::function<bool(Cell *)> uses = [&](Cell *c) -> bool {
				if (!c || !seen.insert(c).second)
					return false;
				if (sva_op(c) == "assign")
					return true;
				for (auto pr : ports_of(c)) {
					Cell *d = child_cell(c->getPort(pr));
					if (d && uses(d))
						return true;
					if (!d)
						for (auto bit : c->getPort(pr))
							if (bit.is_wire() && locals.count(bit.wire))
								return true;
				}
				return false;
			};
			r = uses(n);
		}
		tul_busy.erase(n);
		tul_memo[n] = r;
		return r;
	}

	// --- per-attempt local variable slots -------------------------------

	// age vector advanced by one tick: bit j takes bit j-1's signal
	// registered once (bit 0 becomes zero: no attempt is younger than 0)
	SigSpec age_tick(SigSpec a)
	{
		std::vector<SigBit> bits(nslots, Const(0, 1)[0]);
		for (int j = nslots - 1; j >= 1; j--)
			bits[j] = reg(a[j - 1], "age");
		return SigSpec(bits);
	}

	// OR of the age vector shifted by k ticks, for k in [lo, hi]
	SigSpec age_window(SigSpec a, int lo, int hi)
	{
		if (hi > nslots - 1)
			hi = nslots - 1;
		SigSpec acc = Const(0, nslots);
		if (lo == 0)
			acc = a;
		SigSpec cur = a;
		for (int k = 1; k <= hi; k++) {
			cur = age_tick(cur);
			if (k >= lo)
				for (int j = 0; j < nslots; j++) {
					// fold constant-0 bits so the compile-time hotness of
					// the window stays visible (see reg() for role "age")
					if (acc[j] == State::S0)
						acc = sigset(acc, j, cur[j]);
					else if (cur[j] != State::S0)
						acc = sigset(acc, j, mod->Or(NEW_ID, acc[j], cur[j]));
				}
		}
		return acc;
	}

	// enable per-attempt slot mode with an l-slot ring
	void init_slots(int l)
	{
		slots_mode = true;
		split_ranges = true;
		// a 1-slot ring cannot represent an attempt age at all; use at
		// least 2 so age ticking has somewhere to go
		nslots = std::max(2, l);
		age = Const(0, nslots);
		age = sigset(age, 0, Const(1, 1)[0]);
		// memoized reads/addresses depend on nslots and the phase register
		addr_cache.clear();
		read_cache.clear();
		slot_epoch++;
		if (nslots > 1) {
			int w = 1;
			while ((1 << w) < nslots)
				w++;
			Wire *pq = wn("phase", w);
			SigSpec inc = mod->Add(NEW_ID, pq, Const(1, w));
			SigSpec next = mod->Mux(NEW_ID, inc, Const(0, w), mod->Eq(NEW_ID, pq, Const(nslots - 1, w)));
			ffd(next, pq, false);
			phase = pq;
		} else {
			phase = Const(0, 1);
		}
	}

	// The slot address of an attempt whose compile-time age offset is k,
	// at runtime tick with phase == tick mod nslots:
	// addr = (phase - k) mod nslots, returned one-hot.
	SigSpec addr_onehot(int k)
	{
		if (nslots == 1)
			return Const(1, 1);
		if (addr_cache.count(k))
			return addr_cache[k];
		std::vector<SigBit> bits;
		for (int j = 0; j < nslots; j++)
			bits.push_back(mod->Eq(NEW_ID, phase, Const((j + k) % nslots, phase.size())));
		addr_cache[k] = SigSpec(bits);
		return addr_cache[k];
	}

	// the value of xw's slot for the current attempt age, including
	// same-tick pending writes (lowest hot age wins; see slots_mode note)
	SigSpec local_read(Wire *xw)
	{
		if (read_cache.count(xw) && read_cache[xw].first == slot_epoch)
			return read_cache[xw].second;
		if (island_mode) {
			// multi-clock consequent island: the value crossed the clock
			// boundary in a shift chain (stage 0 = value at the handoff
			// pulse, stage k = k consequent ticks later); reads select the
			// stage by the island age vector -- the ring and phase are not
			// reachable from the consequent clock
			auto it = island_chains.find(xw);
			if (it == island_chains.end())
				log_error("sva2check: internal: island read of unknown local %s.\n", log_id(xw));
			auto &chain = it->second;
			SigSpec v = Const(0, chain[0]->width);
			for (int k = (int)chain.size() - 1; k >= 0; k--)
				v = mod->Mux(NEW_ID, v, chain[k], age[k]);
			read_cache[xw] = {slot_epoch, v};
			return v;
		}
		{
			// more than one age can be at this read at compile time: the
			// lowest hot age wins (violations visible only to a younger
			// overlapping attempt can be missed)
			int hot = 0;
			for (auto bit : age)
				if (bit.wire != nullptr || bit.data != State::S0)
					hot++;
			if (hot > 1)
				approx("locals-lowest-hot-age", "local " + std::string(log_id(xw)));
		}
		SlotFile &sf = slotfiles[xw];
		if (sf.q.empty()) {
			// read before any write: materialize an undriven slot file so
			// the value reads x (loud) rather than a wrong constant
			sf.width = std::max(1, sf.width);
			sf.q.resize(nslots);
			sf.wr.resize(nslots);
			for (int k = 0; k < nslots; k++)
				sf.q[k] = wn("slot", sf.width);
		}
		auto eff_slot = [&](int j) -> SigSpec {
			SigSpec d = sf.q[j];
			auto it = pending_wr.find(xw);
			if (it != pending_wr.end())
				d = mod->Mux(NEW_ID, d, it->second.second, it->second.first[j]);
			return d;
		};
		// priority over the hot age bits: value for age k reads the slot at
		// (phase - k) mod nslots
		auto read_for = [&](int k) -> SigSpec {
			SigSpec ad = addr_onehot(k);
			SigSpec v = Const(0, sf.width);
			for (int j = 0; j < nslots; j++)
				v = mod->Mux(NEW_ID, v, eff_slot(j), ad[j]);
			return v;
		};
		// priority over the hot age bits
		SigSpec v = read_for(nslots - 1);
		for (int k = nslots - 2; k >= 0; k--)
			v = mod->Mux(NEW_ID, v, read_for(k), age[k]);
		read_cache[xw] = {slot_epoch, v};
		return v;
	}

	// Sample a local variable: write the current expression into the slot
	// of every in-flight attempt age (we[k] = fire & age[k]; the slot is
	// addressed by (phase - k) mod nslots), with same-tick chaining visible
	// to later reads at this age.
	// Sample a local variable: write expr into the slot of every in-flight
	// attempt age (we[k] = fire & age[k], slot addressed by (phase - k) mod
	// nslots), with same-tick chaining visible to later reads at this age.
	// The cell is the (possibly cloned) assign node: its target port is the
	// local's wire verbatim (in an instance body, clone_lmap's fresh wire),
	// never resolved through the slot file.
	void capture_local(Cell *a, SigSpec fire)
	{
		auto ap = ports_of(a);
		if (GetSize(ap) < 2)
			return;
		if (island_mode)
			log_error("sva2check: local variable assignments in a multi-clock consequent are not supported in v1 (%s).\n", log_id(a));
		SigSpec xs = a->getPort(ap[0]);
		// the target names the body-declared local; inside an instance its
		// slot file is keyed by the fresh lmap wire
		if (!xs.empty() && xs[0].is_wire() && clone_lmap.count(xs[0].wire))
			xs = SigSpec(clone_lmap[xs[0].wire]);
		SigSpec e = resolve(a->getPort(ap[1]));
		if (xs.empty() || !xs[0].is_wire())
			return;
		Wire *xw = xs[0].wire;
		if (!slots_mode)
			log_error("sva2check: internal: local variable capture outside slots mode in %s.\n", log_id(a));
		// the sampled expression may read other locals (chained assigns):
		// splice slot reads into its cone at this age
		{
			static std::vector<Wire *> no_formals;
			e = splice_expr(e, no_formals, false);
		}
		int w = std::max(1, e.size());
		if (e.size() < w)
			e.extend_u0(w, false);
		SlotFile &sf = slotfiles[xw];
		if (sf.q.empty()) {
			sf.width = w;
			sf.q.resize(nslots);
			sf.dff.resize(nslots);
			sf.wr.resize(nslots);
			for (int k = 0; k < nslots; k++)
				sf.q[k] = wn("slot", w);
		}
		if (sf.width != w)
			log_error("sva2check: local variable %s re-sampled at a different width in %s.\n", log_id(xw), log_id(a));
		// per-slot write enable: fire & age[k], addressed by (phase - k)
		std::vector<SigBit> we_b(nslots, Const(0, 1)[0]);
		for (int k = 0; k < nslots; k++) {
			SigSpec hot = andN({fire, age[k]});
			SigSpec ad = addr_onehot(k);
			for (int j = 0; j < nslots; j++)
				we_b[j] = mod->Or(NEW_ID, we_b[j], mod->And(NEW_ID, hot, ad[j]));
		}
		SigSpec we = SigSpec(we_b);
		for (int k = 0; k < nslots; k++)
			sf.wr[k].push_back({we[k], e});
		// rebuild each slot's register: next = q, then apply writes in order
		for (int k = 0; k < nslots; k++) {
			SigSpec next = sf.q[k];
			for (auto &wr : sf.wr[k])
				next = mod->Mux(NEW_ID, next, wr.second, wr.first);
			if (sf.dff[k])
				sf.dff[k]->setPort(ID(D), next);
			else
				sf.dff[k] = ffd(next, sf.q[k], false);
		}
		pending_wr[xw] = {we, e};
		slot_epoch++;
	}

	// --- empty matches (LRM 16.9.2.1, register v3 E1) ----------------------
	// An empty match has no tick: (empty ##n s) = ##(n-1) s and
	// (s ##n empty) = s ##(n-1) `true for n > 0, and ##0 next to an empty
	// match does not match.  Elements never emit an end for their empty
	// match; every ## whose operand can match empty is rewritten once per
	// module (before any check compiles) into the union of its nonempty
	// parse and the shifted forms above.

	// can the sequence rooted at c match empty?  [*0..], [=0..], [->0..],
	// and `or` / `throughout` over such; a ## never does (n >= 1 shifts
	// to ##(n-1) of the other side, which is nonempty or ##0-dead)
	bool can_empty(Cell *c)
	{
		if (!c || c->type != "\\$sva")
			return false;
		std::string op = sva_op(c);
		auto p = ports_of(c);
		if (op == "[*]" || op == "[->]" || op == "[=]") {
			int m = op == "[*]" ? 0 : 1;
			Cell *b = p.size() >= 2 ? child_cell(c->getPort(p[1])) : nullptr;
			if (b && sva_op(b) == "bounds") {
				auto bp = ports_of(b);
				if (bp.empty() || !const_int(b->getPort(bp[0]), m))
					return false;
			}
			return m == 0;
		}
		if (op == "or" && p.size() >= 2)
			return can_empty(child_cell(c->getPort(p[0]))) || can_empty(child_cell(c->getPort(p[1])));
		if (op == "throughout" && p.size() >= 2)
			return can_empty(child_cell(c->getPort(p[1])));
		return false;
	}

	SigSpec sva_ref(Cell *c)
	{
		SigSpec s = c->getPort(ID(Y));
		s.append(Const(0, 31));
		return s;
	}

	Cell *new_sva(const std::string &op, std::vector<SigSpec> args, Cell *like)
	{
		Cell *c = mod->addCell(NEW_ID, "\\$sva");
		c->setParam(ID(OP), Const(op));
		c->setParam(ID(NARGS), Const(GetSize(args), 32));
		static const IdString arg_ports[] = {ID(A), ID(B), ID(C), ID(D), ID(E)};
		for (int i = 0; i < 5; i++) {
			SigSpec a = i < GetSize(args) ? args[i] : SigSpec(Const(0, 32));
			if (a.size() < 32)
				a.append(Const(0, 32 - a.size()));
			c->setPort(arg_ports[i], a);
		}
		Wire *y = mod->addWire(NEW_ID, 1);
		c->setPort(ID(Y), y);
		by_y[SigBit(y, 0)] = c;
		if (like && like->has_attribute(ID::src))
			c->attributes[ID::src] = like->attributes.at(ID::src);
		return c;
	}

	// the cycle operand of a ## for [lo:hi] (hi < 0: $)
	SigSpec mk_cyc(int lo, int hi, Cell *like)
	{
		if (lo == hi)
			return Const(lo, 32);
		return sva_ref(new_sva("bounds", {Const(lo, 32), Const(hi, 32)}, like));
	}

	// the nonempty matches of sequence operand s (s itself when it
	// cannot match empty); empty SigSpec when it has none
	SigSpec ne_seq(SigSpec s)
	{
		Cell *c = child_cell(s);
		if (!c || !can_empty(c))
			return s;
		std::string op = sva_op(c);
		auto p = ports_of(c);
		if (op == "[*]" || op == "[->]" || op == "[=]") {
			int hi = -1;
			Cell *b = p.size() >= 2 ? child_cell(c->getPort(p[1])) : nullptr;
			if (b && sva_op(b) == "bounds") {
				auto bp = ports_of(b);
				if (bp.size() < 2)
					hi = 0;   // [*0]: the single bound
				else if (!const_int(b->getPort(bp[1]), hi))
					return s;
			}
			if (hi == 0)
				return SigSpec();
			// the count-set lowering of [->0:n] / [=0:n] already ends
			// only at ticks (its [=] tail needs a !b tick); [*0:n] /
			// [*] become [*1:n] / [*1:$]
			if (op != "[*]")
				return s;
			Cell *nb = new_sva("bounds", {Const(1, 32), Const(hi, 32)}, c);
			return sva_ref(new_sva("[*]", {c->getPort(p[0]), sva_ref(nb)}, c));
		}
		if (op == "or") {
			SigSpec a = ne_seq(c->getPort(p[0])), b = ne_seq(c->getPort(p[1]));
			if (a.empty()) return b;
			if (b.empty()) return a;
			return sva_ref(new_sva("or", {a, b}, c));
		}
		if (op == "throughout") {
			SigSpec b = ne_seq(c->getPort(p[1]));
			if (b.empty()) return b;
			return sva_ref(new_sva("throughout", {c->getPort(p[0]), b}, c));
		}
		return s;
	}

	void rewrite_empty_delays()
	{
		std::vector<Cell *> work;
		for (auto c : mod->cells())
			if (c->type == "\\$sva" && sva_op(c) == "##")
				work.push_back(c);
		for (auto n : work) {
			auto p = ports_of(n);
			bool lead2 = GetSize(p) == 2;
			SigSpec H = lead2 ? SigSpec() : n->getPort(p[0]);
			SigSpec cyc = n->getPort(p[lead2 ? 0 : 1]);
			SigSpec T = n->getPort(p[lead2 ? 1 : 2]);
			bool hE = !lead2 && can_empty(child_cell(H));
			bool tE = can_empty(child_cell(T));
			if (!hE && !tE)
				continue;
			if (hE && tE)
				log_error("sva2check: both operands of the ## at %s can match empty; not supported.\n", log_id(n));
			int lo = 0, hi = -1;
			Cell *cc = child_cell(cyc);
			if (cc && (sva_op(cc) == "##[*]" || sva_op(cc) == "##[+]")) {
				lo = sva_op(cc) == "##[+]" ? 1 : 0;
			} else if (cc && sva_op(cc) == "bounds") {
				auto bp = ports_of(cc);
				if (bp.empty() || !const_int(cc->getPort(bp[0]), lo))
					continue;
				if (bp.size() < 2)
					hi = lo;
				else if (!const_int(cc->getPort(bp[1]), hi))
					continue;
			} else if (!cc && const_int(cyc, lo)) {
				hi = lo;
			} else {
				continue;  // the lowering reports a malformed delay
			}
			std::vector<SigSpec> alts;
			SigSpec Hn = hE ? ne_seq(H) : H, Tn = tE ? ne_seq(T) : T;
			if ((lead2 || !Hn.empty()) && !Tn.empty())
				alts.push_back(sva_ref(lead2 ? new_sva("##", {cyc, Tn}, n) : new_sva("##", {Hn, cyc, Tn}, n)));
			if (hi < 0 || hi >= 1) {
				SigSpec scyc = mk_cyc(std::max(lo, 1) - 1, hi < 0 ? -1 : hi - 1, n);
				if (tE)   // (s ##k empty) = s ##(k-1) 1; a leading ##k = 1 ##k
					alts.push_back(sva_ref(new_sva("##", {lead2 ? SigSpec(Const(1, 32)) : H, scyc, Const(1, 32)}, n)));
				if (hE)   // (empty ##k s) = ##(k-1) s
					alts.push_back(sva_ref(new_sva("##", {scyc, T}, n)));
			}
			// n becomes the union in place (its parents keep their port)
			SigSpec a0 = alts.empty() ? SigSpec(Const(0, 32)) : alts[0];
			SigSpec rest = alts.size() <= 1 ? a0 : alts[1];
			for (size_t i = 2; i < alts.size(); i++)
				rest = sva_ref(new_sva("or", {rest, alts[i]}, n));
			n->setParam(ID(OP), Const(std::string("or")));
			n->setParam(ID(NARGS), Const(2, 32));
			n->setPort(ID(A), a0);
			n->setPort(ID(B), rest);
			n->setPort(ID(C), Const(0, 32));
			n->setPort(ID(D), Const(0, 32));
			n->setPort(ID(E), Const(0, 32));
		}
	}

	// --- sequences ---------------------------------------------------------

	// Compile a named declaration body (sequence/property/let) with actuals
	// bound to formals.  in_prop: compile as a property (prop_fail);
	// sequence declarations in property position must complete strongly.
	// Bare expression bodies (let) are rewritten through clone_tree's
	// expression path so formals resolve.
	SigSpec inline_decl(Cell *decl, std::vector<SigSpec> actuals, bool in_prop,
			SigSpec start, SigSpec gate)
	{
		if (inline_depth >= 64)
			log_error("sva2check: recursive property instantiation involving %s.\n", log_id(decl));
		std::string dop = sva_op(decl);
		Cell *dbody = nullptr;
		std::vector<Wire *> dformals;
		SigSpec draw;
		auto dp = ports_of(decl);
		if (!dp.empty()) {
			draw = decl->getPort(dp[0]);
			dbody = child_cell(draw);
			for (size_t i = 1; i < dp.size(); i++) {
				SigSpec f = decl->getPort(dp[i]);
				dformals.push_back(!f.empty() && f[0].is_wire() ? f[0].wire : nullptr);
			}
		}
		if (GetSize(actuals) != GetSize(dformals))
			log_error("sva2check: instance of %s has %d actuals, declaration has %d formals.\n",
					dop.c_str(), GetSize(actuals), GetSize(dformals));
		SubstGuard guard(this);
		for (int i = 0; i < GetSize(dformals); i++)
			guard.bind(dformals[i], actuals[i]);
		SigSpec bare;
		dbody = dbody ? unwrap_spec(dbody, &bare) : nullptr;
		SigSpec bsig;
		// lazy per-node copy of the instance body: nodes are copied as the
		// walk reaches them and each expression port is filled when first
		// read, so local reads splice at the age current at the read.  The
		// callee's locals need slots even when the caller has none.
		bool had_slots = slots_mode;
		if (dbody && !slots_mode) {
			pool<Cell *> seen;
			if (tree_has_locals(dbody, seen)) {
				int l = seq_max_len(dbody);
				if (l < 0 || l > 64)
					log_error("sva2check: local variables over an unbounded sequence in %s.\n", log_id(dbody));
				init_slots(l);
			}
		}
		Cell *saved_scope = clone_scope;
		dict<Cell *, Cell *> saved_cmap;
		dict<Cell *, pool<IdString>> saved_stubbed;
		std::vector<Wire *> *saved_formals = clone_formals;
		dict<Wire *, Wire *> saved_lmap;
		saved_cmap.swap(clone_cmap);
		saved_stubbed.swap(stubbed_ports);
		saved_lmap.swap(clone_lmap);
		clone_cmap.clear();
		stubbed_ports.clear();
		clone_lmap.clear();
		clone_scope = dbody ? dbody : nullptr;
		clone_formals = &dformals;
		if (dbody) {
			// fresh wire per local variable of this body: the slot file is
			// keyed by it, so distinct instance sites never share slots
			pool<Cell *> lseen;
			std::function<void(Cell *)> lscan = [&](Cell *c) {
				if (!c || !lseen.insert(c).second)
					return;
				if (sva_op(c) == "assign") {
					SigSpec xs = c->getPort(ID(A));
					if (!xs.empty() && xs[0].is_wire() && !clone_lmap.count(xs[0].wire))
						clone_lmap[xs[0].wire] = mod->addWire(NEW_ID, xs[0].wire->width);
				}
				for (auto pr : ports_of(c)) {
					Cell *ch = child_cell(c->getPort(pr));
					if (ch)
						lscan(ch);
				}
			};
			lscan(dbody);
		}
		if (dbody)
			ensure_stub(dbody);
		else {
			if (bare.empty())
				bare = draw;
			if (!bare.empty())
				bsig = splice_expr(bare, dformals, false);
		}
		inline_depth++;
		SigSpec r;
		if (in_prop) {
			if (dbody && dop == "sequence_decl")
				r = strong_seq_fail(dbody, start);
			else if (dbody)
				r = prop_fail(dbody, start);
			else if (!bsig.empty())
				r = andN({start, mod->Not(NEW_ID, reduce1(bsig))});
			else
				r = andN({start, mod->Not(NEW_ID, Const(1, 1))});
		} else {
			r = dbody ? seq_end(dbody, start, gate)
			          : (!bsig.empty() ? seq_dispatch(bsig, nullptr, start, gate)
			                           : andN({start, gate}));
		}
		inline_depth--;
		clone_scope = saved_scope;
		clone_cmap.swap(saved_cmap);
		stubbed_ports.swap(saved_stubbed);
		clone_lmap.swap(saved_lmap);
		clone_formals = saved_formals;
		if (!had_slots && slots_mode) {
			slots_mode = false;
			nslots = 0;
			age = SigSpec();
			slotfiles.clear();
			pending_wr.clear();
		}
		return r;
	}

	// A plain wire naming a declaration (bare `foo` reference: the frontend
	// emits a plain identifier wire rather than an instance cell).  The
	// port may be zero-padded to 32 bits.
	Cell *decl_for_wire(SigSpec s)
	{
		Wire *rep = nullptr;
		for (int i = 0; i < s.size(); i++)
			if (s[i].is_wire()) {
				if (!rep)
					rep = s[i].wire;
				else if (s[i].wire != rep)
					return nullptr;
			} else if (s[i].data != State::S0)
				return nullptr;
		return rep ? decl_by_name(rep->name.str()) : nullptr;
	}

	// Exact per-offset compile of a `lhs ##[lo:hi] tail` chain under
	// split_ranges: returns the end pulses decomposed per parse age, each
	// with its age offset relative to the incoming member age, so a tail
	// that uses local variables can be re-compiled per parse at a
	// single-hot age (fresh stubs force re-splicing) and read the slot of
	// the tick its parse is actually at.  `exact` false means some
	// subsequence could not be decomposed (variable-length non-repetition
	// or budget exhaustion); the caller then falls back to the merged
	// multi-hot window (documented approximation).
	struct SvaEnd { SigSpec end; int age; };
	std::pair<std::vector<SvaEnd>, bool> seq_ends_split(Cell *n, SigSpec nport,
			SigSpec start, SigSpec gate, int depth = 0)
	{
		bool outer = !in_split;
		if (outer)
			in_split = true;
		auto finish = [&](std::pair<std::vector<SvaEnd>, bool> r)
				-> std::pair<std::vector<SvaEnd>, bool> {
			if (outer)
				in_split = false;
			return r;
		};
		if (!n) {
			static std::vector<Wire *> no_formals;
			SigSpec e = andN({start, gate_now(gate),
					reduce1(splice_expr(nport, no_formals, false))});
			return finish({{SvaEnd{e, 0}}, true});
		}
		if (depth > 64 || split_budget <= 0)
			return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
		std::string op = sva_op(n);
		auto ports = ports_of(n);
		if (clone_scope && !clone_cmap.count(n))
			ensure_stub(n);
		Cell *srcn = clone_scope ? clone_cmap[n] : nullptr;
		auto port = [&](int i) -> SigSpec {
			return i < GetSize(ports) ? cport(n, srcn, ports[i]) : SigSpec();
		};
		auto child = [&](int i) -> Cell * {
			return i < GetSize(ports) ? child_cell(n->getPort(ports[i])) : nullptr;
		};
		if (op == "##") {
			// a two-port ## is the leading-delay form `@(c) ##k body`
			// (IEEE 16.5): an implicit true lhs occupies the resample
			// tick, so the ports are (cycle, tail); normalize to the
			// three-port (lhs, cycle, tail) view
			bool lead2 = GetSize(ports) == 2;
			SigSpec cyc = n->getPort(ports[lead2 ? 0 : 1]);
			Cell *cyc_cell = child_cell(cyc);
			std::string cop = cyc_cell ? sva_op(cyc_cell) : "";
			int lo = 0, hi = -1;
			if (cyc_cell && (cop == "##[*]" || cop == "##[+]")) {
				lo = cop == "##[+]" ? 1 : 0;
			} else if (cyc_cell && cop == "bounds") {
				auto bp = ports_of(cyc_cell);
				if (!const_int(cyc_cell->getPort(bp[0]), lo))
					return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
				if (GetSize(bp) >= 2 && !const_int(cyc_cell->getPort(bp[1]), hi))
					return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
			} else if (cyc_cell) {
				return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
			} else {
				int k;
				if (!const_int(cyc, k) || k < 0)
					return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
				lo = hi = k;
			}
			if (hi < 0 || hi > 64 || hi - lo + 1 > split_budget)
				return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
			std::pair<std::vector<SvaEnd>, bool> lhs;
			if (lead2)
				lhs = {{SvaEnd{andN({start, gate_now(gate)}), 0}}, true};
			else {
				lhs = seq_ends_split(child(0), port(0), start, gate, depth + 1);
				if (!lhs.second)
					return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
			}
			split_budget -= hi - lo + 1;
			std::vector<SvaEnd> r;
			SigSpec entry_age = age;
			dict<Cell *, Cell *> saved_cmap = clone_cmap;
			dict<Cell *, pool<IdString>> saved_stubbed = stubbed_ports;
			dict<Wire *, std::pair<SigSpec, SigSpec>> saved_pending = pending_wr;
			bool ok = true;
			size_t n_compiles = 0;
			for (auto &le : lhs.first) {
				for (int k = lo; k <= hi && ok; k++) {
					if (++n_compiles > 64) { ok = false; break; }
					if (n_compiles > 1) {
						// fresh stubs: tail ports re-splice at this
						// parse's age
						clone_cmap.clear();
						stubbed_ports.clear();
					}
					age = entry_age;
					for (int j = 0; j < le.age + k; j++)
						age = age_tick(age);
					slot_epoch++;
					pending_wr.clear();
					auto te = seq_ends_split(child(lead2 ? 1 : 2), port(lead2 ? 1 : 2),
							k == 0 ? le.end : gap_delay(le.end, k, gate, "dspl"), gate, depth + 1);
					if (!te.second) { ok = false; break; }
					for (auto &t : te.first)
						r.push_back(SvaEnd{t.end, le.age + k + t.age});
				}
			}
			clone_cmap = saved_cmap;
			stubbed_ports = saved_stubbed;
			pending_wr = saved_pending;
			age = entry_age;
			if (!ok || r.empty())
				return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
			return finish({r, true});
		}
		if (op == "or") {
			auto a = seq_ends_split(child(0), port(0), start, gate, depth + 1);
			auto b = seq_ends_split(child(1), port(1), start, gate, depth + 1);
			if (!a.second || !b.second)
				return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
			a.first.insert(a.first.end(), b.first.begin(), b.first.end());
			return finish({a.first, true});
		}
		if (op == "[*]" || op == "[+]") {
			Cell *b = child(1);
			int m = op == "[*]" ? 0 : 1, hi = -1;
			if (b && sva_op(b) == "bounds") {
				auto bp = ports_of(b);
				int lo_v = 0;
				if (!bp.empty() && !const_int(b->getPort(bp[0]), lo_v))
					return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
				m = lo_v;
				if (GetSize(bp) >= 2 && !const_int(b->getPort(bp[1]), hi))
					return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
			}
			if (hi < 0 || hi > 8 || hi > split_budget)
				return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
			Cell *bc = child(0);
			int bl = bc ? seq_max_len(bc) : 1;
			int blmin = bc ? seq_min_len(bc) : 1;
			if (bc && (bl < 0 || blmin != bl))
				return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
			if (!bc) bl = 1;
			std::vector<SigSpec> per_copy;
			rep_chain_end(bc, n->getPort(ports[0]), start, gate, 1, hi, &per_copy);
			split_budget -= hi;
			std::vector<SvaEnd> r;
			// no empty end: an empty match has no tick (LRM 16.9.2.1); the
			// enclosing ## accounts for it (rewrite_empty_delays)
			for (int c = std::max(m, 1); c <= hi; c++)
				r.push_back(SvaEnd{per_copy[c - 1], c * bl - 1});
			return finish({r, true});
		}
		if (op == "throughout") {
			SigSpec saved = dyn_gate;
			dyn_gate = n->getPort(ports[0]);
			auto r = seq_ends_split(child(1), port(1), start, gate, depth + 1);
			dyn_gate = saved;
			return finish(r);
		}
		if (op == "clocked") {
			// a different inner clock is only supported as the direct
			// consequent of |->/|=> (compiled in prop_fail) in v1
			if (clocked_differs(n))
				log_error("sva2check: multi-clock sequence element only supported as the direct consequent of |->/|=> in v1 (%s).\n", log_id(n));
			return finish(seq_ends_split(child(1), port(1), start, gate, depth + 1));
		}
		int lmin = seq_min_len(n), lmax = seq_max_len(n);
		if (lmin >= 0 && lmin == lmax && lmax <= 64)
			return finish({{SvaEnd{seq_end(n, start, gate), lmax - 1}}, true});
		return finish({{SvaEnd{seq_end(n, start, gate), -1}}, false});
	}

	// end pulse: "some parse of this sequence, started by `start`, gated to
	// ticks where `gate` holds, ends exactly at the current tick".
	SigSpec seq_end(Cell *n, SigSpec start, SigSpec gate)
	{
		static int depth_ctr = 0;
		if (++depth_ctr > 200) {
			fprintf(stderr, "RUNAWAY seq_end\n");
			abort();
		}
		struct DepthGuard { int *c; ~DepthGuard() { --*c; } } dg { &depth_ctr };
		std::string op = sva_op(n);
		auto ports = ports_of(n);
		// expression ports read from the node's clone, filled lazily at the
		// age current when each port is first read; children stay on the
		// original cell.  Self-heal the stub: parents wire children lazily.
		if (clone_scope && !clone_cmap.count(n))
			ensure_stub(n);
		Cell *srcn = clone_scope ? clone_cmap[n] : nullptr;
		auto port = [&](int i) -> SigSpec {
			return i < GetSize(ports) ? cport(n, srcn, ports[i]) : SigSpec();
		};
		auto child = [&](int i) -> Cell * {
			return i < GetSize(ports) ? child_cell(n->getPort(ports[i])) : nullptr;
		};

		// named sequence instance: inline the declaration body
		if (Cell *decl = decl_by_name(op)) {
			auto actuals = inst_actuals(n);
			return inline_decl(decl, actuals, false, start, gate);
		}

		if (op == "true")
			return andN({start, gate_now(gate)});

		if (op == "##") {
			// a two-port ## is the leading-delay form `@(c) ##k body`
			// (IEEE 16.5): an implicit true lhs occupies the resample
			// tick, so the ports are (cycle, tail); normalize to the
			// three-port (lhs, cycle, tail) view
			bool lead2 = GetSize(ports) == 2;
			int ti = lead2 ? 1 : 2;
			SigSpec s_end = lead2 ? andN({start, gate_now(gate)})
			                      : seq_dispatch(port(0), child(0), start, gate);
			// the cycle count lives on the original cell (a bounds child is
			// a data cell the walk never visits; its clone is a shell)
			SigSpec cyc = n->getPort(ports[lead2 ? 0 : 1]);
			Cell *cyc_cell = child_cell(cyc);
			std::string cop = cyc_cell ? sva_op(cyc_cell) : "";
			SigSpec t_start;
			SigSpec t_age;
			int lo = 0, hi = -1;
			if (cyc_cell && (cop == "##[*]" || cop == "##[+]")) {
				lo = cop == "##[+]" ? 1 : 0;
				hi = -1;
			} else if (cyc_cell && cop == "bounds") {
				auto bp = ports_of(cyc_cell);
				if (!const_int(cyc_cell->getPort(bp[0]), lo))
					log_error("sva2check: non-constant ## bound in %s.\n", log_id(n));
				if (GetSize(bp) >= 2 && !const_int(cyc_cell->getPort(bp[1]), hi))
					log_error("sva2check: non-constant ## bound in %s.\n", log_id(n));
			} else if (cyc_cell) {
				log_error("sva2check: unsupported ## cycle operator '%s' in %s.\n", cop.c_str(), log_id(n));
			} else {
				int k;
				if (!const_int(cyc, k) || k < 0)
					log_error("sva2check: non-constant ## delay in %s.\n", log_id(n));
				lo = hi = k;
			}
			// Inside `b throughout (... ##k ...)` b must hold at EVERY tick
			// of the match (LRM 16.9.9), the gap ticks between the lhs
			// end and the tail start included; the elements gate their
			// own ticks, the register chain carrying the token across the
			// gap did not, so `g throughout (1 ##5 x)` was PROVED with g
			// low inside the gap (register v3 C1, sv-tests 16.7).  When a
			// throughout gate is active, the token leaving a gap tick
			// must see the gate there.
			bool gap_gate = !dyn_gate.empty();
			SigSpec g_gap = gap_gate ? gate_now(gate) : SigSpec();
			auto step = [&](SigSpec tok, bool at_gap) -> SigSpec {
				return reg(at_gap && gap_gate ? andN({tok, g_gap}) : tok, "delay");
			};
			if (hi < 0) {
				// unbounded: tail may start at any tick >= end + lo
				SigSpec d = s_end;
				for (int k = 1; k <= lo; k++)
					d = step(d, k > 1);
				if (lo >= 1 && gap_gate) {
					// waiting past end+lo: every waited tick is a gap tick
					t_start = feedback([=](SigSpec q) {
						return andN({orN({d, q}), g_gap});
					}, "delay_trig", 1);
					t_start = orN({d, t_start});
				} else {
					SigSpec dd = delay(s_end, lo);
					t_start = feedback([=](SigSpec q) {
						return orN({dd, q});
					}, "delay_trig", 1);
				}
				if (slots_mode)
					t_age = age_window(age, lo, nslots - 1);
			} else if (hi <= 128) {
				// single register chain with one tap per admissible offset
				t_start = lo == 0 ? s_end : Const(0, 1);
				SigSpec tap = s_end;
				for (int k = 1; k <= hi; k++) {
					tap = step(tap, k > 1);
					if (k >= lo)
						t_start = orN({t_start, tap});
				}
				if (slots_mode)
					t_age = age_window(age, lo, hi);
			} else {
				log_error("sva2check: ## delay bound %d too large in %s.\n", hi, log_id(n));
			}
			if (!slots_mode)
				return seq_dispatch(port(ti), child(ti), t_start, gate);
			// the tail is one or more ticks later: ages shift, same-tick
			// pending writes expire
			SigSpec saved_age = age;
			dict<Wire *, std::pair<SigSpec, SigSpec>> saved_pending = pending_wr;

			// Exact per-offset compile under split_ranges when the delay
			// is ranged, the tail may touch local variables, or the LHS
			// spans a variable number of ticks: end pulses come back
			// decomposed per parse age.  Falls back to the merged window
			// below when a subsequence cannot be decomposed (documented
			// approximation).
			if (split_ranges && hi >= 0 && !in_split) {
				Cell *tailc = child(ti);
				auto sig_uses_locals = [&](SigSpec s) {
					for (auto bit : s)
						if (bit.is_wire() && (slotfiles.count(bit.wire) || clone_lmap.count(bit.wire)))
							return true;
					return false;
				};
				Cell *lhsc = lead2 ? nullptr : child(0);
				bool tailu = (tailc && tree_uses_locals(tailc)) ||
						sig_uses_locals(n->getPort(ports[ti]));
				bool lhsvar = false;
				if (lhsc) {
					int l0 = seq_min_len(lhsc), l1 = seq_max_len(lhsc);
					lhsvar = l0 < 0 || l0 != l1;
				}
				if (tailu || hi > lo || lhsvar) {
					// SVA_NOSPLIT=1 skips the exact split (positive control
					// for the merged-window detector below)
					if (getenv("SVA_NOSPLIT") == nullptr) {
						auto sp = seq_ends_split(n, SigSpec(), start, gate);
						if (sp.second) {
							SigSpec r = Const(0, 1);
							for (auto &e : sp.first)
								r = orN({r, e.end});
							return r;
						}
					}
					if (tailu)
						approx("locals-merged-window", "## at " + std::string(log_id(n)));
				}
			}

			age = t_age;
			slot_epoch++;
			if (lo > 0 || hi > 0)
				pending_wr.clear();  // the tail is a later tick
			SigSpec r = seq_dispatch(port(ti), child(ti), t_start, gate);
			age = saved_age;
			pending_wr = saved_pending;
			return r;
		}

		if (op == "##[*]" || op == "##[+]") {
			// bare unbounded delay at sequence start: equivalent to
			// true ##[*] body handled by caller; treat as "true"
			return andN({start, gate_now(gate)});
		}

		if (op == "[*]" || op == "[+]" || op == "[->]" || op == "[=]") {
			// repetition of a sequence element; bounds in port B ("bounds")
			SigSpec body = port(0);
			// the ORIGINAL child: inside an inlined declaration port(0) is
			// the clone stub's Y, and a stub's ports fill lazily from its
			// original (seq_end on the stub read a missing port: abort on
			// `sequence s; (a ##1 b)[*2]; endsequence`)
			Cell *body_cell = child(0);
			int m = op == "[*]" ? 0 : 1, hi = -1; // hi < 0 = unbounded
			Cell *b = child(1);
			if (b) {
				if (sva_op(b) != "bounds")
					log_error("sva2check: malformed repetition bounds in %s.\n", log_id(n));
				auto bp = ports_of(b);
				int lo_v;
				if (!bp.empty() && !const_int(b->getPort(bp[0]), lo_v))
					log_error("sva2check: non-constant repetition bound in %s.\n", log_id(n));
				m = lo_v;
				if (GetSize(bp) >= 2) {
					int hi_v;
					if (!const_int(b->getPort(bp[1]), hi_v))
						log_error("sva2check: non-constant repetition bound in %s.\n", log_id(n));
					hi = hi_v;
				} else {
					// single bound: exactly m (LRM F.3.4.2.3 derives
					// [->m]/[=m]/[*m] from [*m] == [*m:m]; for boolean
					// [*]-bodies the run form coincides, but [->]/[=] and
					// sequence bodies need the exact window)
					hi = m;
				}
			}
			if (op == "[*]" || op == "[+]") {
				if (m == 0 && hi < 0) {
					// [*] = [*0:$]: the nonempty runs.  The empty match has
					// no tick of its own (LRM 16.9.2.1): an enclosing ##
					// accounts for it (rewrite_empty_delays); an implication
					// antecedent ignores it (|->) or starts the consequent at
					// the attempt tick (|=>); a sequence property must not
					// admit it (16.13.22).  It used to end AT the start tick,
					// one tick late inside ## (`a ##1 x[*0:1] ##1 b` missed
					// b at a+1).
					return rep_run_end(body_cell, body, start, gate, 1);
				}
				if (hi >= 0 && hi <= 8) {
					// bounded: chain hi copies, tap ends at >= max(m,1);
					// [*0:N]'s empty match is handled as for [*] above
					if (hi == 0)
						return Const(0, 1);  // [*0]: only the empty match
					return rep_chain_end(body_cell, body, start, gate, std::max(m, 1), hi);
				}
				// [*m:$] / [*m:hi] past the chain budget: exact run-length
				// set for boolean bodies; s[+] (= s[*1:$]) feeds copies back
				if (body_cell && sva_op(body_cell) != "true" && !(hi < 0 && m <= 1))
					log_error("sva2check: repetition [*%d:%s] of a sequence in %s; bound it to <= 8.\n",
							m, hi < 0 ? "$" : std::to_string(hi).c_str(), log_id(n));
				return rep_run_end(body_cell, body, start, gate, std::max(m, 1), hi);
			}
			// non-contiguous repetitions of a boolean element or a
			// bounded sequence: the counter below counts OCCURRENCES.
			// For a sequence body the occurrence pulse is its match end
			// with an attempt every tick (overlapping attempts merge,
			// the documented approximation); a sequence whose own length
			// is unbounded is rejected by the outer length machinery.
			SigSpec bsig;
			if (body_cell && sva_op(body_cell) != "true") {
				if (tree_uses_locals(body_cell))
					log_error("sva2check: local variables in a non-contiguous repetition are not supported in %s.\n", log_id(n));
				approx("noncons-rep-seq-body-merged", std::string(op) + " at " + log_id(n));
				bsig = seq_end(body_cell, Const(1, 1), gate);
			} else {
				bsig = andN({reduce1(body), gate_now(gate)});
			}
			bool every_tick = start.is_fully_const() && start.as_const() == Const(1, 1);
			// Exact occurrence-count set (LRM 16.9.2: b[->m:n] =
			// (!b[*0:$] ##1 b)[*m:n], b[=m:n] = b[->m:n] ##1 !b[*0:$]).
			// In-flight attempts of the element differ only in how many
			// occurrences each has seen, so bit c of the state says "some
			// live attempt has seen c occurrences before this tick"; an
			// attempt entering now has seen 0.  An occurrence moves every
			// attempt up one count (counts past n leave; with no upper
			// bound the top count saturates).  The ends are the union over
			// attempts, like every other sequence end.  (The anchored
			// snapshot ring this replaces read the slot of the attempt
			// starting at the END tick instead of the one that entered the
			// element, and its ring sizing took `s ##0 b[->n]` for bounded:
			// both a false proof and a false disproof, tagged exact.)
			int gk = std::max(m, 1);                   // goto ends: c+1 in [gk, hi]
			int K = op == "[->]" ? (hi >= 0 ? hi : gk) - 1 : (hi >= 0 ? hi : std::max(m, 1));
			if (K >= 0 && K <= 64) {
				SigSpec g = gate_now(gate);
				Wire *sw = wn("occ_set", K + 1);
				std::vector<SigBit> cur;
				for (int c = 0; c <= K; c++)
					cur.push_back(c == 0 ? mod->Or(NEW_ID, SigSpec(sw, 0, 1), start)[0] : SigBit(sw, c));
				auto any_in = [&](int lo, int hiq) {
					SigSpec acc = Const(0, 1);
					for (int c = std::max(lo, 0); c <= std::min(hiq, K); c++)
						acc = orN({acc, SigSpec(cur[c])});
					return acc;
				};
				SigSpec ends = andN({bsig, any_in(gk - 1, hi >= 0 ? hi - 1 : K)});
				if (op == "[=]")
					ends = orN({ends, andN({mod->Not(NEW_ID, bsig), g, any_in(m, K)})});
				std::vector<SigBit> up;
				for (int c = 0; c <= K; c++) {
					SigSpec v = c == 0 ? SigSpec(Const(0, 1)) : SigSpec(cur[c - 1]);
					if (c == K && hi < 0)
						v = orN({v, SigSpec(cur[K])});
					up.push_back(v[0]);
				}
				// an occurrence shifts; a non-occurrence tick keeps the set
				// while the gate (throughout) holds and kills it otherwise
				SigSpec keep = mod->Mux(NEW_ID, Const(0, K + 1), SigSpec(cur), g);
				SigSpec nxt = mod->Mux(NEW_ID, keep, SigSpec(up), bsig);
				// all-zero = no attempt pending in this element (safe)
				ffd(dis_gate(nxt), sw, true);
				return ends;
			}
			if (!every_tick && hi >= 0)
				approx("noncons-rep-merged-counter", std::string(op) + " at " + log_id(n));
			int cap = std::max(m, 1);
			int w = 1;
			while ((1 << w) <= cap) w++;
			// an attempt counts b's from its start tick on; attempts are
			// merged existentially, so counting starts at the first start
			Wire *started_w = wn("rep_started");
			Wire *cnt_w = wn("rep_cnt", w);
			ffd(orN({start, started_w}), started_w, false);
			SigSpec sum = mod->Add(NEW_ID, cnt_w, SigSpec(bsig));
			SigSpec sat = mod->Eq(NEW_ID, cnt_w, Const(cap, w));
			SigSpec capped = mod->Mux(NEW_ID, sum, Const(cap, w), sat);
			ffd(mod->Mux(NEW_ID, Const(0, w), capped, orN({start, started_w})), cnt_w, false);
			SigSpec cnt = cnt_w;
			// cnt counts occurrence ends strictly before this tick.  A
			// concurrent assertion starts an attempt at EVERY tick and the
			// LRM F.3.4.2.3 window "the k-th occurrence for k in [m,n]"
			// collapses at the property level: the k-th occurrence (k >= m)
			// is the m-th occurrence for the attempt starting right after
			// the (k-m)-th, so every occurrence from the m-th on matches
			// SOME attempt and the upper bound n is vacuous.  [->] matches
			// AT the occurrence end (index k = cnt+1 >= m); [=m:n] ==
			// [->m:n] ##1 !b[*0:$] matches one or more ticks after the last
			// occurrence: cnt >= m (occurrences at-or-before t-1) and no
			// occurrence at this tick.
			//
			// Inside ## chains with a non-uniform offset this is an
			// over-approximation for BOUNDED ranges and single-bound
			// [->m] (the exact per-attempt anchored path above handles
			// Boolean bodies); it stays exact for unbounded ranges
			// (k >= m matches the LRM [->m:$] form) and at the property
			// root (attempts every tick collapse the upper bound away).
			SigSpec enough = mod->Ge(NEW_ID, cnt, Const(m > 0 ? m - 1 : 0, w));
			SigSpec at_occ = andN({bsig, enough});
			if (op == "[->]")
				return at_occ;
			// [=m:n] = [->m:n] ##1 !b[*0:$] (LRM 16.9.2): the empty !b
			// run ends the match AT the m-th occurrence (register v3 C4:
			// the battery oracle found ic3 proving `a ##1 b[=2] |-> 0`
			// while the antecedent matches), and every later !b tick
			// with cnt >= m ends it too
			SigSpec idle = mod->Not(NEW_ID, bsig);
			SigSpec enough_d = mod->Ge(NEW_ID, cnt, Const(m, w));
			SigSpec tail = andN({idle, gate_now(gate), enough_d});
			// (b[=0] = !b[*0:$] never ends at a b tick)
			return m > 0 ? mod->Or(NEW_ID, at_occ, tail) : tail;
		}

		if (op == "throughout") {
			// a throughout b: b's match with every tick gated by a.
			// The gate stays a raw expression (dyn_gate) so it is
			// re-spliced at the age of each tick of b's span.
			SigSpec saved = dyn_gate;
			dyn_gate = n->getPort(ports[0]);
			SigSpec r = seq_dispatch(port(1), child(1), start, gate);
			dyn_gate = saved;
			return r;
		}

		if (op == "clocked") {
			// nested sub-clock on the same clock is transparent
			// (single-clock approximation); a different clock is only
			// supported as the direct consequent of |->/|=> (v1, compiled
			// in prop_fail) -- anywhere else is an error
			if (clocked_differs(n))
				log_error("sva2check: multi-clock sequence element only supported as the direct consequent of |->/|=> in v1 (%s).\n", log_id(n));
			return seq_dispatch(port(1), child(1), start, gate);
		}

		if (op == "inside") {
			SigSpec x = port(0);
			int w = x.size();
			for (int i = 1; i < GetSize(ports); i++) {
				SigSpec item = n->getPort(ports[i]);
				Cell *ic = child_cell(item);
				if (ic && sva_op(ic) == "bounds") {
					auto bp = ports_of(ic);
					SigSpec lo = bp.size() > 0 ? ic->getPort(bp[0]) : Const(0, 32);
					SigSpec hi = bp.size() > 1 ? ic->getPort(bp[1]) : Const(0, 32);
					w = std::max(w, std::max(lo.size(), hi.size()));
				} else {
					w = std::max(w, item.size());
				}
			}
			if (x.size() < w)
				x.extend_u0(w, false);
			SigSpec r = Const(0, 1);
			for (int i = 1; i < GetSize(ports); i++) {
				SigSpec item = n->getPort(ports[i]);
				Cell *ic = child_cell(item);
				if (ic && sva_op(ic) == "bounds") {
					auto bp = ports_of(ic);
					SigSpec lo = bp.size() > 0 ? ic->getPort(bp[0]) : Const(0, 32);
					SigSpec hi = bp.size() > 1 ? ic->getPort(bp[1]) : Const(0, 32);
					if (lo.size() < w) lo.extend_u0(w, false);
					if (hi.size() < w) hi.extend_u0(w, false);
					r = mod->Or(NEW_ID, r, mod->And(NEW_ID,
							mod->Le(NEW_ID, lo, x), mod->Ge(NEW_ID, hi, x)));
				} else {
					if (item.size() < w) item.extend_u0(w, false);
					r = mod->Or(NEW_ID, r, mod->Eq(NEW_ID, x, item));
				}
			}
			return andN({start, gate_now(gate), r});
		}

		if (op == "weq" || op == "wne") {
			SigSpec a = port(0), b = port(1);
			int w = std::max(a.size(), b.size());
			if (a.size() < w) a.extend_u0(w, false);
			if (b.size() < w) b.extend_u0(w, false);
			SigSpec eq = mod->Eq(NEW_ID, a, b);
			return andN({start, gate_now(gate), op == "weq" ? eq : mod->Not(NEW_ID, eq)});
		}

		if (op == "or") {
			// sequence or: a parse of either operand ends the composite
			SigSpec a_end = seq_dispatch(port(0), child(0), start, gate);
			SigSpec b_end = seq_dispatch(port(1), child(1), start, gate);
			return orN({a_end, b_end});
		}

		if (op == "intersect" || op == "and" || op == "within") {
			if (can_empty(child(0)) || can_empty(child(1)))
				approx("empty-match-operand", op + " at " + std::string(log_id(n)));
			// Binary sequence operators pair one parse of each operand
			// FROM THE SAME ATTEMPT (LRM 16.9.5, 16.9.6, 16.9.10).  The
			// operand end signals are merged over every attempt in
			// flight, so pairing them directly let one attempt's end
			// complete another attempt's pair: `(t==3||t==4) |-> ((x ##1
			// 1) intersect (y ##[0:1] 1))`, `... (y within (x ##2 1))` and
			// `... ((a ##1 1) and (b ##1 1))` were PROVED with the
			// earlier attempt unmatchable (register v3 C1, found through
			// the LRM Figure 16-7 transcription, 2026-10-19).
			//   both operands fixed-length: an end's start tick is
			//     determined by its length, so shifting one operand's
			//     end to the other's is exact;
			//   otherwise: rotating copies -- copy i takes the attempts
			//     at ticks with phase == i, as many copies as an
			//     attempt's longest span, so each copy has one attempt in
			//     flight and pairs only its own parses.  Slot mode
			//     (locals), multi-clock islands and spans > 16 keep the
			//     merged pairing, tagged.
			Cell *ac = child(0), *bc = child(1);
			int la = ac ? seq_max_len(ac) : 1, lb = bc ? seq_max_len(bc) : 1;
			int ma = ac ? seq_min_len(ac) : 1, mb = bc ? seq_min_len(bc) : 1;
			// `and` needs both operands bounded, `within` its container;
			// `intersect` / `within` with an unbounded other operand keep
			// the merged pairing (tagged): a long parse of the unbounded
			// side from an earlier attempt would reach into a copy's next
			// attempt
			bool bounded = la >= 0 && la <= 64 && lb >= 0 && lb <= 64;
			if ((op == "and" && !bounded) || (op == "within" && (lb < 0 || lb > 64)))
				log_error("sva2check: '%s' over an unbounded sequence in %s.\n", op.c_str(), log_id(n));
			auto a_from = [&](SigSpec st) { return seq_dispatch(port(0), ac, st, gate); };
			auto b_from = [&](SigSpec st) { return seq_dispatch(port(1), bc, st, gate); };
			bool fixed = bounded && ma == la && mb == lb;
			if (fixed) {
				if (op == "intersect")
					return la == lb ? mod->And(NEW_ID, a_from(start), b_from(start)) : SigSpec(Const(0, 1));
				if (op == "and") {
					SigSpec a_end = a_from(start), b_end = b_from(start);
					return la >= lb ? andN({a_end, delay(b_end, la - lb, "sand")})
							: andN({b_end, delay(a_end, lb - la, "sand")});
				}
				// within: a starts at offset k of b's span and ends by
				// b's end, k = 0 .. lb-la; at b's end t, a's end was
				// lb-la-k ticks ago
				SigSpec b_end = b_from(start), any = Const(0, 1);
				for (int k = 0; k <= lb - la; k++)
					any = orN({any, delay(a_from(k == 0 ? start : delay(start, k, "within")), lb - la - k, "within")});
				return andN({b_end, any});
			}
			// within: a's parses may start anywhere in b's span and run
			// la-1 ticks further
			int span = op == "within" ? lb + la - 1 : std::max(la, lb);
			auto pair_one = [&](SigSpec st) -> SigSpec {
				if (op == "intersect")
					return mod->And(NEW_ID, a_from(st), b_from(st));
				if (op == "and") {
					SigSpec a_end = a_from(st), b_end = b_from(st);
					SigSpec a_seen = since(a_end, st), b_seen = since(b_end, st);
					return orN({andN({a_end, b_seen}), andN({b_end, a_seen})});
				}
				SigSpec a_end = Const(0, 1);
				for (int k = 0; k < lb; k++)
					a_end = orN({a_end, a_from(k == 0 ? st : delay(st, k, "within"))});
				return andN({b_from(st), since(a_end, st)});
			};
			if (bounded && span > 1 && !(slots_mode || island_mode || in_island || span > 16)) {
				int w = 1;
				while ((1 << w) < span) w++;
				Wire *ph = wn("att_phase", w);
				SigSpec wrap = mod->Eq(NEW_ID, SigSpec(ph), Const(span - 1, w));
				SigSpec nxt = mod->Mux(NEW_ID, mod->Add(NEW_ID, SigSpec(ph), Const(1, w)), Const(0, w), wrap);
				mod->addDff(NEW_ID, clk, nxt.extract(0, w), ph, clk_pos);
				SigSpec r = Const(0, 1);
				for (int i = 0; i < span; i++)
					r = orN({r, pair_one(andN({start, mod->Eq(NEW_ID, SigSpec(ph), Const(i, w))}))});
				return r;
			}
			// the merged pairing (pre-C1), tagged: exact for one attempt
			// in flight, otherwise an end may pair across attempts
			if (span > 1 || !bounded)
				approx("seq-binop-merged-attempts", op + " at " + std::string(log_id(n)));
			if (op == "intersect")
				return mod->And(NEW_ID, a_from(start), b_from(start));
			if (op == "and") {
				SigSpec a_end = a_from(start), b_end = b_from(start);
				SigSpec a_ever = a_end, b_ever = b_end;
				for (int j = 1; j <= lb; j++)
					a_ever = orN({a_ever, delay(a_end, j, "sand")});
				for (int j = 1; j <= la; j++)
					b_ever = orN({b_ever, delay(b_end, j, "sand")});
				return orN({andN({a_end, b_ever}), andN({b_end, a_ever})});
			}
			SigSpec b_end = b_from(start), r = Const(0, 1);
			for (int k = 0; k <= lb; k++) {
				SigSpec a_end = a_from(k == 0 ? start : delay(start, k, "within"));
				SigSpec any = a_end;
				for (int j = 1; j <= lb - k; j++)
					any = orN({any, delay(a_end, j, "within")});
				r = orN({r, andN({b_end, any})});
			}
			return r;
		}

		if (op == "first_match") {
			if (can_empty(child(0)))
				approx("empty-match-operand", "first_match at " + std::string(log_id(n)));
			// ( seq [, x = expr [, ...]] ): of all parses of an attempt,
			// only the earliest-ending one survives (LRM 16.9.9); local
			// variables sampled at that end.  Exact per-offset splitting
			// when the tree has no locals; with locals the shared slot
			// file approximation stays (documented).
			SigSpec end;
			if (!slots_mode && child(0)) {
				end = seq_end_fm(child(0), start, gate).first;
			} else {
				if (child(0))
					approx("first-match-locals-shared-slots", "first_match at " + std::string(log_id(n)));
				end = seq_dispatch(port(0), child(0), start, gate);
			}
			if (GetSize(ports) >= 2) {
				Cell *m0 = child(1);   // original matches cell (clone ports are shells)
				if (m0 && sva_op(m0) == "matches") {
					for (auto mp : ports_of(m0)) {
						Cell *a = child_cell(m0->getPort(mp));
						if (a && sva_op(a) == "assign")
							capture_local(a, end);
					}
				}
			}
			return end;
		}

		if (op == "seq_match") {
			// ( seq , x = expr [, ... ] ): local variables sampled when
			// the sequence completes; multi-match pass-through.
			SigSpec end = seq_dispatch(port(0), child(0), start, gate);
			if (GetSize(ports) >= 2) {
				Cell *m0 = child(1);   // original matches cell (clone ports are shells)
				if (m0 && sva_op(m0) == "matches") {
					for (auto mp : ports_of(m0)) {
						Cell *a = child_cell(m0->getPort(mp));
						if (a && sva_op(a) == "assign")
							capture_local(a, end);
					}
				}
			}
			return end;
		}

		if (op == "assign") {
			// standalone assignment element: matches its start tick
			SigSpec end = andN({start, gate_now(gate)});
			capture_local(n, end);
			return end;
		}

		if (op == "meq") {
			// wildcard equality: constant pattern x/z bits are don't-cares
			SigSpec a = resolve(port(0));
			SigSpec b = resolve(port(1));
			SigSpec eq;
			if (b.is_fully_const() && !b.as_const().is_fully_def()) {
				int w = std::max(a.size(), b.size());
				if (a.size() < w) a.extend_u0(w, false);
				Const bc = b.as_const();
				Const mask(0, w), target(0, w);
				for (int i = 0; i < w; i++) {
					State s = i < bc.size() ? bc[i] : State::S0;
					if (s == State::S0 || s == State::S1) {
						mask.set(i, State::S1);
						target.set(i, s);
					}
				}
				SigSpec diff = mod->And(NEW_ID, mod->Xor(NEW_ID, a, target), mask);
				eq = mod->Not(NEW_ID, mod->ReduceOr(NEW_ID, diff));
			} else if (a.is_fully_const() && !a.as_const().is_fully_def()) {
				eq = mod->Eq(NEW_ID, a, b);
			} else {
				eq = mod->Eq(NEW_ID, a, b);
			}
			return andN({start, gate_now(gate), eq});
		}

		log_error("sva2check: unsupported sequence operator '%s' in %s.\n", op.c_str(), log_id(n));
		return SigSpec();
	}

	// Exactness budget for per-offset first-match splitting: each ranged
	// delay/multi-match node spends (hi-lo+1); when the budget runs out the
	// .first signal degrades to .any (documented approximation; .any is
	// always the plain existential semantics).
	int fm_budget = 64;

	// First-match compilation (LRM 16.9.9): of all parses of one attempt,
	// only the earliest-ending one survives.  Returns {first, any}:
	// - .first: pulse at the earliest end tick of the attempt
	// - .any:   pulse at any end tick (plain existential semantics)
	// Exact for delay chains, sequence or/and, throughout, gating
	// repetitions and leaves: a ranged `lhs ##[lo:hi] tail` parses at
	// lhs_end + k for k in [lo,hi], and the earliest surviving end comes
	// from the smallest offset whose tail completes, so .first compiles
	// the tail once per offset and keeps end_k only when no earlier
	// offset produced an end, while .any keeps the existential tap OR
	// (the plain ## semantics).  Approximated by .any where the budget
	// runs out, for unbounded delays/repetitions, intersect operands and
	// instance bodies (documented).
	std::pair<SigSpec, SigSpec> seq_end_fm(Cell *n, SigSpec start, SigSpec gate)
	{
		std::string op = sva_op(n);
		auto ports = ports_of(n);
		if (clone_scope && !clone_cmap.count(n))
			ensure_stub(n);
		Cell *srcn = clone_scope ? clone_cmap[n] : nullptr;
		auto port = [&](int i) -> SigSpec {
			return i < GetSize(ports) ? cport(n, srcn, ports[i]) : SigSpec();
		};
		auto child = [&](int i) -> Cell * {
			return i < GetSize(ports) ? child_cell(n->getPort(ports[i])) : nullptr;
		};
		auto sub_fm = [&](int i) {
			Cell *c = child(i);
			if (c)
				return seq_end_fm(c, start, gate);
			SigSpec e = andN({start, gate, reduce1(port(i))});
			return std::make_pair(e, e);
		};
		auto leaf = [&]() {
			SigSpec e = seq_end(n, start, gate);
			return std::make_pair(e, e);
		};
		if (op == "##") {
			// two-port ##: leading-delay form `@(c) ##k body` (see seq_end)
			bool lead2 = GetSize(ports) == 2;
			int ti = lead2 ? 1 : 2;
			SigSpec s_any = lead2 ? andN({start, gate_now(gate)})
			                      : seq_dispatch(port(0), child(0), start, gate);
			SigSpec s_first = (!lead2 && child(0)) ? seq_end_fm(child(0), start, gate).first : s_any;
			SigSpec cyc = n->getPort(ports[lead2 ? 0 : 1]);
			Cell *cyc_cell = child_cell(cyc);
			std::string cop = cyc_cell ? sva_op(cyc_cell) : "";
			int lo = 0, hi = -1;
			if (cyc_cell && (cop == "##[*]" || cop == "##[+]")) {
				lo = cop == "##[+]" ? 1 : 0;
			} else if (cyc_cell && cop == "bounds") {
				auto bp = ports_of(cyc_cell);
				if (!const_int(cyc_cell->getPort(bp[0]), lo))
					log_error("sva2check: non-constant ## bound in %s.\n", log_id(n));
				if (GetSize(bp) >= 2 && !const_int(cyc_cell->getPort(bp[1]), hi))
					log_error("sva2check: non-constant ## bound in %s.\n", log_id(n));
			} else if (cyc_cell) {
				log_error("sva2check: unsupported ## cycle operator '%s' in %s.\n", cop.c_str(), log_id(n));
			} else {
				int k;
				if (!const_int(cyc, k) || k < 0)
					log_error("sva2check: non-constant ## delay in %s.\n", log_id(n));
				lo = hi = k;
			}
			if (hi < 0 || hi > 128) {
				// unbounded (or absurdly wide): no per-offset split
				SigSpec e = seq_end(n, start, gate);
				return std::make_pair(e, e);
			}
			// .any: OR of taps into one tail compile (plain semantics)
			SigSpec t_any = lo == 0 ? s_any : Const(0, 1);
			SigSpec tap = s_any;
			for (int k = 1; k <= hi; k++) {
				tap = reg(tap, "delay");
				if (k >= lo)
					t_any = orN({t_any, tap});
			}
			SigSpec any = seq_dispatch(port(ti), child(ti), t_any, gate);
			int span = hi - lo + 1;
			if (span > fm_budget) {
				// splitting would blow up: .first degrades to .any
				approx("first-match-any", "split budget at " + std::string(log_id(n)));
				SigSpec e = seq_dispatch(port(ti), child(ti), t_any, gate);
				return std::make_pair(e, any);
			}
			fm_budget -= span;
			if (hi > lo)
				approx("first-match-overlap-window", "## at " + std::string(log_id(n)));
			// .first: per-offset tail compiles; end_k survives only if no
			// earlier offset produced an end at or before it for this
			// attempt.  An earlier offset's end sits at most (hi-lo) +
			// tail-length behind the candidate end (the offset gap plus
			// the tail's own stretch; beyond that the ends belong to
			// other attempts -- documented approximation).
			int w = (hi - lo) + (child(ti) ? seq_max_len(child(ti)) : 1);
			if (w < 0 || w > 64) {
				approx("first-match-any", "window at " + std::string(log_id(n)));
				SigSpec e = seq_dispatch(port(ti), child(ti), t_any, gate);
				return std::make_pair(e, any);
			}
			SigSpec first = Const(0, 1);
			SigSpec earlier_any = Const(0, 1);   // 1 = some earlier offset ended
			for (int k = lo; k <= hi; k++) {
				SigSpec t_f = k == 0 ? s_first : delay(s_first, k, "fmd");
				auto tk = child(ti) ? seq_end_fm(child(ti), t_f, gate)
						   : std::make_pair(andN({t_f, gate, reduce1(port(ti))}),
								    andN({t_f, gate, reduce1(port(ti))}));
				SigSpec earlier_ever = earlier_any;
				for (int j = 1; j <= w; j++)
					earlier_ever = orN({earlier_ever, delay(earlier_any, j, "fmx")});
				first = orN({first, andN({tk.first, mod->Not(NEW_ID, earlier_ever)})});
				earlier_any = orN({earlier_any, tk.second});
			}
			return std::make_pair(first, any);
		}
		if (op == "or") {
			auto a = sub_fm(0);
			auto b = sub_fm(1);
			SigSpec any = orN({a.second, b.second});
			// .first: a branch's earliest end survives only when the sibling
			// branch produced no end at or before it in the same attempt;
			// the suppression window is the sibling's static max length
			// (ends beyond it belong to other attempts -- a documented
			// approximation for overlapping attempts)
			int la = child(0) ? seq_max_len(child(0)) : 1;
			int lb = child(1) ? seq_max_len(child(1)) : 1;
			if (la < 0 || la > 64 || lb < 0 || lb > 64) {
				approx("first-match-any", "or at " + std::string(log_id(n)));
				return std::make_pair(any, any);
			}
			approx("first-match-overlap-window", "or at " + std::string(log_id(n)));
			SigSpec a_ever = a.second;
			for (int j = 1; j <= lb; j++)
				a_ever = orN({a_ever, delay(a.second, j, "fmo")});
			SigSpec b_ever = b.second;
			for (int j = 1; j <= la; j++)
				b_ever = orN({b_ever, delay(b.second, j, "fmo")});
			SigSpec first = orN({andN({a.first, mod->Not(NEW_ID, b_ever)}),
					andN({b.first, mod->Not(NEW_ID, a_ever)})});
			return std::make_pair(first, any);
		}
		if (op == "and" || op == "intersect") {
			// earliest common end is not derivable from the operands'
			// earliest ends; approximate .first by .any (documented)
			approx("first-match-any", op + " at " + std::string(log_id(n)));
			auto a = sub_fm(0);
			auto b = sub_fm(1);
			SigSpec both = mod->And(NEW_ID, a.second, b.second);
			return std::make_pair(both, both);
		}
		if (op == "throughout") {
			SigSpec saved_g = dyn_gate;
			dyn_gate = n->getPort(ports[0]);
			SigSpec g = gate;
			auto b = child(1) ? seq_end_fm(child(1), start, g)
			                  : std::make_pair(andN({start, gate_now(g)}), andN({start, gate_now(g)}));
			dyn_gate = saved_g;
			return b;
		}
		if (op == "first_match") {
			if (!child(0))
				return leaf();
			auto r = seq_end_fm(child(0), start, gate);
			return r;
		}
		if (op == "clocked") {
			if (clocked_differs(n))
				log_error("sva2check: multi-clock sequence element only supported as the direct consequent of |->/|=> in v1 (%s).\n", log_id(n));
			return sub_fm(1);
		}
		if (op == "[*]" || op == "[+]") {
			Cell *b = child(1);
			int m = op == "[*]" ? 0 : 1, hi = -1;
			if (b) {
				if (sva_op(b) != "bounds")
					log_error("sva2check: malformed repetition bounds in %s.\n", log_id(n));
				auto bp = ports_of(b);
				int lo_v = 0;
				if (!bp.empty() && !const_int(b->getPort(bp[0]), lo_v))
					log_error("sva2check: non-constant repetition bound in %s.\n", log_id(n));
				m = lo_v;
				if (GetSize(bp) >= 2 && !const_int(b->getPort(bp[1]), hi))
					log_error("sva2check: non-constant repetition bound in %s.\n", log_id(n));
			}
			if (hi >= 0 && hi <= 8) {
				// consecutive repetition: .first ends after exactly m
				// copies, .any after m..hi
				SigSpec first = rep_chain_end(child(0), port(0), start, gate, std::max(m, 1), std::max(m, 1));
				SigSpec any = rep_chain_end(child(0), port(0), start, gate, std::max(m, 1), hi);
				return std::make_pair(first, any);
			}
			approx("first-match-any", "unbounded repetition at " + std::string(log_id(n)));
			return leaf();  // unbounded run: approximate
		}
		if (op == "true" || op == "inside" || op == "weq" || op == "wne" ||
				op == "meq" || op == "assign")
			return leaf();  // boolean leaves: exact
		if (op == "[->]" || op == "[=]" || op == "within" || op == "##[*]" ||
				op == "##[+]" || op == "seq_match") {
			approx("first-match-any", op + " at " + std::string(log_id(n)));
			return leaf();
		}
		if (decl_by_name(op)) {
			approx("first-match-any", "instance body " + op);
			return leaf();  // instance body: approximate (documented)
		}
		return leaf();
	}

	SigSpec seq_dispatch(SigSpec s, Cell *c, SigSpec start, SigSpec gate)
	{
		static int depth_ctr = 0;
		if (++depth_ctr > 200)
			log_error("sva2check: runaway recursion in seq_dispatch.\n");
		struct DepthGuard { int *c; ~DepthGuard() { --*c; } } dg { &depth_ctr };
		if (c)
			return seq_end(c, start, gate);
		if (slots_mode) {
			static std::vector<Wire *> no_formals;
			s = splice_expr(s, no_formals, false);
		}
		s = resolve(s);
		if (Cell *decl = decl_for_wire(s))
			return inline_decl(decl, {}, false, start, gate);
		if (s.size() != 1)
			return andN({start, gate_now(gate), mod->ReduceBool(NEW_ID, s)});
		return andN({start, gate_now(gate), s});
	}

	// End pulse of a run of >= m consecutive matches of body (boolean body
	// or true): run counter capped at m.
	SigSpec rep_run_end(Cell *body_cell, SigSpec body, SigSpec start, SigSpec gate, int m, int hi = -1)
	{
		if (body_cell) {
			// sub-sequence run s[+]: the next copy starts the tick after a
			// copy ends (s[*2] = s ##1 s), so copies are fed back into the
			// body's start; ends merge across attempts like any sequence
			// end.  (The old form OR-ed a never-cleared "a copy ended
			// before" flag into the end: true forever after one match.)
			if (tree_uses_locals(body_cell))
				log_error("sva2check: local variables in an unbounded repetition of a sequence are not supported.\n");
			Wire *qw = wn("rep_next", 1);
			SigSpec e = seq_end(body_cell, orN({andN({start, gate_now(gate)}), SigSpec(qw)}), gate);
			ffd(dis_gate(e), qw, true);
			return e;
		}
		// Boolean run b[*m:hi] (hi < 0: unbounded) as an exact run-length
		// set: bit c = some live attempt has c consecutive b's before this
		// tick (an attempt entering now has 0).  b moves every attempt up
		// one (lengths past hi leave; unbounded saturates at m-1), !b kills
		// them all.  The end needs b now and a length c+1 in [m, hi].  (The
		// old merged run counter ignored `start`: `t==5 ##1 c[*1:20]`
		// matched at any c tick.)
		SigSpec b = andN({reduce1(body), gate_now(gate)});
		m = std::max(m, 1);
		int K = (hi >= 0 ? hi : m) - 1;
		if (K > 64)
			log_error("sva2check: consecutive repetition bound %d is too large (max 65).\n", hi);
		Wire *sw = wn("run_set", K + 1);
		std::vector<SigBit> cur;
		for (int c = 0; c <= K; c++)
			cur.push_back(c == 0 ? mod->Or(NEW_ID, SigSpec(sw, 0, 1), start)[0] : SigBit(sw, c));
		SigSpec hit = Const(0, 1);
		for (int c = m - 1; c <= K; c++)
			hit = orN({hit, SigSpec(cur[c])});
		std::vector<SigBit> up;
		for (int c = 0; c <= K; c++) {
			SigSpec v = c == 0 ? SigSpec(Const(0, 1)) : SigSpec(cur[c - 1]);
			if (c == K && hi < 0)
				v = orN({v, SigSpec(cur[K])});
			up.push_back(v[0]);
		}
		ffd(dis_gate(mod->Mux(NEW_ID, Const(0, K + 1), SigSpec(up), b)), sw, true);
		return andN({b, hit});
	}

	// Bounded consecutive repetition: chain hi copies of the body, the
	// element starting one tick after the previous element ends; end pulses
	// are tapped at copy counts >= m.
	SigSpec rep_chain_end(Cell *body_cell, SigSpec body, SigSpec start, SigSpec gate, int m, int hi,
			std::vector<SigSpec> *per_copy = nullptr)
	{
		SigSpec chain_start = start;
		SigSpec cur_age = age;
		SigSpec r = Const(0, 1);
		for (int k = 1; k <= hi; k++) {
			SigSpec e;
			if (slots_mode) {
				// copy k sits at age k-1 along the attempt (copy 1 shares
				// the start tick); later copies are later ticks, so their
				// same-tick chained writes have expired
				SigSpec saved_age = age;
				dict<Wire *, std::pair<SigSpec, SigSpec>> saved_pending = pending_wr;
				age = cur_age;
				slot_epoch++;
				if (k > 1)
					pending_wr.clear();
				e = body_cell ? seq_end(body_cell, chain_start, gate)
				              : andN({chain_start, gate_now(gate), reduce1(body)});
				age = saved_age;
				pending_wr = saved_pending;
				cur_age = age_tick(cur_age);
			} else {
				e = body_cell ? seq_end(body_cell, chain_start, gate)
				              : andN({chain_start, gate_now(gate), reduce1(body)});
			}
			if (per_copy)
				per_copy->push_back(e);
			if (k >= m)
				r = mod->Or(NEW_ID, r, e);
			chain_start = delay(e, 1, "rep_chain");
		}
		return r;
	}

	// --- properties --------------------------------------------------------

	// violation pulse: "the property is violated at the current tick by an
	// attempt started per `start`".
	// weak-until monitor shared by the "until"/"until_with" prop_dispatch
	// branch and the s_until/s_until_with split below (strong until =
	// weak monitor plus an F q liveness twin).  with = until_with
	// semantics: q completes only together with p.
	SigSpec until_fail(SigSpec p, SigSpec q, SigSpec start, bool with)
	{
		// One live bit for all attempts in flight: every live attempt of
		// a weak until sees the same future (p and q from now on), so
		// they complete or fail together and the merged state is exact,
		// for pulse starts as well as at the property root.  An attempt
		// starting now is live now (a failure at its start tick fires,
		// sva_pbt F1).
		//   until:      q completes; !p && !q fails; p && !q continues
		//   until_with: q && p completes; !p fails (the q step itself is
		//               not exempt, LRM 16.12.10); p && !q continues
		// Both continue exactly when p && !q.  (The old monitor's
		// `active` latch never cleared, so an attempt that had already
		// failed fired again at every later !p tick: extra failure ticks
		// after the first, PBT SIM-STEP s1_i7 / s2_i39 / s3_i5; it was
		// tagged until-merged-attempts.)
		Wire *lw = wn("until_live", 1);
		SigSpec live = orN({start, SigSpec(lw)});
		SigSpec np = mod->Not(NEW_ID, p);
		SigSpec fail = with ? andN({live, np}) : andN({live, np, mod->Not(NEW_ID, q)});
		// all-zero = no attempt pending (safe for the early check)
		ffd(dis_gate(andN({live, p, mod->Not(NEW_ID, q)})), lw, true);
		return fail;
	}

	// The fixed failure offset of a property (register v3 F10): ticks from
	// an attempt's start to the tick its failure is reported, when that is
	// the same for every attempt; -1 when it varies.  Mirrors prop_fail.
	int fail_off(Cell *c)
	{
		if (!c)
			return 0;   // Boolean leaf: fails at its own tick
		std::string op = sva_op(c);
		auto p = ports_of(c);
		auto ch = [&](int i) -> Cell * { return i < GetSize(p) ? child_cell(c->getPort(p[i])) : nullptr; };
		auto fixed_len = [&](Cell *q) -> int {
			if (!q) return 1;
			int lo = seq_min_len(q), hi = seq_max_len(q);
			return lo == hi && lo >= 1 ? lo : -1;
		};
		if (is_seq_op(op)) {
			// strong_seq_fail: the deadline form reports at len-1; the
			// early form at the tick the attempt dies (varies)
			int len = seq_max_len(c);
			if (len < 1) return -1;
			// the early form (assume/restrict, -early) reports when the
			// attempt dies -- even a fixed-length sequence can die before
			// its deadline (tearly e_fixed), so no offset is fixed
			if (early_fail && len > 1) return -1;
			return len - 1;
		}
		if (decl_by_name(op))
			return -1;
		if (op == "|->" || op == "|=>") {
			int la = fixed_len(ch(0)), d = fail_off(ch(1));
			if (la < 0 || d < 0 || (ch(1) && sva_op(ch(1)) == "clocked")) return -1;
			return la - 1 + (op == "|=>") + d;
		}
		if (op == "or" || op == "implies" || op == "iff") {
			int d1 = fail_off(ch(0)), d2 = fail_off(ch(1));
			return d1 < 0 || d2 < 0 ? -1 : std::max(d1, d2);
		}
		if (op == "and") {
			int d1 = fail_off(ch(0)), d2 = fail_off(ch(1));
			return d1 >= 0 && d1 == d2 ? d1 : -1;
		}
		if (op == "not") {
			if (!ch(0)) return 0;
			int l = fixed_len(ch(0));
			return seq_shaped(ch(0)) && l >= 1 ? l - 1 : -1;
		}
		if (op == "if")
			return fail_off(ch(1));
		if (op == "ifelse") {
			int d1 = fail_off(ch(1)), d2 = fail_off(ch(2));
			return d1 >= 0 && d1 == d2 ? d1 : -1;
		}
		if (op == "strong" || op == "weak")
			return fail_off(ch(0));
		if (op == "nexttime" || op == "s_nexttime") {
			int k = 1, idx = 0;
			if (GetSize(p) >= 2 && const_int(c->getPort(p[0]), k))
				idx = 1;
			int d = fail_off(ch(idx));
			return d < 0 ? -1 : k + d;
		}
		if (op == "eventually" || op == "s_eventually") {
			Cell *b = ch(0);
			if (!b || sva_op(b) != "bounds") return -1;
			auto bp = ports_of(b);
			int hi;
			if (bp.size() < 2 || !const_int(b->getPort(bp[1]), hi) || hi < 0) return -1;
			int d = fail_off(ch(1));
			return d < 0 ? -1 : hi + d;
		}
		if (op == "always" || op == "s_always") {
			Cell *b = ch(0);
			if (!b || sva_op(b) != "bounds") return -1;
			auto bp = ports_of(b);
			int lo, hi;
			if (bp.size() < 2 || !const_int(b->getPort(bp[0]), lo) ||
					!const_int(b->getPort(bp[1]), hi) || lo != hi)
				return -1;
			int d = fail_off(ch(1));
			return d < 0 ? -1 : lo + d;
		}
		return -1;
	}

	SigSpec prop_fail(Cell *n, SigSpec start)
	{
		static int depth_ctr = 0;
		if (++depth_ctr > 200) {
			fprintf(stderr, "RUNAWAY prop_fail\n");
			abort();
		}
		struct DepthGuard { int *c; ~DepthGuard() { --*c; } } dg { &depth_ctr };
		std::string op = sva_op(n);
		if (getenv("SVA_DEBUG_OPS"))
			log("sva2check-debug: prop_fail %s (%s) in %s\n", op.c_str(), log_id(n), check_name.c_str());
		auto ports = ports_of(n);
		if (clone_scope && !clone_cmap.count(n))
			ensure_stub(n);
		Cell *srcn = clone_scope ? clone_cmap[n] : nullptr;
		auto port = [&](int i) -> SigSpec {
			return i < GetSize(ports) ? cport(n, srcn, ports[i]) : SigSpec();
		};
		auto child = [&](int i) -> Cell * {
			return i < GetSize(ports) ? child_cell(n->getPort(ports[i])) : nullptr;
		};
		auto sub = [&](int i) -> SigSpec {
			return prop_dispatch(port(i), child(i), start);
		};

		if (op == "|->" || op == "|=>" || op == "#-#" || op == "#=#") {
			// the antecedent is a merged-existential context (concurrent
			// attempts every tick): [->]/[=] keep the collapsed global
			// counter there
			SigSpec ant_end = seq_dispatch(port(0), child(0), start, Const(1, 1));
			bool shift = op == "|=>" || op == "#=#";
			SigSpec c_start = shift ? delay(ant_end, 1, "impl") : ant_end;
			// an empty antecedent match: |-> ignores it (Annex F: the
			// match is a nonempty prefix), |=> = (R ##1 1) |-> P checks P
			// at the attempt tick itself (empty ##1 1 = 1)
			if (shift && can_empty(child(0)))
				c_start = orN({c_start, start});
			// v1 multi-clock: a consequent rooted at a `clocked` node whose
			// clock differs from the property clock compiles as its own
			// monitor island on the inner clock.  The island start is
			// derived from the antecedent end/start signal with
			// attempt-delimiting logic (NOT a CDC synchronizer): the raw
			// level would spawn shadow attempts at every inner-clock edge
			// while the outer clock stalls (the start value only changes on
			// outer edges, but the island's deadline chains sample it at
			// inner edges), falsely failing true properties; and a bare
			// outer-edge pulse could fall between inner edges and never be
			// seen.  So the start is delimited to the LRM attempt ticks
			// (outer edges where it holds, persisted across the rest of the
			// outer cycle) and then sampled by the inner clock into a
			// single inner-tick pulse at the first inner edge at or after
			// the handoff -- which is also the ##0/##1 first-element
			// sampling semantics.
			{
				Cell *cc = child(1);
				while (cc && (sva_op(cc) == "strong" || sva_op(cc) == "weak"))
					cc = child_cell(cc->getPort(ID(A)));
				if (cc && sva_op(cc) == "clocked") {
					SigSpec iclk;
					bool ipos;
					if (clocked_event(cc, iclk, ipos) &&
							(iclk[0] != clk[0] || ipos != clk_pos)) {
						if (clone_scope)
							log_error("sva2check: multi-clock consequent inside an instantiated property/sequence is not supported in v1 (%s).\n", log_id(n));
						SigSpec cbody = cc->getPort(ID(B));
						Cell *cbc = child_cell(cbody);
						if (cbc && !is_seq_op(sva_op(cbc)))
							log_error("sva2check: multi-clock property element only supported as the direct consequent of |->/|=> in v1 (%s).\n", log_id(cc));
						// delimit the start to the outer edges where it
						// holds, persisted across the rest of the outer
						// cycle (global-clock FFs: unaffected by
						// clk2fflogic, which only rewrites clocked FFs)
						SigSpec oclk = clk;
						bool opos = clk_pos;
						Wire *pastw = wn("mcpastclk");
						mod->addFf(NEW_ID, oclk, pastw);
						// init so edge_prev is false at the init step:
						// with pastw=0 a clock starting high at step 0
						// false-triggers oedge while no wr-domain state
						// ticks there, handing the island a match whose
						// captured data does not exist yet (tmcloc1 data
						// cex).  Matches clk2fflogic's own past-clk init.
						pastw->attributes[ID::init] = RTLIL::Const(opos ? State::S1 : State::S0, 1);
						SigSpec edge_now = opos ? oclk : mod->Not(NEW_ID, oclk);
						SigSpec edge_prev = opos ? mod->Not(NEW_ID, SigSpec(pastw)) : SigSpec(pastw);
						SigSpec oedge = mod->And(NEW_ID, edge_now, edge_prev);
						Wire *holdw = wn("mchold");
						mod->addFf(NEW_ID, mod->Mux(NEW_ID, holdw, c_start, oedge), holdw);
						SigSpec s_level = mod->And(NEW_ID, c_start,
								mod->Or(NEW_ID, oedge, holdw));
						// deliver as a single inner-tick pulse at the
						// first inner edge that sees the level
						SigSpec saved_clk = clk;
						bool saved_pos = clk_pos;
						clk = iclk[0];
						clk_pos = ipos;
						multiclock = true;
						SigSpec ss = reg(s_level, "mcstart");
						SigSpec pss = reg(ss, "mcstart");
						SigSpec pulse = mod->And(NEW_ID, ss, mod->Not(NEW_ID, pss));
						// locals: carry every known local across the
						// handoff in a shift chain on the consequent
						// clock.  stage 0 loads the outer ring's value
						// (combinatorial at the handoff tick; the ring,
						// phase and age are stable for the rest of the
						// outer cycle, and s_level guarantees the pulse
						// fires inside it); stage k shifts once per
						// consequent tick.  Reads inside the island
						// select the stage by the island age vector.
						// Writes in the island are rejected (v1).
						if (slots_mode) {
							int span = cbc ? seq_max_len(cbc) : 0;
							int L = std::min(nslots, span + 1);
							for (auto &kv : slotfiles) {
								Wire *xw = kv.first;
								SigSpec ov = local_read(xw);
								std::vector<Wire *> chain(L);
								for (int k = 0; k < L; k++) {
									chain[k] = wn("mcslot", ov.size());
									// stage 0 is gated by s_level (not the
									// registered pulse): it must sample ov
									// AT the first inner edge of the
									// handoff so its Q is valid during the
									// island's tick-0 window; a pulse-based
									// enable lands one inner tick late and
									// shifts the whole chain off by one
									SigSpec d = k == 0
											? mod->Mux(NEW_ID, SigSpec(chain[0]), ov, s_level)
											: SigSpec(chain[k - 1]);
									mod->addDff(NEW_ID, iclk[0], d, chain[k], ipos);
								}
								island_chains[xw] = chain;
							}
						}
						SigSpec saved_age = age;
						if (slots_mode) {
							island_mode = true;
							age = Const(0, nslots);
							age = sigset(age, 0, pulse);
							slot_epoch++;
						}
						SigSpec r;
						{
							if (cbc) {
								in_island = true;
								r = strong_seq_fail(cbc, pulse);
								in_island = false;
							}
							else {
								static std::vector<Wire *> no_formals;
								r = andN({pulse,
										mod->Not(NEW_ID, reduce1(splice_expr(cbody, no_formals, false)))});
							}
						}
						if (slots_mode) {
							island_mode = false;
							island_chains.clear();
							age = saved_age;
							slot_epoch++;
						}
						clk = saved_clk;
						clk_pos = saved_pos;
						return r;
					}
				}
			}
			if (!slots_mode || !shift)
				return prop_dispatch(port(1), child(1), c_start);
			// the consequent is one tick further along the same attempt
			SigSpec saved_age = age;
			dict<Wire *, std::pair<SigSpec, SigSpec>> saved_pending = pending_wr;
			age = age_tick(age);
			slot_epoch++;
			pending_wr.clear();
			SigSpec r = prop_dispatch(port(1), child(1), c_start);
			age = saved_age;
			pending_wr = saved_pending;
			return r;
		}
		if (op == "implies" || op == "iff" || op == "or") {
			// The operand fail pulses are merged over attempts; an
			// attempt's verdict needs BOTH operands of the SAME attempt,
			// resolved at possibly different ticks (LRM 16.12.4).  When
			// each operand reports at a fixed offset from its attempt
			// start (fail_off), the earlier one is delayed onto the later
			// and the pair is exact; the verdict lands when the later
			// operand resolves.  (The same-tick pairing used to combine
			// different attempts: false counterexamples and missed
			// failures -- PBT class F10.)
			// operands in the deadline form even when the early form is
			// requested: its failure tick varies per attempt and could not
			// be aligned (pairing unaligned pulses can PROVE false things:
			// tpalign_early p_or_bad).  The deadline form keeps every
			// assert verdict; an assumption reports late (tagged; spurious
			// counterexamples at worst, never false proofs).
			bool saved_early = early_fail;
			early_fail = false;
			SigSpec f1 = sub(0), f2 = prop_dispatch(port(1), child(1), start);
			int d1 = fail_off(child(0)), d2 = fail_off(child(1));
			early_fail = saved_early;
			if (saved_early && (d1 > 0 || d2 > 0))
				approx("late-failure-tick", op + " operands at " + std::string(log_id(n)));
			if (d1 < 0 || d2 < 0) {
				if (d1 != 0 || d2 != 0)
					approx("prop-binop-unaligned", op + " at " + std::string(log_id(n)));
				d1 = d2 = 0;
			}
			int D = std::max(d1, d2);
			SigSpec a_f = delay(f1, D - d1, "pbin"), b_f = delay(f2, D - d2, "pbin");
			if (op == "or")
				return mod->And(NEW_ID, a_f, b_f);
			// "holds" of an operand: its attempt reached its offset unfailed
			SigSpec a_h = delay(andN({delay(start, d1, "pbin_st"), mod->Not(NEW_ID, f1)}), D - d1, "pbin");
			if (op == "implies")
				return mod->And(NEW_ID, a_h, b_f);
			SigSpec b_h = delay(andN({delay(start, d2, "pbin_st"), mod->Not(NEW_ID, f2)}), D - d2, "pbin");
			return orN({mod->And(NEW_ID, a_h, b_f), mod->And(NEW_ID, a_f, b_h)});
		}
		if (op == "and")
			return orN({sub(0), prop_dispatch(port(1), child(1), start)});
		if (op == "not") {
			if (child(0)) {
				// 'not' of a bounded sequence is a safety property: it
				// fails exactly when the sequence matches.  Unbounded
				// sequences stay rejected (their complement is liveness).
				if (seq_shaped(child(0)) && seq_max_len(child(0)) >= 0 && seq_max_len(child(0)) <= 64)
					return seq_end(child(0), start, Const(1, 1));
				log_error("sva2check: 'not' of a temporal property in %s is not a safety property.\n", log_id(n));
			}
			return andN({start, mod->Not(NEW_ID, sub(0))});
		}
		if (op == "if")
			return prop_dispatch(port(1), child(1), andN({start, reduce1(port(0))}));
		if (op == "ifelse") {
			SigSpec c = reduce1(port(0));
			SigSpec fp = prop_dispatch(port(1), child(1), andN({start, c}));
			SigSpec fq = prop_dispatch(port(2), child(2), andN({start, mod->Not(NEW_ID, c)}));
			return orN({fp, fq});
		}
		if (op == "case") {
			// children: expr, then (value, property) pairs, optional default
			SigSpec e = port(0);
			int nargs = GetSize(ports);
			int npairs = (nargs - 1) / 2;
			bool has_default = (nargs - 1) % 2 == 1;
			SigSpec r = Const(0, 1);
			SigSpec matched = Const(0, 1);
			for (int i = 0; i < npairs; i++) {
				SigSpec eq = mod->Eq(NEW_ID, e, port(1 + 2 * i));
				matched = orN({matched, eq});
				r = orN({r, andN({start, eq, prop_dispatch(port(1 + 2 * i + 1), child(1 + 2 * i + 1), start)})});
			}
			if (has_default)
				r = orN({r, andN({start, mod->Not(NEW_ID, matched),
						prop_dispatch(port(nargs - 1), child(nargs - 1), start)})});
			return r;
		}
		if (op == "strong")
			return prop_dispatch(port(0), child(0), start);
		if (op == "weak") {
			// weak/strong differ only on finite traces (LRM 16.12.1); model
			// checking explores infinite traces, where a weak sequence
			// property carries the same completion obligation as strong.
			// Compiling it strongly is sound for PDR/ic3 and conservative
			// for finite simulation.
			if (child(0) && is_seq_op(sva_op(child(0)))) {
				approx("weak-as-strong", "weak at " + std::string(log_id(n)));
				return strong_seq_fail(child(0), start);
			}
			return prop_dispatch(port(0), child(0), start);
		}
		if (op == "always" || op == "s_always") {
			Cell *b = child(0);
			if (b && sva_op(b) == "bounds") {
				// children: bounds, property.  The property is evaluated at
				// every tick in the window, so the window level itself
				// becomes the start signal for the inner property.
				auto bp = ports_of(b);
				int m;
				if (!const_int(b->getPort(bp[0]), m))
					log_error("sva2check: non-constant always bound in %s.\n", log_id(n));
				SigSpec w;
				int hi = -1;
				if (bp.size() >= 2 && !const_int(b->getPort(bp[1]), hi))
					log_error("sva2check: non-constant always bound in %s.\n", log_id(n));
				if (hi < 0 && op == "s_always")
					log_error("sva2check: s_always needs a bounded range (LRM 16.12.11), not [m:$], in %s.\n", log_id(n));
				if (hi < 0) {
					// always [m:$]: every tick from start+m on.  feedback()
					// returns the REGISTER, so the current tick's term is
					// OR-ed back in (the bare register opened the window
					// one tick late: `always [1:$]` checked from start+2)
					SigSpec lo_now = delay(start, m, "always_lo");
					SigSpec held = feedback([=](SigSpec q) {
						return orN({lo_now, q});
					}, "always_wlo", 1);
					w = orN({lo_now, held});
				} else {
					w = Const(0, 1);
					for (int k = m; k <= hi; k++)
						w = orN({w, delay(start, k, "always_win")});
				}
				// each tick of the window spawns its own attempt
				SigSpec saved_age = age;
				dict<Wire *, std::pair<SigSpec, SigSpec>> saved_pending = pending_wr;
				if (slots_mode) {
					age = Const(0, nslots);
					age = sigset(age, 0, Const(1, 1)[0]);
					slot_epoch++;
					pending_wr.clear();
				}
				SigSpec r = prop_dispatch(port(1), child(1), w);
				age = saved_age;
				pending_wr = saved_pending;
				return r;
			}
			return sub(0);
		}
		if (op == "nexttime" || op == "s_nexttime") {
			// children: [property] or [const k, property].  Move the attempt
			// start k ticks forward (and the compile-time age with it) so
			// the inner property is evaluated AT t+k: expression wires are
			// continuous and slot reads use the shifted age, so no post-hoc
			// delay of the fail wire is needed (delaying the fail wire would
			// sample plain expressions at the attempt tick, one bug per k>1).
			int k = 1;
			int idx = 0;
			if (GetSize(ports) >= 2 && const_int(n->getPort(ports[0]), k))
				idx = 1;
			Cell *inner = child(idx);
			SigSpec start_k = delay(start, k, "nexttime");
			if (k > 0 && slots_mode) {
				SigSpec saved_age = age;
				dict<Wire *, std::pair<SigSpec, SigSpec>> saved_pending = pending_wr;
				for (int i = 0; i < k; i++)
					age = age_tick(age);
				slot_epoch++;
				pending_wr.clear();
				SigSpec r = inner ? prop_dispatch(port(idx), inner, start_k)
				                  : andN({start_k, mod->Not(NEW_ID, reduce1(port(idx)))});
				age = saved_age;
				pending_wr = saved_pending;
				return r;
			}
			return inner ? prop_dispatch(port(idx), inner, start_k)
			             : andN({start_k, mod->Not(NEW_ID, reduce1(port(idx)))});
		}
		if (op == "until" || op == "until_with") {
			SigSpec p = reduce1(resolve(port(0)));
			SigSpec q = reduce1(resolve(port(1)));
			return until_fail(p, q, start, op == "until_with");
		}
		if (op == "accept_on" || op == "sync_accept_on" || op == "reject_on" || op == "sync_reject_on") {
			SigSpec b = reduce1(resolve(port(0)));
			bool reject = op == "reject_on" || op == "sync_reject_on";
			SigSpec active = feedback([=](SigSpec qq) {
				return andN({orN({start, qq}), mod->Not(NEW_ID, b)});
			}, "abort_active", 1);
			SigSpec fail_p = prop_dispatch(port(1), child(1), start);
			if (reject)
				return orN({andN({active, b}), andN({active, fail_p})});
			return andN({active, fail_p});
		}
		if (op == "eventually" || op == "s_eventually") {
			// children: bounds, property
			Cell *b = child(0);
			if (!b || sva_op(b) != "bounds")
				log_error("sva2check: unbounded %s is liveness and cannot be checked by PDR in %s; bound it.\n", op.c_str(), log_id(n));
			auto bp = ports_of(b);
			int m;
			if (!const_int(b->getPort(bp[0]), m))
				log_error("sva2check: non-constant eventually bound in %s.\n", log_id(n));
			if (bp.size() < 2)
				log_error("sva2check: unbounded %s is liveness; give it a finite upper bound in %s.\n", op.c_str(), log_id(n));
			int hi;
			if (!const_int(b->getPort(bp[1]), hi))
				log_error("sva2check: non-constant eventually bound in %s.\n", log_id(n));
			if (hi < 0)
				log_error("sva2check: unbounded %s [m:$] is liveness; give it a finite upper bound in %s.\n", op.c_str(), log_id(n));
			// per-tick evaluation: each tick is its own attempt; the operand
			// in the deadline form (see the or / implies branch)
			bool saved_early = early_fail;
			early_fail = false;
			SigSpec fail_p;
			if (slots_mode) {
				SigSpec saved_age = age;
				dict<Wire *, std::pair<SigSpec, SigSpec>> saved_pending = pending_wr;
				age = Const(0, nslots);
				age = sigset(age, 0, Const(1, 1)[0]);
				slot_epoch++;
				pending_wr.clear();
				fail_p = prop_dispatch(port(1), child(1), Const(1, 1));
				age = saved_age;
				pending_wr = saved_pending;
			} else {
				fail_p = prop_dispatch(port(1), child(1), Const(1, 1));
			}
			// fail at t = s+hi+dp if p failed at every tick in [s+m, s+hi],
			// where p's attempt at j reports at j+dp (fail_off); the
			// per-tick evaluation start is reintroduced by the delays.
			// (dp was taken as 0: a temporal p's failure for one tick was
			// paired with another tick's -- PBT class F6.)
			int dp = fail_off(child(1));
			early_fail = saved_early;
			if (saved_early && dp > 0)
				approx("late-failure-tick", op + " operand at " + std::string(log_id(n)));
			if (dp < 0) {
				approx("eventually-unaligned", op + " at " + std::string(log_id(n)));
				dp = 0;
			}
			SigSpec r = delay(start, hi + dp, "ev_expire");
			for (int j = m; j <= hi; j++)
				r = mod->And(NEW_ID, r, delay(fail_p, hi - j, "ev_win"));
			return r;
		}

		log_error("sva2check: unsupported property operator '%s' in %s.\n", op.c_str(), log_id(n));
		return SigSpec();
	}

	// A sequence in property position: a sequence operator, or an
	// and/or whose operands are all sequences or Boolean leaves (the
	// frontend gives `a and b` one op for the property and the sequence
	// reading; with sequence operands the two coincide, LRM 16.13.4 /
	// 16.9.5: `not (te1 and te2)` fails exactly when the sequence
	// `te1 and te2` matches).
	bool seq_shaped(Cell *n)
	{
		if (!n) return true;
		std::string op = sva_op(n);
		if (is_seq_op(op)) return true;
		if (op != "and" && op != "or") return false;
		for (auto &p : ports_of(n)) {
			SigSpec s = n->getPort(p);
			Cell *c = len_child(s);
			if (c && !seq_shaped(c)) return false;
		}
		return true;
	}

	static bool is_seq_op(const std::string &op)
	{
		return op == "true" || op == "##" || op == "##[*]" || op == "##[+]" ||
				op == "[*]" || op == "[+]" || op == "[->]" || op == "[=]" ||
				op == "throughout" || op == "clocked" || op == "inside" ||
				op == "weq" || op == "wne" || op == "meq" ||
				op == "intersect" || op == "within" ||
				op == "first_match" || op == "seq_match";
	}

	SigSpec prop_dispatch(SigSpec s, Cell *c, SigSpec start)
	{
		static int depth_ctr = 0;
		if (++depth_ctr > 200)
			log_error("sva2check: runaway recursion in prop_dispatch.\n");
		struct DepthGuard { int *c; ~DepthGuard() { --*c; } } dg { &depth_ctr };
		if (c && getenv("SVA_DEBUG_OPS"))
			log("sva2check-debug: prop_dispatch %s (%s) in %s\n", sva_op(c).c_str(), log_id(c), check_name.c_str());
		if (c) {
			if (!is_seq_op(sva_op(c))) {
				// named instance: sequence declarations in property
				// position complete strongly; property/let declarations
				// compile as properties
				if (Cell *decl = decl_by_name(sva_op(c))) {
					auto actuals = inst_actuals(c);
					return inline_decl(decl, actuals, true, start, Const(1, 1));
				}
				return prop_fail(c, start);
			}
			// a sequence in property position completes strongly
			return strong_seq_fail(c, start);
		}
		if (slots_mode) {
			static std::vector<Wire *> no_formals;
			s = splice_expr(s, no_formals, false);
		}
		s = resolve(s);
		if (Cell *decl = decl_for_wire(s))
			return inline_decl(decl, {}, true, start, Const(1, 1));
		return andN({start, mod->Not(NEW_ID, reduce1(s))});
	}

	// Static maximum length (in clock ticks) of a sequence or property; -1 =
	// unbounded.  Covers property operators and named instances so slot
	// sizing sees through instance boundaries.  Memoized; a busy set turns
	// recursive declarations into -1.
	dict<Cell *, int> len_memo;
	pool<Cell *> len_busy;
	int seq_max_len(Cell *n)
	{
		if (!n)
			return 1;
		if (len_memo.count(n))
			return len_memo[n];
		if (!len_busy.insert(n).second)
			return -1;
		int r = seq_max_len_raw(n);
		len_busy.erase(n);
		len_memo[n] = r;
		return r;
	}

	// Static minimum length of a sequence in ticks (1 = a single boolean
	// tick); -1 = unknown/unbounded below.  Mirrors seq_max_len.
	dict<Cell *, int> mlen_memo;
	pool<Cell *> mlen_busy;
	int seq_min_len(Cell *n)
	{
		if (!n)
			return 1;
		if (mlen_memo.count(n))
			return mlen_memo[n];
		if (!mlen_busy.insert(n).second)
			return -1;
		int r = seq_min_len_raw(n);
		mlen_busy.erase(n);
		mlen_memo[n] = r;
		return r;
	}
	// A bare operand that names a sequence/property declaration (`a
	// throughout seq`): the frontend leaves a plain identifier wire, so
	// child_cell finds nothing and the length helpers used to assume 1 --
	// a one-tick deadline, and `gnt throughout seq` failed at frame 1
	// while the inline form proved (register v3 C1, sv-tests 16.7).
	// For LENGTHS the declaration's body stands in (the lowering itself
	// inlines through seq_dispatch with formals and scoping).
	Cell *len_child(SigSpec s)
	{
		if (Cell *c = child_cell(s))
			return c;
		if (Cell *d = decl_for_wire(s)) {
			auto dp = ports_of(d);
			return dp.empty() ? nullptr : child_cell(d->getPort(dp[0]));
		}
		return nullptr;
	}

	int seq_min_len_raw(Cell *n)
	{
		std::string op = sva_op(n);
		auto ports = ports_of(n);
		auto child = [&](int i) -> Cell * {
			return i < GetSize(ports) ? len_child(n->getPort(ports[i])) : nullptr;
		};
		auto cmin = [&](int i) -> int {
			Cell *c = child(i);
			return c ? seq_min_len(c) : 1;
		};
		if (decl_by_name(op)) {
			Cell *dbody = nullptr;
			std::vector<Wire *> dformals;
			SigSpec draw, bare;
			if (!lookup_decl(n, dbody, dformals, nullptr, &draw))
				return 1;
			dbody = dbody ? unwrap_spec(dbody, &bare) : nullptr;
			return dbody ? seq_min_len(dbody) : 1;
		}
		if (op == "true" || op == "inside" || op == "weq" || op == "wne")
			return 1;
		if (op == "##") {
			// two-port ##: leading-delay form `@(c) ##k body` (see seq_end)
			bool lead2 = GetSize(ports) == 2;
			int ti = lead2 ? 1 : 2;
			int lo, hi = -1;
			SigSpec cyc = n->getPort(ports[lead2 ? 0 : 1]);
			Cell *cc = child_cell(cyc);
			if (cc && sva_op(cc) == "bounds") {
				auto bp = ports_of(cc);
				if (!const_int(cc->getPort(bp[0]), lo)) return -1;
				if (GetSize(bp) >= 2 && !const_int(cc->getPort(bp[1]), hi)) return -1;
			} else if (cc) {
				return -1;
			} else {
				if (!const_int(cyc, lo)) return -1;
				hi = lo;
			}
			Cell *lhs = lead2 ? nullptr : child(0);
			int ll = lhs ? seq_min_len(lhs) : 1;
			Cell *tail = child(ti);
			int tl = tail ? seq_min_len(tail) : 1;
			if (ll < 0 || tl < 0) return -1;
			(void)hi;
			return ll + lo + tl - 1;
		}
		if (op == "[*]" || op == "[+]") {
			Cell *b = child(1);
			int m = op == "[*]" ? 0 : 1;
			if (b && sva_op(b) == "bounds") {
				auto bp = ports_of(b);
				int lo_v = 0;
				if (!bp.empty() && !const_int(b->getPort(bp[0]), lo_v))
					return -1;
				m = lo_v;
			}
			Cell *body = child(0);
			int bl = body ? seq_min_len(body) : 1;
			if (bl < 0) return -1;
			if (op == "[*]" && m == 0)
				return 0;  // [*0:hi] allows the empty match
			return m * bl;  // copies abut: m non-empty spans
		}
		if (op == "[->]")
			return 1;
		if (op == "[=]")
			return 1;   // [*0] tail: ends at the last occurrence, like [->]
		if (op == "throughout" || op == "clocked" || op == "first_match" ||
				op == "seq_match" || op == "within") {
			Cell *b = child(1);
			return b ? seq_min_len(b) : 1;
		}
		if (op == "intersect" || op == "and")
			return std::max(cmin(0), cmin(1));
		if (op == "or")
			return std::min(cmin(0), cmin(1));
		return 1;
	}
	int seq_max_len_raw(Cell *n)
	{
		std::string op = sva_op(n);
		auto ports = ports_of(n);
		auto child = [&](int i) -> Cell * {
			return i < GetSize(ports) ? len_child(n->getPort(ports[i])) : nullptr;
		};
		auto clen = [&](int i) -> int {
			Cell *c = child(i);
			return c ? seq_max_len(c) : 1;
		};
		// named instance: the declaration body's length
		if (decl_by_name(op)) {
			Cell *dbody = nullptr;
			std::vector<Wire *> dformals;
			SigSpec draw, bare;
			if (!lookup_decl(n, dbody, dformals, nullptr, &draw))
				return 1;
			dbody = dbody ? unwrap_spec(dbody, &bare) : nullptr;
			return dbody ? seq_max_len(dbody) : 1;
		}
		if (op == "true" || op == "inside" || op == "weq" || op == "wne")
			return 1;
		if (op == "##") {
			// two-port ##: leading-delay form `@(c) ##k body` (see seq_end)
			bool lead2 = GetSize(ports) == 2;
			int ti = lead2 ? 1 : 2;
			int lo, hi = -1;
			SigSpec cyc = n->getPort(ports[lead2 ? 0 : 1]);
			Cell *cc = child_cell(cyc);
			if (cc && sva_op(cc) == "bounds") {
				auto bp = ports_of(cc);
				if (!const_int(cc->getPort(bp[0]), lo)) return -1;
				if (GetSize(bp) >= 2) {
					if (!const_int(cc->getPort(bp[1]), hi)) return -1;
				} else {
					// ##[k] == ##k: single-bound bounds cell
					hi = lo;
				}
			} else if (cc) {
				return -1;
			} else {
				if (!const_int(cyc, lo)) return -1;
				hi = lo;
			}
			Cell *lhs = lead2 ? nullptr : child(0);
			int ll = lhs ? seq_max_len(lhs) : (lead2 ? 1 : clen(0));
			Cell *tail = child(ti);
			int tl = tail ? seq_max_len(tail) : clen(ti);
			// an unbounded tail ([->n], [*m:$]) makes the whole ## unbounded
			// (it used to clamp to 0, measuring `s ##0 b[->n]` as bounded)
			if (ll < 0 || tl < 0) return -1;
			// ticks: lhs spans ll (its last tick abuts the delay), the delay
			// adds hi, the tail adds its own length minus the shared tick
			return hi < 0 ? -1 : ll + hi + tl - 1;
		}
		if (op == "[*]" || op == "[+]") {
			Cell *b = child(1);
			if (b && sva_op(b) == "bounds") {
				auto bp = ports_of(b);
				int lo, hi;
				if (GetSize(bp) < 1 || !const_int(b->getPort(bp[0]), lo))
					return -1;
				if (GetSize(bp) >= 2) {
					if (!const_int(b->getPort(bp[1]), hi))
						return -1;
				} else {
					// [*N] exact repetition: the parser carries a
					// single-bound bounds cell (== [*N:N])
					hi = lo;
				}
				if (hi < 0) return -1;  // [*m:$]
				Cell *body = child(0);
				int bl = body ? seq_max_len(body) : 1;
				return hi * std::max(bl, 0);
			}
			return -1;
		}
		if (op == "[->]" || op == "[=]")
			return -1; // non-contiguous reps are unbounded in time
		if (op == "throughout") {
			Cell *b = child(1);
			return b ? seq_max_len(b) : 1;
		}
		if (op == "clocked") {
			Cell *b = child(1);
			return b ? seq_max_len(b) : 1;
		}
		if (op == "first_match" || op == "seq_match") {
			Cell *b = child(0);
			return b ? seq_max_len(b) : 1;
		}
		if (op == "within") {
			Cell *b = child(1);
			return b ? seq_max_len(b) : 1;
		}
		if (op == "intersect") {
			// both operands must match with the same length, so one
			// bounded operand bounds the intersection
			int a = clen(0), b = clen(1);
			if (a < 0 && b < 0) return -1;
			if (a < 0) return b;
			if (b < 0) return a;
			return std::max(a, b);
		}
		// property operators: all operands start at the same tick except
		// the consequent of a (possibly shifted) implication, which starts
		// where the antecedent ends
		if (op == "|->" || op == "|=>" || op == "#-#" || op == "#=#") {
			int alen = clen(0);
			int blen = clen(1);
			if (alen < 0 || blen < 0) return -1;
			int shift = op == "|=>" || op == "#=#";
			return std::max(alen, alen - 1 + blen + shift);
		}
		if (op == "implies" || op == "iff" || op == "or" || op == "and") {
			int a = clen(0), b = clen(1);
			if (a < 0 || b < 0) return -1;
			return std::max(a, b);
		}
		if (op == "not" || op == "strong" || op == "weak")
			return clen(0);
		if (op == "if")
			return clen(1);
		if (op == "ifelse")
			return std::max(clen(1), clen(2));
		if (op == "case") {
			int r = 1;
			int nargs = GetSize(ports);
			int npairs = (nargs - 1) / 2;
			bool has_default = (nargs - 1) % 2 == 1;
			for (int i = 0; i < npairs; i++)
				r = std::max(r, clen(1 + 2 * i + 1));
			if (has_default)
				r = std::max(r, clen(nargs - 1));
			return r;
		}
		if (op == "nexttime" || op == "s_nexttime") {
			int k = 1;
			if (GetSize(ports) >= 2 && const_int(n->getPort(ports[0]), k)) {
				int bl = clen(1);
				return bl < 0 ? -1 : k + bl;
			}
			int bl = clen(0);
			return bl < 0 ? -1 : k + bl;
		}
		if (op == "always" || op == "s_always" || op == "eventually" || op == "s_eventually") {
			// children: [bounds], property; the window ticks each spawn
			// their own attempt, so only the body's own length adds
			Cell *b = child(0);
			bool has_bounds = b && sva_op(b) == "bounds";
			int bl = has_bounds ? clen(1) : clen(0);
			if (bl < 0) return -1;
			if (!has_bounds)
				return bl;
			auto bp = ports_of(b);
			int hi;
			if (GetSize(bp) < 2 || !const_int(b->getPort(bp[1]), hi) || hi < 0)
				return -1; // unbounded always/eventually ([m:$])
			return hi + bl;
		}
		if (op == "until" || op == "until_with")
			return -1; // unbounded
		if (op == "accept_on" || op == "sync_accept_on" || op == "reject_on" || op == "sync_reject_on")
			return clen(1);
		return 1;
	}

	// A sequence used as a property must complete strongly: the attempt
	// must finish within its (bounded) maximum length.
	//
	// seq_max_len() counts the trailing boolean tick, but the completion
	// signal from seq_end() already fires at the sequence's last tick, so
	// the expiry offset is len-1: the old len fired one tick late, and its
	// extra window term (a completion at the expiry tick) let a later
	// attempt's completion excuse an earlier attempt's missed deadline --
	// silently dropped violations, e.g. `a |-> ##1 !a` with a=1,1.
	// LRM 16.13.22 (a): a sequence used as a property shall not admit an
	// empty match.  (Its empty match used to count as a match at the
	// attempt tick, e.g. `a |-> b[*0:1]` held with b low.)
	void refuse_empty_prop(Cell *n)
	{
		if (can_empty(n))
			log_error("sva2check: sequence in %s is used as a property but can match empty (LRM 16.13.22).\n", log_id(n));
	}

	SigSpec strong_seq_fail(Cell *n, SigSpec start)
	{
		refuse_empty_prop(n);
		int len = seq_max_len(n);
		if (len < 0 || len > 64)
			log_error("sva2check: sequence in %s used in property position has no finite bound (max %d ticks).\n", log_id(n), len);
		int deadline = len - 1;
		int minl = seq_min_len(n);
		// A variable-length sequence: the window [t, t+deadline] of the
		// attempt started at t also sees the ends of LATER attempts, which
		// used to excuse its miss -- `g throughout (1 ##[1:3] x)` was
		// PROVED with the attempt at g's low tick unmatchable (register v3
		// C1).  Exact form: deadline+1 rotating monitor copies; copy i
		// starts only the attempts at ticks with phase == i, so it has one
		// attempt in flight and every end inside that attempt's window is
		// its own.  A fixed-length sequence has a one-tick window (below)
		// and is exact already; slot mode (locals) and windows > 16 keep
		// the merged form, tagged.
		// (multi-clock islands also keep it: the rotating copies on the
		// island clock took tmcdom1 from 27 s to > 10 min in ic3)
		// Early failure (register v3 D5): an attempt fails at the first
		// tick no thread of it survives (LRM: Fig 16-12 "at clock tick 9
		// ... thereby failing"), not at its deadline -- the deadline-only
		// check was one or more ticks late (PBT class F5) and let a short
		// trace slip past a sequence ASSUMPTION.  Each copy runs one
		// attempt; while ff_log records its monitor registers, "no end yet
		// and every register's next state is 0" means no end can come
		// (start_i stays low until the copy's next attempt).  Copies whose
		// monitor holds counters or slot state (which can be 0 while a
		// thread waits) keep the deadline check only.  Fixed-length
		// sequences take the copies too: their verdict was already exact,
		// their failure tick was not.
		bool merged_modes = slots_mode || island_mode || in_island || deadline + 1 > 16 || getenv("SVA_MERGED_STRONG");
		if (early_fail && deadline > 0 && merged_modes)
			approx("late-failure-tick", std::string(log_id(n)));
		if (deadline > 0 && minl != len && merged_modes) {
			approx("strong-seq-merged-attempts", std::string(log_id(n)));
		} else if (deadline > 0 && !merged_modes && (minl != len || early_fail)) {
			int copies = deadline + 1;
			int w = 1;
			while ((1 << w) < copies) w++;
			Wire *ph = wn("ssf_phase", w);
			SigSpec wrap = mod->Eq(NEW_ID, SigSpec(ph), Const(copies - 1, w));
			SigSpec nxt = mod->Mux(NEW_ID, mod->Add(NEW_ID, SigSpec(ph), Const(1, w)), Const(0, w), wrap);
			mod->addDff(NEW_ID, clk, nxt.extract(0, w), ph, clk_pos);
			SigSpec fail = Const(0, 1);
			for (int i = 0; i < copies; i++) {
				SigSpec start_i = andN({start, mod->Eq(NEW_ID, SigSpec(ph), Const(i, w))});
				std::vector<std::pair<SigSpec, bool>> log, *saved_log = ff_log;
				ff_log = &log;
				SigSpec end_i = seq_end(n, start_i, Const(1, 1));
				ff_log = saved_log;
				bool early = early_fail && !log.empty();
				for (auto &e : log)
					early = early && e.second;
				if (early_fail && !early && i == 0)
					approx("late-failure-tick", std::string(log_id(n)));
				SigSpec dl_i = delay(start_i, deadline, "strong_expire");
				if (!early) {
					// deadline check: this attempt's ends in its window
					SigSpec any_i = Const(0, 1);
					for (int k = 0; k <= len - std::max(minl, 1); k++)
						any_i = orN({any_i, k == 0 ? end_i : delay(end_i, k, "strong_win")});
					fail = orN({fail, andN({dl_i, mod->Not(NEW_ID, any_i)})});
					continue;
				}
				SigSpec live = Const(0, 1);
				for (auto &e : log)
					live = orN({live, mod->ReduceOr(NEW_ID, e.first)});
				// matched: an end since this copy's attempt started
				// (same tick counts); act: the attempt is in flight
				SigSpec matched = since(end_i, start_i);
				Wire *act_q = wn("ssf_act", 1);
				SigSpec act = orN({start_i, SigSpec(act_q)});
				SigSpec open = andN({act, mod->Not(NEW_ID, matched)});
				SigSpec dead = orN({dl_i, mod->Not(NEW_ID, live)});
				SigSpec fail_i = andN({open, dead});
				ffd(dis_gate(andN({open, mod->Not(NEW_ID, fail_i)})), act_q, true);
				fail = orN({fail, fail_i});
			}
			return fail;
		}
		// Checked at the attempt's deadline s+len-1: its non-empty ends
		// lie in [s+max(minl,1)-1, s+len-1], i.e. 0 .. len-max(minl,1)
		// ticks back (an empty match never satisfies a sequence property,
		// LRM 16.12.1: `d[*0]` has no window and always fails).  The window
		// used to reach `deadline` ticks back, into ends of EARLIER
		// attempts: `(t==2||t==3) |-> (b ##1 1)` was PROVED with b low at
		// 3, the attempt at 2's end at 3 excusing the attempt at 3 -- a
		// fixed-length sequence consequent after any passing attempt
		// (register v3 C1, found through the LRM Figure 16-7 transcription,
		// 2026-10-19).
		SigSpec end = seq_end(n, start, Const(1, 1));
		SigSpec any = Const(0, 1);
		for (int k = 0; k <= len - std::max(minl, 1); k++)
			any = orN({any, k == 0 ? end : delay(end, k, "strong_win")});
		return andN({delay(start, deadline, "strong_expire"), mod->Not(NEW_ID, any)});
	}
};

struct Sva2CheckPass : public Pass {
	Sva2CheckPass() : Pass("sva2check", "compile $sva property trees into $check EN/A form") { }

	void help() override
	{
		log("\n");
		log("    sva2check [-expose_bad] [-exact] [-early] [selection]\n");
		log("\n");
		log("Compile the $sva property trees under assert-flavor $check cells into\n");
		log("synchronous monitor hardware (single-clock spec subset: ## delays and\n");
		log("ranges, repetitions, |->/|=>, boolean property operators, always/\n");
		log("nexttime bounds, until, abort operators), then lower each $check to\n");
		log("EN=1'b1, A = disable || !fail with \\CLK and \\CLK_POLARITY attributes.\n");
		log("v1 multi-clock: a |->/|=> consequent `clocked` on a different clock\n");
		log("compiles as its own island on that clock; such $check cells keep\n");
		log("EN=1'b1 and carry no \\CLK attributes.  Other multi-clock elements\n");
		log("abort with an error, as do local variables, within, and matches.\n");
		log("Unbounded eventually/s_eventually of a plain expression lowers to a\n");
		log("FLAVOR \"liveness\" $check (A = p, EN = 1'b1) for the native ic3\n");
		log("liveness engine; temporal operands keep the \"bound it\" error.\n");
		log("The same shape under assume/restrict flavor lowers to a $fair cell\n");
		log("(a fairness constraint: the predicate must hold infinitely often; ic3\n");
		log("prunes its liveness gfp to survivor states that can still reach every\n");
		log("$fair predicate within the set, so only fair !p-cycles disprove).\n");
		log("a |->/|=> consequent of unbounded (s_)eventually with a plain boolean\n");
		log("operand lowers the same way through a pending-bit monitor\n");
		log("(A = !pend || b, bad = pend && !b).\n");
		log("\n");
		log("Every $check the pass touches gets (* sva_lowering = ... *): \"exact\",\n");
		log("\"approx:<kind>[,<kind>]\" naming each documented approximation taken\n");
		log("(each is also a warning with the check's name), \"placeholder:<why>\"\n");
		log("for a skipped check whose A port is still the frontend placeholder,\n");
		log("\"immediate\" / \"plain\" for non-temporal checks the frontend already\n");
		log("compiled.  With -exact, taking an approximation is an error.\n");
		log("\n");
		log("A sequence in property position fails at its deadline (its longest\n");
		log("match length) by default.  Assume/restrict checks, and every check\n");
		log("with -early, report the failure at the tick no thread of the attempt\n");
		log("survives (the LRM's tick): the late form lets a short trace slip past\n");
		log("an assumption.  -early on asserts changes no verdict, only failure\n");
		log("ticks, and costs one monitor copy per attempt slot.\n");
		log("\n");
		log("With -expose_bad, additionally create a module output port per\n");
		log("converted check named bad_<check> driven by EN && !A, for AIGER\n");
		log("export to external model checkers.\n");
		log("\n");
	}

	// Two-state formal semantics for case-equality against x/z constants.
	// The frontend lowers `$isunknown(e)` (OpenTitan's ASSERT_KNOWN) to a
	// `$countbits(e, 'x, 'z) > 0`, i.e. `$eqx(bit, 1'bx)` per bit.  The
	// formal engines have no x: satgen (ic3, sat) and write_smt2 read an x
	// constant bit as 0, so `$eqx(bit, 1'bx)` became `bit == 0` and every
	// ASSERT_KNOWN on a zero-initialised signal was "disproved at depth 0"
	// (found by the circuit-mutation leg, register v3 C3: 202 OpenTitan
	// disproofs with no circuit in their cone).  In a two-state model no
	// signal ever case-equals a constant with an x or z bit: fold $eqx to
	// 0 and $nex to 1, once, loudly.  `sim -xprop` keeps four-state
	// semantics because it does not run this pass.
	static void fold_x_compares(RTLIL::Module *mod)
	{
		std::vector<RTLIL::Cell *> dead;
		int n = 0;
		for (auto cell : mod->cells()) {
			if (!cell->type.in(ID($eqx), ID($nex)))
				continue;
			bool has_xz = false;
			for (auto port : {ID::A, ID::B})
				for (auto bit : cell->getPort(port))
					if (bit.wire == nullptr && (bit.data == RTLIL::State::Sx || bit.data == RTLIL::State::Sz))
						has_xz = true;
			if (!has_xz)
				continue;
			SigSpec y = cell->getPort(ID::Y);
			RTLIL::Const val(cell->type == ID($eqx) ? RTLIL::State::S0 : RTLIL::State::S1, GetSize(y));
			mod->connect(y, val);
			dead.push_back(cell);
			n++;
		}
		for (auto cell : dead)
			mod->remove(cell);
		if (n)
			log_warning("%s: folded %d case-equality compare(s) against an x/z constant to a constant "
					"(two-state formal semantics: `$isunknown` is never true; `sim -xprop` keeps four-state).\n",
					log_id(mod), n);
	}

	// Does `sig` evaluate to a constant through its combinational drivers?
	// A small local evaluator: constants, inverters of constants, and
	// and/or gates with a dominant constant input (`dis || 1`, the shape a
	// disable iff leaves around a constant body).  Conservative: anything
	// it cannot see through is "not constant".
	static bool const_cone(RTLIL::Module *mod, SigSpec sig)
	{
		// module-level `connect`s (a wire tied to a constant, as the
		// frontend's folded $isunknown and fold_x_compares leave behind)
		// are what SigMap resolves; follow cells from there
		SigMap sm(mod);
		return const_cone_rec(mod, sm, sig, 0);
	}

	static bool const_cone_rec(RTLIL::Module *mod, SigMap &sm, SigSpec sig, int depth)
	{
		sig = sm(sig);
		if (sig.is_fully_const())
			return true;
		if (depth > 16)
			return false;
		if (GetSize(sig) != 1) {
			// the frontend pads $sva operands to 32 bits ({31'0, x}): a
			// vector is constant when every bit is
			for (auto b : sig)
				if (!const_cone_rec(mod, sm, SigSpec(b), depth + 1))
					return false;
			return true;
		}
		SigBit bit = sig[0];
		RTLIL::Cell *drv = nullptr;
		for (auto cell : mod->cells())
			for (auto &conn : cell->connections())
				if (cell->output(conn.first))
					for (auto b : sm(conn.second))
						if (b == bit)
							drv = cell;
		if (!drv)
			return false;
		if (drv->type.in(ID($not), ID($logic_not), ID($reduce_bool), ID($reduce_or), ID($reduce_and)))
			return const_cone_rec(mod, sm, drv->getPort(ID::A), depth + 1);
		if (drv->type.in(ID($or), ID($logic_or), ID($and), ID($logic_and))) {
			bool is_or = drv->type.in(ID($or), ID($logic_or));
			SigSpec a = sm(drv->getPort(ID::A)), b = sm(drv->getPort(ID::B));
			bool ca = const_cone_rec(mod, sm, a, depth + 1), cb = const_cone_rec(mod, sm, b, depth + 1);
			if (ca && cb)
				return true;
			// a dominant constant on one side decides the gate
			for (auto s : {a, b})
				if (GetSize(s) == 1 && s.is_fully_const()) {
					bool v = s[0].data == RTLIL::State::S1;
					if ((is_or && v) || (!is_or && !v))
						return true;
				}
			return false;
		}
		return false;
	}

	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing SVA2CHECK pass (compile $sva trees into $check cells).\n");

		bool expose_bad = false;
		bool exact_mode = false;
		bool early_opt = false;
		std::vector<std::string> sub_args;
		for (auto &arg : args) {
			if (arg == "-expose_bad") {
				expose_bad = true;
				continue;
			}
			if (arg == "-exact") {
				exact_mode = true;
				continue;
			}
			if (arg == "-early") {
				early_opt = true;
				continue;
			}
			sub_args.push_back(arg);
		}
		extra_args(sub_args, 1, design);

		int converted = 0, unsupported = 0;

		for (auto mod : design->selected_modules())
		{
			fold_x_compares(mod);

			// named sequence/property/let declarations visible to instances
			dict<std::string, Cell *> decls;
			for (auto cell : mod->cells())
				if (cell->type == "\\$sva") {
					std::string op = sva_op(cell);
					if ((op == "sequence_decl" || op == "property_decl" || op == "let_decl") &&
							cell->hasParam(ID(NAME).c_str())) {
						std::string nm = cell->getParam(ID(NAME).c_str()).decode_string();
						if (!nm.empty() && nm[0] == '\\')
							nm = nm.substr(1);
						decls[nm] = cell;
					}
				}

			// E1: ## next to an empty-capable operand, once per module and
			// before the declaration-body scan so new cells are protected
			SvaCompiler(mod, SigSpec(), true, "$probe", nullptr).rewrite_empty_delays();

			// bodies of declarations are shared between instances: never
			// drop them as part of a single check's consumed tree
			pool<Cell *> decl_bodies;
			{
				std::vector<Cell *> stack;
				for (auto &it : decls) {
					Cell *decl = it.second;
					auto dp = SvaCompiler(mod, SigSpec(), true, "$probe", nullptr).ports_of(decl);
					if (dp.empty())
						continue;
					Cell *b = SvaCompiler(mod, SigSpec(), true, "$probe", nullptr).child_cell(decl->getPort(dp[0]));
					if (b && !decl_bodies.count(b)) {
						decl_bodies.insert(b);
						stack.push_back(b);
					}
				}
				while (!stack.empty()) {
					Cell *c = stack.back();
					stack.pop_back();
					SvaCompiler probe(mod, SigSpec(), true, "$probe", nullptr);
					for (auto port : probe.ports_of(c)) {
						Cell *d = probe.child_cell(c->getPort(port));
						if (d && !decl_bodies.count(d)) {
							decl_bodies.insert(d);
							stack.push_back(d);
						}
					}
				}
			}

			pool<Cell *> lowered_sv;
			pool<Cell *> done_checks;
			// one $check per snapshot pass: processing it removes its
			// consumed $sva tree, so a per-check fresh selected_cells()
			// snapshot is required (the stale ones hold freed cells)
			for (bool again = true; again;) {
				again = false;
				std::vector<Cell *> allsel;
				for (auto c : mod->selected_cells())
					allsel.push_back(c);
				for (auto cell : allsel) {
					if (cell->type != ID($check) || done_checks.count(cell))
						continue;
					done_checks.insert(cell);
					again = true;

				std::string flavor = cell->getParam(ID(FLAVOR).c_str()).decode_string();
				if (flavor == "liveness")
					// already compiled in place (eventually lowering) or
					// emitted pre-compiled by the s_until split (<name>$live
					// twin); not part of the assert/assume conversion loop
					continue;
				if (flavor != "assert" && flavor != "assume" && flavor != "restrict") {
					unsupported++;
					log("Skipping %s/%s: flavor %s not supported.\n", log_id(mod), log_id(cell), flavor.c_str());
					cell->attributes[ID(sva_lowering)] = Const("placeholder:flavor " + flavor);
					// B5/B1: no $sva placeholder may reach a backend.  A cover
					// that is not checked is "never reached": tie its
					// condition to 0 so the spec tree is unreferenced (the
					// end-of-pass cleanup drops it) and write_smt2 / sim see a
					// plain $cover; the tag says it was not checked.
					if (flavor == "cover")
						cell->setPort(ID::A, Const(0, 1));
					continue;
				}

				SvaCompiler comp(mod, SigSpec(), true, stringf("$sva%s", log_id(cell)), &decls);
				comp.check_name = log_id(cell);
				if (getenv("SVA_DEBUG_OPS"))
					log("sva2check-debug: check %s\n", log_id(cell));
				comp.exact_mode = exact_mode;
				{
					std::string fl = cell->getParam(ID(FLAVOR)).decode_string();
					comp.early_fail = early_opt || getenv("SVA_EARLY") || fl == "assume" || fl == "restrict";
				}

				SigSpec aport = cell->getPort(ID(A));
				Cell *topcell = aport.size() == 1 ? comp.find_sva_driver(aport[0]) : nullptr;
				if (!topcell) {
					// bare name reference: assert property (p) leaves an
					// undriven wire named after the declaration as the A port
					std::string cn;
					if (aport.size() == 1 && aport[0].is_wire())
						cn = aport[0].wire->name.str();
					if (cn.empty())
						cn = log_id(cell);
					if (!cn.empty() && cn[0] == '\\')
						cn = cn.substr(1);
					if (decls.count(cn))
						topcell = decls[cn];
				}
				if (!topcell) {
					// Plain expression property (no temporal operators): the
					// $check EN/A form from the frontend is already final;
					// async2sync/chformal lower edge-triggered TRG forms.
					// (This used to `break` out of the module loop, so any
					// immediate assertion that happened to be visited first
					// silently left EVERY temporal check in the module as an
					// unconnected placeholder -- 22 of 159 OpenTitan prim
					// files, ~half their checks.)
					if (!cell->attributes.count(ID(sva_lowering)))
						cell->attributes[ID(sva_lowering)] = Const(std::string(
								cell->getParam(ID::TRG_ENABLE).as_bool() ? "plain" : "immediate"));
					continue;
				}

				// Unwrap property instances: assert property (p(args)) has no
				// spec wrapper of its own; the clock/disable live in the
				// declaration body's spec.  Bind instance actuals to formals
				// along the chain.
				Cell *spec = topcell;
				std::vector<std::tuple<Cell *, std::vector<Wire *>, std::vector<SigSpec>>> chain;
				// a clocked SEQUENCE declaration used as the whole property
				// (`sequence s; @(posedge clk) ...; endsequence  assert
				// property (s);`, LRM 16.16 clock inference) ends the chain at
				// its `clocked` node: clock + body, no disable -- the same
				// shape the spec classification below reads (it was skipped
				// as "not a spec" and ic3 printed a verdict on the
				// placeholder; register v3 C1, sv-tests chapter 16)
				auto is_spec = [&](Cell *c) { return sva_op(c) == "spec" || sva_op(c) == "clocked"; };
				while (!is_spec(spec)) {
					std::string sop = sva_op(spec);
					if (sop == "sequence_decl" || sop == "property_decl" || sop == "let_decl") {
						// bare name reference resolves straight to the decl
						auto dp = comp.ports_of(spec);
						chain.emplace_back(spec, std::vector<Wire *>(), std::vector<SigSpec>());
						spec = dp.empty() ? nullptr : comp.child_cell(spec->getPort(dp[0]));
					} else {
						Cell *dbody;
						std::vector<Wire *> dformals;
						if (!comp.lookup_decl(spec, dbody, dformals) || !dbody)
							break;
						chain.emplace_back(spec, dformals, comp.inst_actuals(spec));
						spec = dbody;
					}
					if (!spec)
						break;
					if (chain.size() > 64)
						log_error("sva2check: recursive property instantiation involving %s.\n", log_id(cell));
				}
				// the parser binds `@(posedge clk) a ##1 b` in a sequence
				// declaration as ##(clocked(clk, a), b): the clock sits on
				// the LEFTMOST element although it governs the whole
				// sequence (LRM 16.16).  Walk the leftmost operands to that
				// `clocked` node; its edge is the property clock and the
				// whole tree the body (the inner `clocked` passes through
				// when its clock equals the property clock).
				Cell *hoist_clock = nullptr;
				if (spec && !is_spec(spec)) {
					Cell *c = spec;
					for (int d = 0; c && d < 64; d++) {
						std::string op = sva_op(c);
						if (op == "clocked") {
							auto cp = comp.ports_of(c);
							Cell *edge = cp.empty() ? nullptr : comp.child_cell(c->getPort(cp[0]));
							if (edge && (sva_op(edge) == "posedge" || sva_op(edge) == "negedge" || sva_op(edge) == "edge"))
								hoist_clock = edge;
							break;
						}
						if (op == "spec" || op == "sequence_decl" || op == "property_decl" || op.empty())
							break;
						auto cp = comp.ports_of(c);
						c = cp.empty() ? nullptr : comp.child_cell(c->getPort(cp[0]));
					}
				}
				if (!spec || (!is_spec(spec) && !hoist_clock)) {
					unsupported++;
					log("Skipping %s/%s: property is not a $sva spec.\n", log_id(mod), log_id(cell));
					cell->attributes[ID(sva_lowering)] = Const(std::string("placeholder:not a spec"));
					continue;
				}
				for (auto &t : chain) {
					auto &f = std::get<1>(t);
					auto &a = std::get<2>(t);
					if (GetSize(f) != GetSize(a))
						log_error("sva2check: instance of %s has %d actuals, declaration has %d formals.\n",
								sva_op(std::get<0>(t)).c_str(), GetSize(a), GetSize(f));
					for (int i = 0; i < GetSize(f); i++)
						comp.subst[f[i]] = a[i];
				}

				// Classify spec arguments: clock cell, disable cell, body.
				// A bare wire body that names a declaration (the frontend
				// leaves bare `assert property (@(clk) p)` references as
				// plain identifier wires) advances the instance chain.
				Cell *clock = nullptr, *disable = nullptr, *body = nullptr;
				SigSpec body_sig, body_const;
				bool bad = false;
				for (int guard = 0; guard < 64 && !hoist_clock; guard++) {
					clock = nullptr, disable = nullptr, body = nullptr;
					body_sig = SigSpec(), body_const = SigSpec();
					bad = false;
					for (auto port : comp.ports_of(spec)) {
						SigSpec sp = spec->getPort(port);
						if (sp.empty())
							{ bad = true; break; }
						if (sp.is_fully_const()) {
							// constant property body (e.g. ... 1'b1): the
							// violation condition is its complement; a
							// second const port is malformed
							if (!body && body_sig.empty() && body_const.empty())
								body_const = sp;
							else
								bad = true;
							continue;
						}
						Cell *drv = comp.child_cell(sp);
						if (drv) {
							std::string op = sva_op(drv);
							if ((op == "posedge" || op == "negedge" || op == "edge" || op == "no_clock") && !clock)
								clock = drv;
							else if ((op == "disable_iff" || op == "no_disable") && !disable)
								disable = drv;
							else if (!body)
								body = drv;
							else
								bad = true;
						} else if (!body && body_sig.empty()) {
							body_sig = sp;
						} else {
							bad = true;
						}
					}
					if (bad || body)
						break;
					if (body_sig.empty() || !body_sig[0].is_wire())
						break;
					std::string cn = body_sig[0].wire->name.str();
					if (!cn.empty() && cn[0] == '\\')
						cn = cn.substr(1);
					auto it = decls.find(cn);
					if (it == decls.end())
						break;
					chain.emplace_back(it->second, std::vector<Wire *>(), std::vector<SigSpec>());
					auto dp = comp.ports_of(it->second);
					Cell *next = dp.empty() ? nullptr : comp.child_cell(it->second->getPort(dp[0]));
					if (!next) {
						body_sig = SigSpec();
						break;
					}
					spec = next;
					if (!is_spec(spec))
						break;
				}

				if (hoist_clock) {
					clock = hoist_clock;
					body = spec;
				}
				if (bad || (!body && body_sig.empty() && body_const.empty())) {
					unsupported++;
					log("Skipping %s/%s: malformed spec.\n", log_id(mod), log_id(cell));
					cell->attributes[ID(sva_lowering)] = Const(std::string("placeholder:malformed spec"));
					continue;
				}

				std::string clop = clock ? sva_op(clock) : "no_clock";
				SigSpec clksig = clock ? clock->getPort(ID(A)) : SigSpec();
				if (clop == "no_clock" || clksig.empty() || !clksig[0].is_wire()) {
					unsupported++;
					log("Skipping %s/%s: no usable sampling clock.\n", log_id(mod), log_id(cell));
					cell->attributes[ID(sva_lowering)] = Const(std::string("placeholder:no sampling clock"));
					continue;
				}
				comp.clk = clksig[0];
				comp.clk_pos = clop != "negedge";
				if (disable && sva_op(disable) == "disable_iff")
					comp.dis = comp.reduce1(disable->getPort(ID(A)));

				if (!body && body_sig.empty() && !body_const.empty()) {
					// Constant property body (e.g. assert property
					// (@(posedge clk) 1'b1)): no temporal compile needed.
					// $check A carries the "property holds" condition
					// (assert bad := EN && !A, per the build in ic3 and
					// write_smt2), so it is the constant itself; EN=1 stays
					// from the frontend and the spec tree is dropped by the
					// end-of-pass cleanup once no $check references it.
					// A disable_iff on such a spec is ignored: conservative
					// for the true body, and the false body is degenerate.
					// the frontend pads $sva const ports to 32 bits;
					// the boolean value is the low bit
					SigSpec ca = SigSpec(body_const[0]);
					if (disable && sva_op(disable) == "disable_iff")
						ca = mod->Or(NEW_ID, comp.reduce1(disable->getPort(ID(A))), ca);
					cell->setPort(ID(A), ca);
					// the body IS a constant here (see above): say so in the provenance
					cell->attributes[ID(sva_lowering)] = Const(std::string("exact:const"));
					converted++;
					break;
				}
				comp.lower_sampled_value_cells(lowered_sv);
				comp.lower_sequence_methods();

				// v1 liveness: an unbounded eventually/s_eventually whose
				// operand is a plain boolean expression is F p with p a
				// state predicate; the ic3 pass proves it via a gfp over
				// (reachable ∧ ¬p).  Lower to A = p (NOT negated), EN =
				// 1'b1, FLAVOR "liveness".  A temporal operand (a non-bounds
				// $sva child) keeps the existing "bound it" error via
				// prop_fail below.
				bool liveness = false;
				if (flavor == "assert" && body &&
						(sva_op(body) == "eventually" || sva_op(body) == "s_eventually")) {
					auto bp = comp.ports_of(body);
					Cell *bcell = bp.empty() ? nullptr : comp.child_cell(body->getPort(bp[0]));
					liveness = !bcell;
				}

				// v2 liveness, nested-temporal response: the consequent of
				// |->/|=> is an unbounded (s_)eventually whose operand is a
				// plain boolean.  Anything else on the consequent (bounded
				// eventually, temporal operand) keeps the prop_fail path:
				// bounded lowers as safety, temporal hits the existing clear
				// error, so only the genuinely uncheckable shapes fall.
				bool tresp = false;
				Cell *tev = nullptr;
				if (flavor == "assert" && !liveness && body &&
						(sva_op(body) == "|->" || sva_op(body) == "|=>")) {
					auto ip = comp.ports_of(body);
					Cell *cc = ip.size() == 2 ? comp.child_cell(body->getPort(ip[1])) : nullptr;
					if (cc && (sva_op(cc) == "eventually" || sva_op(cc) == "s_eventually")) {
						auto ep = comp.ports_of(cc);
						if (ep.size() == 1 && !comp.child_cell(cc->getPort(ep[0]))) {
							tresp = true;
							tev = cc;
						}
					}
				}

				// A concurrent assertion in an initial block, and `expect`
				// (LRM 16.15.6, 16.18), is ONE attempt, at the first clock
				// tick.  The frontend marks it like an initial immediate
				// check (TRG_ENABLE, no trigger), and async2sync then gated
				// the compiled A with $initstate: only step 0 was checked,
				// so `initial assert property (@(posedge clk) a ##1 b)` never
				// saw its failure at step 1 (and ic3 read $initstate as 0, so
				// even `a` was PROVED; register v3 C1, 2026-10-19).  Compile
				// it as an ordinary clocked check whose only attempt starts
				// in the initial state.
				bool once = cell->getParam(ID::TRG_ENABLE).as_bool() && cell->getParam(ID::TRG_WIDTH).as_int() == 0;
				SigSpec root = Const(1, 1);
				if (once) {
					if (liveness || tresp)
						log_error("sva2check: unbounded eventually in an initial-block check %s is not supported.\n", log_id(cell));
					root = mod->Initstate(NEW_ID);
					cell->setParam(ID::TRG_ENABLE, State::S0);
				}

				SigSpec fail;
				SigSpec new_a;
				bool strong_until = false;
				bool fairness = false;
				SigSpec su_p, su_q;
				if ((flavor == "assume" || flavor == "restrict") && body &&
						(sva_op(body) == "eventually" || sva_op(body) == "s_eventually")) {
					// fairness: assume/restrict of an unbounded
					// (s_)eventually with a plain boolean operand is a
					// FAIRNESS constraint (the predicate must hold
					// infinitely often), not a proof obligation; lower to
					// a $fair cell (the chformal-documented meaning:
					// assume(s_eventually ...)).  The ic3 liveness engine
					// prunes its gfp survivor set to states that can still
					// reach every $fair predicate within the set, so only
					// fair !p-cycles disprove.  A temporal operand keeps
					// the "bound it" error (same as the assert flavor).
					auto bp = comp.ports_of(body);
					Cell *bcell = bp.empty() ? nullptr : comp.child_cell(body->getPort(bp[0]));
					if (bcell)
						log_error("sva2check: unbounded %s with a temporal operand is not supported in %s; bound it.\n",
								sva_op(body).c_str(), log_id(cell));
					pool<Cell *> seen;
					if (comp.tree_has_locals(body, seen))
						log_error("sva2check: local variables in unbounded %s are not supported in %s.\n",
								sva_op(body).c_str(), log_id(cell));
					SigSpec psig = comp.resolve(bp.empty() ? SigSpec() : body->getPort(bp[0]));
					if (Cell *decl = comp.decl_for_wire(psig))
						log_error("sva2check: named declaration operand of unbounded %s is not supported in %s.\n",
								sva_op(body).c_str(), log_id(cell));
					new_a = comp.reduce1(psig);
					fairness = true;
				} else if (liveness) {
					pool<Cell *> seen;
					if (comp.tree_has_locals(body, seen))
						log_error("sva2check: local variables in unbounded %s are not supported in %s.\n",
								sva_op(body).c_str(), log_id(cell));
					SigSpec psig = comp.resolve(comp.ports_of(body).empty() ? SigSpec()
							: body->getPort(comp.ports_of(body)[0]));
					if (Cell *decl = comp.decl_for_wire(psig))
						log_error("sva2check: named declaration operand of unbounded %s is not supported in %s.\n",
								sva_op(body).c_str(), log_id(cell));
					// A = p directly (ic3: bad := !A = !p); fail = !p for
					// -expose_bad
					new_a = comp.reduce1(psig);
					fail = mod->Not(NEW_ID, new_a);
				} else if (tresp) {
					// Violation of (a |=> F b) is an attempt whose antecedent
					// completes at t with no b from t+1 on (|->: from t on,
					// the consequent window includes the end tick).  Track
					// "some attempt outstanding" in one global pend bit on
					// the property clock; the violation is then exactly a
					// reachable cycle in (pend && !b) = !A, which is what
					// the existing liveness engine decides.
					//   Sound: on such a cycle b is false everywhere and
					//     pend stays high; the clear-dominant D means the
					//     bit can only be high entering the cycle if an
					//     attempt opened with no b since -- a b at or after
					//     the open tick would have zeroed it, and an open
					//     pulse coincident with a b opens a window that
					//     contains that very tick, so the b satisfies it
					//     too.  (Set-dominant would instead keep demanding
					//     b across a b at the open tick -- a false
					//     violation for e.g. b on the |-> end tick or the
					//     |=> first consequent tick.)
					//   Complete: a genuine violation never clears the bit
					//     after the open tick, so its tail is such a cycle.
					//   One bit suffices because every window is a suffix
					//     [open, inf): a b at tick t lies in every
					//     outstanding window (all opened at <= t), so one b
					//     clears all of them -- the gfp only needs "some
					//     obligation outstanding", not a count.
					if (comp.multiclock)
						log_error("sva2check: temporal response in a multi-clock property is not supported in %s.\n", log_id(cell));
					pool<Cell *> seen;
					if (comp.tree_has_locals(body, seen))
						log_error("sva2check: local variables in a %s temporal response are not supported in %s.\n",
								sva_op(body).c_str(), log_id(cell));
					auto ip = comp.ports_of(body);
					// antecedent end pulse, compiled exactly as prop_fail's
					// implication branch does (every tick attempts)
					SigSpec open = comp.seq_dispatch(body->getPort(ip[0]),
							comp.child_cell(body->getPort(ip[0])), Const(1, 1), Const(1, 1));
					// |=> evaluates the consequent one tick after the
					// antecedent end (same shift as prop_fail); the open
					// pulse must land on the obligation's first window tick
					// so that a b there lies inside the window and clears
					if (sva_op(body) == "|=>") {
						open = comp.delay(open, 1, "impl");
						if (comp.can_empty(comp.child_cell(body->getPort(ip[0]))))
							open = comp.orN({open, Const(1, 1)});  // see prop_fail
					}
					SigSpec bsig = comp.resolve(tev->getPort(comp.ports_of(tev)[0]));
					if (Cell *decl = comp.decl_for_wire(bsig))
						log_error("sva2check: named declaration operand of unbounded %s is not supported in %s.\n",
								sva_op(tev).c_str(), log_id(cell));
					SigSpec b1 = comp.reduce1(bsig);
					SigSpec pend = comp.feedback([=](SigSpec q) {
						return mod->And(NEW_ID, mod->Not(NEW_ID, b1), mod->Or(NEW_ID, open, q));
					}, "tresp_pend", 1);
					// A = !pend || b (ic3: bad := !A = pend && !b);
					// fail = !A for -expose_bad
					new_a = mod->Or(NEW_ID, mod->Not(NEW_ID, pend), b1);
					fail = mod->Not(NEW_ID, new_a);
					liveness = true;
				} else {
					// v1.5 strong until: p s_until q == (p until q) && F q.
					// The weak monitor keeps this $check, the F q
					// obligation is emitted below as a "<name>$live"
					// liveness twin (same scheme as the eventually v1
					// lowering).  q may be a plain boolean or a bounded
					// sequence: in both cases "q holds at t" is a 1-bit
					// wire -- for a sequence, its match pulse attempted
					// at every tick (the LRM property promotion of a
					// sequence).  The same wire feeds the weak monitor
					// and the F twin.  p may be a plain boolean or a
					// bounded property: a temporal p compiles to its
					// per-tick failure pulse (prop_dispatch at start =
					// 1 every tick, like the bounded eventually
					// operand) -- "p holds at tick t" is "the p attempt
					// starting at t completes", whose violation pulse
					// fires at t's own deadline, exactly the sampling
					// the weak monitor performs.  A bounded sequence in
					// property position completes strongly (the usual
					// finite-bound rule).  s_until_with keeps the
					// boolean-only p (the with-variant's q&p completion
					// conjunct is not defined for temporal p).
					bool su_with = false;
					if (body && (sva_op(body) == "s_until" || sva_op(body) == "s_until_with")) {
						auto bp = comp.ports_of(body);
						Cell *qc = bp.size() == 2 ? comp.child_cell(body->getPort(bp[1])) : nullptr;
						Cell *pc = bp.size() == 2 ? comp.child_cell(body->getPort(bp[0])) : nullptr;
						if (bp.size() == 2 && (!pc || sva_op(body) == "s_until") &&
								(!qc || (comp.seq_max_len(qc) >= 0 && comp.seq_max_len(qc) <= 64))) {
							strong_until = true;
							su_with = sva_op(body) == "s_until_with";
							if (!pc) {
								su_p = comp.reduce1(comp.resolve(body->getPort(bp[0])));
							} else {
								pool<Cell *> seen;
								if (comp.tree_has_locals(pc, seen))
									log_error("sva2check: local variables in a s_until operand are not supported in %s.\n", log_id(body));
								SigSpec saved_age = comp.age;
								dict<Wire *, std::pair<SigSpec, SigSpec>> saved_pending = comp.pending_wr;
								if (comp.slots_mode) {
									comp.age = Const(0, comp.nslots);
									comp.age = sigset(comp.age, 0, Const(1, 1)[0]);
									comp.slot_epoch++;
									comp.pending_wr.clear();
								}
								// per-tick failure pulse, inverted: the
							// monitor samples the "p holds" level
							su_p = comp.prop_dispatch(body->getPort(bp[0]), pc, Const(1, 1));
							su_p = mod->Not(NEW_ID, su_p);
								comp.age = saved_age;
								comp.pending_wr = saved_pending;
							}
							if (!qc) {
								su_q = comp.reduce1(comp.resolve(body->getPort(bp[1])));
							} else {
								pool<Cell *> seen;
								if (comp.tree_has_locals(qc, seen))
									log_error("sva2check: local variables in a s_until operand are not supported in %s.\n", log_id(body));
								comp.refuse_empty_prop(qc);
								su_q = comp.seq_end(qc, Const(1, 1), Const(1, 1));
							}
						}
					}
					if (strong_until) {
						fail = comp.until_fail(su_p, su_q, Const(1, 1), su_with);
					} else if (body) {
						pool<Cell *> seen;
						bool had_slots = comp.slots_mode;
						if (comp.tree_has_locals(body, seen)) {
							int l = comp.seq_max_len(body);
							if (l < 0 || l > 64)
								log_error("sva2check: local variables over an unbounded sequence in %s.\n", log_id(body));
							comp.init_slots(l);
						}
						fail = comp.prop_dispatch(SigSpec(), body, root);
						if (!had_slots && comp.slots_mode) {
							// slot state is per-property: do not leak it into
							// the next $check of this module
							comp.slots_mode = false;
							comp.nslots = 0;
							comp.age = SigSpec();
							comp.slotfiles.clear();
							comp.pending_wr.clear();
						}
					} else if (Cell *dcell = comp.decl_for_wire(comp.resolve(body_sig))) {
						// a bare declaration name under the outer spec's
						// clock: `assert property (@(posedge clk) q)` with an
						// UNCLOCKED `property q; ...`.  The declaration body
						// is the property (instance with no actuals).  This
						// used to fall through to the plain-wire branch below
						// and lower to !q on the undriven name wire -- a
						// silent wrong lowering (found by the A3 positive
						// controls: no detector ever fired on named
						// properties).
						auto dp = comp.ports_of(dcell);
						Cell *dbody = dp.empty() ? nullptr : comp.child_cell(dcell->getPort(dp[0]));
						bool had_slots = comp.slots_mode;
						pool<Cell *> seen;
						if (dbody && comp.tree_has_locals(dbody, seen)) {
							int l = comp.seq_max_len(dbody);
							if (l < 0 || l > 64)
								log_error("sva2check: local variables over an unbounded sequence in %s.\n", log_id(dbody));
							comp.init_slots(l);
						}
						fail = comp.prop_dispatch(body_sig, nullptr, root);
						if (!had_slots && comp.slots_mode) {
							comp.slots_mode = false;
							comp.nslots = 0;
							comp.age = SigSpec();
							comp.slotfiles.clear();
							comp.pending_wr.clear();
						}
					} else {
						fail = mod->And(NEW_ID, root, mod->Not(NEW_ID, comp.reduce1(body_sig)));
					}
					new_a = mod->Not(NEW_ID, fail);
				}

				std::string dop = disable ? sva_op(disable) : "no_disable";
				SigSpec dsig;
				if (dop == "disable_iff") {
					dsig = comp.reduce1(disable->getPort(ID(A)));
					new_a = mod->Or(NEW_ID, dsig, new_a);
				}

				if (once && (strong_until || fairness))
					log_error("sva2check: %s in an initial-block check %s is not supported.\n",
							strong_until ? "s_until" : "a fairness assumption", log_id(cell));
				cell->setPort(ID(EN), SigSpec(State::S1));
				cell->setPort(ID(A), new_a);
				if (liveness)
					cell->setParam(ID(FLAVOR), std::string("liveness"));
				if (fairness) {
					// become the $fair cell; A/EN were just set, the
					// $check-only params/ports go away
					cell->type = ID($fair);
					cell->parameters.clear();
					cell->unsetPort(ID(TRG));
					cell->unsetPort(ID(ARGS));
				}
				if (!comp.multiclock) {
					cell->attributes[ID(CLK)] = Const(clksig[0].wire->name.str());
					cell->attributes[ID(CLK_POLARITY)] = Const(comp.clk_pos ? 1 : 0, 1);
				} else {
					// the property monitors on more than one clock, so
					// there is no single sampling clock to name: keep
					// EN=1'b1 (A = !fail is checked at every formal step;
					// the monitor FFs of both islands only move on real
					// clock edges, so fail can only change on one) and
					// omit the clock attributes, which would otherwise
					// misleadingly pin the outer clock
					log("sva2check: %s: multi-clock property; $check left unclocked (checked at every step).\n", log_id(cell));
				}
				cell->attributes[ID(sva2check)] = Const(1, 1);
				{
					// a check whose compiled condition is a constant (ASSERT_KNOWN
					// under two-state semantics, `r || !r`, a body the frontend
					// folded) proves nothing about the circuit: say so in the
					// provenance, where every verdict printer shows it
					std::string tag = comp.lowering_tag();
					if (const_cone(mod, cell->getPort(ID::A)))
						tag += ":const";
					cell->attributes[ID(sva_lowering)] = Const(tag);
				}

				if (strong_until) {
					if (comp.multiclock)
						log_error("sva2check: s_until in a multi-clock property is not supported in %s.\n", log_id(cell));
					// F q twin: the ic3 pass proves FLAVOR "liveness"
					// $checks with its two-phase gfp engine.  Named
					// <name>$live so the battery (and humans) can tell
					// the legs apart; run_battery folds it into the base
					// property verdict.  Mirror every parameter/port of
					// the main cell (TRG/ARGS/PRIORITY/...), overriding
					// FLAVOR, A and EN -- a partial $check trips
					// parameter assertions in downstream passes.
					RTLIL::Cell *live = mod->addCell(cell->name.str() + "$live", ID($check));
					live->parameters = cell->parameters;
					live->connections_ = cell->connections_;
					live->attributes = cell->attributes;
					live->setParam(ID(FLAVOR), std::string("liveness"));
					// `disable` names the disable cell even for no_disable,
					// so gate on the flavor, not the pointer (an Or with an
					// empty dsig port would sit in A otherwise)
					SigSpec la = su_q;
					if (dop == "disable_iff")
						la = mod->Or(NEW_ID, dsig, la);
					live->setPort(ID(A), la);
					live->setPort(ID(EN), SigSpec(State::S1));
					live->attributes[ID(sva2check)] = Const(1, 1);
				}

				if (expose_bad && !fairness) {
					// fairness cells carry no bad_ export: the predicate
					// is an assumption, not a fail condition
					Wire *bad = mod->addWire("\\" + std::string("bad_") + log_id(cell));
					bad->port_output = true;
					mod->connect(bad, fail);
					mod->fixup_ports();
				}

				// Drop the consumed $sva tree cells and their placeholder Y wires.
				std::vector<Cell *> drop;
				pool<Cell *> seen;
				auto enqueue = [&](Cell *c) {
					// a cell already removed during compilation (cloned
					// trees, sampled-value lowering) must not be dropped
					// again: module->remove() deletes the object
					if (c && c->module == mod && mod->cell(c->name) == c &&
							!seen.count(c) && !decl_bodies.count(c)) {
						seen.insert(c);
						drop.push_back(c);
					}
				};
				enqueue(spec);
				enqueue(clock);
				enqueue(disable);
				for (auto &t : chain)
					enqueue(std::get<0>(t));
				for (size_t i = 0; i < drop.size(); i++) {
					Cell *c = drop[i];
					for (auto port : comp.ports_of(c)) {
						SigSpec sp = c->getPort(port);
						enqueue(comp.child_cell(sp));
					}
				}
				pool<Wire *> dropwires;
				for (auto c : drop) {
					SigSpec y = c->getPort(ID(Y));
					if (!y.empty() && y.size() == 1 && y[0].is_wire() &&
							y[0].wire->name.str().rfind("$auto$genrtlil.cc:", 0) == 0)
						dropwires.insert(y[0].wire);
					mod->remove(c);
				}
				mod->remove(dropwires);

				converted++;
					break;
				}
			}

			// drop leftover $sva cells: declarations consumed by inlining
			// and any trees no live $check references.  Trees of skipped
			// (unsupported) checks stay so their $check A ports stay valid.
			pool<Cell *> protected_cells;
			{
				SvaCompiler probe(mod, SigSpec(), true, "$sva_cleanup", nullptr);
				std::vector<Cell *> stack;
				for (auto cell : mod->cells())
					if (cell->type == ID($check)) {
						SigSpec ap = cell->getPort(ID(A));
						if (ap.size() == 1 && probe.by_y.count(ap[0]) && !protected_cells.count(probe.by_y[ap[0]])) {
							protected_cells.insert(probe.by_y[ap[0]]);
							stack.push_back(probe.by_y[ap[0]]);
						}
					}
				while (!stack.empty()) {
					Cell *c = stack.back();
					stack.pop_back();
					for (auto port : probe.ports_of(c)) {
						Cell *d = probe.child_cell(c->getPort(port));
						if (d && !protected_cells.count(d)) {
							protected_cells.insert(d);
							stack.push_back(d);
						}
					}
				}
			}
			pool<Cell *> leftovers;
			pool<Wire *> leftoverwires;
			for (auto cell : mod->cells())
				if (cell->type == "\\$sva" && !protected_cells.count(cell)) {
					SigSpec y = cell->getPort(ID(Y));
					if (!y.empty() && y.size() == 1 && y[0].is_wire() &&
							y[0].wire->name.str().rfind("$auto$genrtlil.cc:", 0) == 0)
						leftoverwires.insert(y[0].wire);
					leftovers.insert(cell);
				}
			for (auto c : leftovers)
				mod->remove(c);
			mod->remove(leftoverwires);

			// sample-value cells no check claimed (clockless or skipped
			// contexts): degrade to the data argument as the frontend used
			// to, so no unknown cell type reaches downstream passes
			{
				std::vector<Cell *> rm;
				bool warned = false;
				std::vector<Cell *> scan(mod->cells().begin(), mod->cells().end());
				for (auto cell : scan) {
					if (!cell->type.in("\\$past", "\\$rose", "\\$fell", "\\$stable", "\\$changed"))
						continue;
					if (lowered_sv.count(cell))
						continue;
					if (!warned) {
						warned = true;
						log("Warning: sample-value function in a clockless context in %s; degraded to its data argument.\n", log_id(mod));
					}
					SigSpec a = cell->getPort(ID(A));
					SigSpec y = cell->getPort(ID(Y));
					if (cell->type.in("\\$rose", "\\$fell"))
						mod->connect(y, SigSpec(a[0]));
					else
						mod->connect(y, a);
					rm.push_back(cell);
				}
				for (auto cell : rm)
					mod->remove(cell);
			}
		}

		log("Converted %d $check cells, %d unsupported.\n", converted, unsupported);
	}
} Sva2CheckPass;

PRIVATE_NAMESPACE_END
