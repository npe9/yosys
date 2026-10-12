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
//   v1 multi-clock: the direct consequent of |->/|=> may be a `clocked`
//     sub-sequence on a different clock (optionally under strong/weak):
//     it compiles as its own monitor island on the inner clock, started by
//     the antecedent end/start pulse; the enclosing $check keeps EN=1'b1
//     and carries no CLK attributes (checked at every formal step; use the
//     clk2fflogic prep so both islands advance under interleaving semantics)
// Rejected with a clear message: any other multi-clock element (mid-sequence
//   or antecedent `clocked`, `clocked` under throughout/within/first_match,
//   or_event/bare-identifier clocking events, instance bodies), unbounded
//   eventually over a temporal operand, s_until over a temporal operand,
//   local variable assignments, within, matches/streaming.
//
// Strong until v1: p s_until q / s_until_with with p, q plain booleans
//   splits into the weak-until monitor on the main $check plus an F q
//   obligation as a second $check named <name>$live with FLAVOR
//   "liveness"; the ic3 pass proves each leg independently.
//
// Sequences compile to nondeterministic-start transducers shared across
// attempts: compile_seq(node, start, gate) returns an "ends at this tick"
// pulse (existential over parses).  Properties compile to a per-tick
// violation pulse: compile_prop(node, start) with start = 1 at the top
// level (every tick is an attempt start) or an antecedent-end pulse for
// spawned consequents.  The $check gets EN=1'b1, A = disable || !fail.

#include "kernel/yosys.h"
#include <cstdio>

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

	// State register with next-state function fed back from Q.
	SigSpec feedback(const std::function<SigSpec(SigSpec)> &f, const char *role, int width)
	{
		Wire *q = wn(role, width);
		mod->addDff(NEW_ID, clk, f(q), q, clk_pos);
		return q;
	}

	SigSpec reg(SigSpec d, const char *role)
	{
		Wire *q = wn(role, d.size());
		mod->addDff(NEW_ID, clk, d, q, clk_pos);
		return q;
	}

	SigSpec delay(SigSpec s, int k, const char *role = "delay")
	{
		for (int i = 0; i < k; i++)
			s = reg(s, role);
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
		for (auto cell : rm)
			mod->remove(cell);
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
				for (int j = 0; j < nslots; j++)
					acc = sigset(acc, j, mod->Or(NEW_ID, acc[j], cur[j]));
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
			mod->addDff(NEW_ID, clk, next, pq, clk_pos);
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
				sf.dff[k] = mod->addDff(NEW_ID, clk, next, sf.q[k], clk_pos);
		}
		pending_wr[xw] = {we, e};
		slot_epoch++;
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
							k == 0 ? le.end : delay(le.end, k, "dspl"), gate, depth + 1);
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
			if (m == 0)
				r.push_back(SvaEnd{andN({start, gate_now(gate)}), 0});
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
			if (hi < 0) {
				// unbounded: tail may start at any tick >= end + lo
				SigSpec trig = feedback([=](SigSpec q) {
					return orN({delay(s_end, lo), q});
				}, "delay_trig", 1);
				t_start = trig;
				if (slots_mode)
					t_age = age_window(age, lo, nslots - 1);
			} else if (hi <= 128) {
				// single register chain with one tap per admissible offset
				t_start = lo == 0 ? s_end : Const(0, 1);
				SigSpec tap = s_end;
				for (int k = 1; k <= hi; k++) {
					tap = reg(tap, "delay");
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
			if (split_ranges && hi >= 0 && !in_split && getenv("SVA_NOSPLIT") == nullptr) {
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
					auto sp = seq_ends_split(n, SigSpec(), start, gate);
					if (sp.second) {
						SigSpec r = Const(0, 1);
						for (auto &e : sp.first)
							r = orN({r, e.end});
						return r;
					}
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
			Cell *body_cell = child_cell(body);
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
				}
			}
			if (op == "[*]" || op == "[+]") {
				// consecutive repetition
				if (m == 0 && hi < 0) {
					// [*]: empty or any run length; the run state still
					// matters for the upper bound only, so the end is
					// "active at the start tick or at any run tick"
					SigSpec run_end = rep_run_end(body_cell, body, start, gate, 1);
					return orN({andN({start, gate_now(gate)}), run_end});
				}
				if (hi >= 0 && hi <= 8) {
					// bounded: chain hi copies, tap ends at >= m
					return rep_chain_end(body_cell, body, start, gate, m, hi);
				}
				// [*m:$]: run of >= m; sub-sequence bodies fall back to the
				// same run form only for boolean bodies, else bounded split
				if (body_cell && sva_op(body_cell) != "true")
					log_error("sva2check: unbounded repetition of a sequence in %s; bound it to <= 8.\n", log_id(n));
				return rep_run_end(body_cell, body, start, gate, std::max(m, 1));
			}
			// non-contiguous repetitions of a boolean element
			if (body_cell && sva_op(body_cell) != "true")
				log_error("sva2check: non-contiguous repetition of a sequence in %s is not supported.\n", log_id(n));
			SigSpec bsig = andN({reduce1(body), gate_now(gate)});
			int cap = std::max(m, 1);
			int w = 1;
			while ((1 << w) <= cap) w++;
			// an attempt counts b's from its start tick on; attempts are
			// merged existentially, so counting starts at the first start
			Wire *started_w = wn("rep_started");
			Wire *cnt_w = wn("rep_cnt", w);
			mod->addDff(NEW_ID, clk, orN({start, started_w}), started_w, clk_pos);
			SigSpec sum = mod->Add(NEW_ID, cnt_w, SigSpec(bsig));
			SigSpec sat = mod->Eq(NEW_ID, cnt_w, Const(cap, w));
			SigSpec capped = mod->Mux(NEW_ID, sum, Const(cap, w), sat);
			mod->addDff(NEW_ID, clk, mod->Mux(NEW_ID, Const(0, w), capped, orN({start, started_w})), cnt_w, clk_pos);
			SigSpec cnt = cnt_w;
			SigSpec enough = mod->Ge(NEW_ID, cnt, Const(m, w));
			if (op == "[->]")
				return andN({bsig, enough});
			// [=m:n]: ends one or more ticks after the last b
			SigSpec idle = mod->Not(NEW_ID, reduce1(body));
			SigSpec cnt_d = delay(cnt, 1, "rep_cnt_d");
			SigSpec enough_d = mod->Ge(NEW_ID, cnt_d, Const(m, w));
			return andN({idle, gate_now(gate), enough_d});
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

		if (op == "intersect") {
			// both operands must match with the same start and the same end
			SigSpec a_end = seq_dispatch(port(0), child(0), start, gate);
			SigSpec b_end = seq_dispatch(port(1), child(1), start, gate);
			return mod->And(NEW_ID, a_end, b_end);
		}

		if (op == "within") {
			// a within b  ==  (1[*0:$] ##1 a ##1 1[*0:$]) intersect b
			// (LRM 16.9.10): b matches and a matches some subinterval.
			// a may start at ANY tick of b's span, so dispatch a at each
			// admissible offset; b's end tick is the composite's end.
			Cell *bc = child(1);
			int len = bc ? seq_max_len(bc) : 1;
			if (len < 0 || len > 32)
				log_error("sva2check: within over an unbounded sequence in %s.\n", log_id(n));
			SigSpec b_end = seq_dispatch(port(1), bc, start, gate);
			SigSpec r = Const(0, 1);
			for (int k = 0; k <= len; k++) {
				SigSpec a_start = k == 0 ? start : delay(start, k, "within");
				SigSpec a_end = seq_dispatch(port(0), child(0), a_start, gate);
				SigSpec any = a_end;
				for (int j = 1; j <= len - k; j++)
					any = orN({any, delay(a_end, j, "within")});
				r = orN({r, andN({b_end, any})});
			}
			return r;
		}

		if (op == "first_match") {
			// ( seq [, x = expr [, ...]] ): of all parses of an attempt,
			// only the earliest-ending one survives (LRM 16.9.9); local
			// variables sampled at that end.  Exact per-offset splitting
			// when the tree has no locals; with locals the shared slot
			// file approximation stays (documented).
			SigSpec end;
			if (!slots_mode && child(0)) {
				end = seq_end_fm(child(0), start, gate).first;
			} else {
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
				SigSpec e = seq_dispatch(port(ti), child(ti), t_any, gate);
				return std::make_pair(e, any);
			}
			fm_budget -= span;
			// .first: per-offset tail compiles; end_k survives only if no
			// earlier offset produced an end at or before it for this
			// attempt.  An earlier offset's end sits at most (hi-lo) +
			// tail-length behind the candidate end (the offset gap plus
			// the tail's own stretch; beyond that the ends belong to
			// other attempts -- documented approximation).
			int w = (hi - lo) + (child(ti) ? seq_max_len(child(ti)) : 1);
			if (w < 0 || w > 64) {
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
			if (la < 0 || la > 64 || lb < 0 || lb > 64)
				return std::make_pair(any, any);
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
			return leaf();  // unbounded run: approximate
		}
		if (op == "true" || op == "inside" || op == "weq" || op == "wne" ||
				op == "meq" || op == "assign" || op == "[->]" || op == "[=]" ||
				op == "within" || op == "##[*]" || op == "##[+]" || op == "seq_match")
			return leaf();
		if (Cell *decl = decl_by_name(op))
			return leaf();  // instance body: approximate (documented)
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
	SigSpec rep_run_end(Cell *body_cell, SigSpec body, SigSpec start, SigSpec gate, int m)
	{
		if (body_cell) {
			// sub-sequence run: iterate the fixed-point rep_end(t) =
			// start | s_end(t) | (s_end | rep_cont)(t-1)
			SigSpec s_end = seq_end(body_cell, andN({start, gate_now(gate)}), gate);
			SigSpec cont = feedback([=](SigSpec q) {
				return orN({delay(s_end, 1), q});
			}, "rep_cont", 1);
			return orN({andN({start, gate_now(gate)}), s_end, cont});
		}
		SigSpec b = andN({reduce1(body), gate_now(gate)});
		if (m <= 1)
			return andN({b, orN({start, Const(1, 1)})}) ; // run>=1 end = b
		int w = 1;
		while ((1 << w) <= m) w++;
		SigSpec run = feedback([=](SigSpec q) {
			SigSpec sum = mod->Add(NEW_ID, q, SigSpec(b));
			// saturate at m; compare the register (not the sum) so the
			// increment cannot overflow the counter width
			SigSpec sat = mod->Eq(NEW_ID, q, Const(m, w));
			SigSpec capped = mod->Mux(NEW_ID, sum, Const(m, w), sat);
			// b ? capped : 0 (a plain And would zero-extend b and
			// clear the upper state bits)
			return mod->Mux(NEW_ID, Const(0, w), capped, b);
		}, "rep_run", w);
		return andN({b, mod->Ge(NEW_ID, run, Const(m, w))});
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
		SigSpec active = feedback([=](SigSpec qq) {
			return orN({start, qq});
		}, "until_active", 1);
		SigSpec q_seen = feedback([=](SigSpec qq) {
			SigSpec hit = with ? andN({q, p}) : q;
			// restart at each start, otherwise accumulate
			return orN({andN({start, hit}), mod->And(NEW_ID, orN({qq, hit}), mod->Not(NEW_ID, start))});
		}, "until_qseen", 1);
		// until: at t either p holds (continue) or q holds (complete);
		//        fail if neither and no q has been seen since the last start
		// until_with: q completes only together with p; fail if p fails
		//        before such a q
		SigSpec fail_now = with ? andN({active, mod->Not(NEW_ID, p)})
		                        : andN({active, mod->Not(NEW_ID, p), mod->Not(NEW_ID, q)});
		if (with)
			return andN({fail_now, mod->Not(NEW_ID, orN({q, q_seen}))});
		return andN({fail_now, mod->Not(NEW_ID, q_seen)});
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
			SigSpec ant_end = seq_dispatch(port(0), child(0), start, Const(1, 1));
			bool shift = op == "|=>" || op == "#=#";
			SigSpec c_start = shift ? delay(ant_end, 1, "impl") : ant_end;
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
						if (cbc)
							r = strong_seq_fail(cbc, pulse);
						else {
							static std::vector<Wire *> no_formals;
							r = andN({pulse,
									mod->Not(NEW_ID, reduce1(splice_expr(cbody, no_formals, false)))});
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
		if (op == "implies")
			// fail = a && !b = !fail(a) && fail(b)
			return mod->And(NEW_ID, mod->Not(NEW_ID, sub(0)), prop_dispatch(port(1), child(1), start));
		if (op == "iff") {
			SigSpec a = sub(0), b = prop_dispatch(port(1), child(1), start);
			return orN({mod->And(NEW_ID, a, mod->Not(NEW_ID, b)), mod->And(NEW_ID, mod->Not(NEW_ID, a), b)});
		}
		if (op == "or")
			return mod->And(NEW_ID, sub(0), prop_dispatch(port(1), child(1), start));
		if (op == "and")
			return orN({sub(0), prop_dispatch(port(1), child(1), start)});
		if (op == "not") {
			if (child(0)) {
				// 'not' of a bounded sequence is a safety property: it
				// fails exactly when the sequence matches.  Unbounded
				// sequences stay rejected (their complement is liveness).
				if (is_seq_op(sva_op(child(0))) && seq_max_len(child(0)) >= 0 && seq_max_len(child(0)) <= 64)
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
			if (child(0) && is_seq_op(sva_op(child(0))))
				return strong_seq_fail(child(0), start);
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
				if (bp.size() < 2) {
					w = feedback([=](SigSpec q) {
						return orN({delay(start, m, "always_lo"), q});
					}, "always_wlo", 1);
				} else {
					int hi;
					if (!const_int(b->getPort(bp[1]), hi))
						log_error("sva2check: non-constant always bound in %s.\n", log_id(n));
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
				SigSpec r = inner ? prop_fail(inner, start_k)
				                  : andN({start_k, mod->Not(NEW_ID, reduce1(port(idx)))});
				age = saved_age;
				pending_wr = saved_pending;
				return r;
			}
			return inner ? prop_fail(inner, start_k)
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
			// per-tick evaluation: each tick is its own attempt
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
			// fail at t = s+hi if p failed at every tick in [s+m, s+hi];
			// the per-tick evaluation start is reintroduced by the delays
			SigSpec r = delay(start, hi, "ev_expire");
			for (int j = m; j <= hi; j++)
				r = mod->And(NEW_ID, r, delay(fail_p, hi - j, "ev_win"));
			return r;
		}

		log_error("sva2check: unsupported property operator '%s' in %s.\n", op.c_str(), log_id(n));
		return SigSpec();
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
	int seq_min_len_raw(Cell *n)
	{
		std::string op = sva_op(n);
		auto ports = ports_of(n);
		auto child = [&](int i) -> Cell * {
			return i < GetSize(ports) ? child_cell(n->getPort(ports[i])) : nullptr;
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
			return 2;
		if (op == "throughout" || op == "clocked" || op == "first_match" ||
				op == "seq_match" || op == "within") {
			Cell *b = child(1);
			return b ? seq_min_len(b) : 1;
		}
		if (op == "intersect")
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
			return i < GetSize(ports) ? child_cell(n->getPort(ports[i])) : nullptr;
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
				if (GetSize(bp) >= 2 && !const_int(cc->getPort(bp[1]), hi)) return -1;
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
			if (ll < 0) return -1;
			// ticks: lhs spans ll (its last tick abuts the delay), the delay
			// adds hi, the tail adds its own length minus the shared tick
			return hi < 0 ? -1 : ll + hi + std::max(tl, 0) - 1;
		}
		if (op == "[*]" || op == "[+]") {
			Cell *b = child(1);
			if (b && sva_op(b) == "bounds") {
				auto bp = ports_of(b);
				int hi;
				if (GetSize(bp) < 2 || !const_int(b->getPort(bp[1]), hi))
					return -1;
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
			int a = clen(0), b = clen(1);
			if (a < 0 || b < 0) return -1;
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
			if (GetSize(bp) < 2 || !const_int(b->getPort(bp[1]), hi))
				return -1; // unbounded always/eventually
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
	SigSpec strong_seq_fail(Cell *n, SigSpec start)
	{
		int len = seq_max_len(n);
		if (len < 0 || len > 64)
			log_error("sva2check: sequence in %s used in property position has no finite bound (max %d ticks).\n", log_id(n), len);
		int deadline = len - 1;
		SigSpec end = seq_end(n, start, Const(1, 1));
		SigSpec any = Const(0, 1);
		for (int k = 0; k <= deadline; k++)
			any = orN({any, k == 0 ? end : delay(end, k, "strong_win")});
		return andN({delay(start, deadline, "strong_expire"), mod->Not(NEW_ID, any)});
	}
};

struct Sva2CheckPass : public Pass {
	Sva2CheckPass() : Pass("sva2check", "compile $sva property trees into $check EN/A form") { }

	void help() override
	{
		log("\n");
		log("    sva2check [-expose_bad] [selection]\n");
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
		log("\n");
		log("With -expose_bad, additionally create a module output port per\n");
		log("converted check named bad_<check> driven by EN && !A, for AIGER\n");
		log("export to external model checkers.\n");
		log("\n");
	}

	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing SVA2CHECK pass (compile $sva trees into $check cells).\n");

		bool expose_bad = false;
		std::vector<std::string> sub_args;
		for (auto &arg : args) {
			if (arg == "-expose_bad") {
				expose_bad = true;
				continue;
			}
			sub_args.push_back(arg);
		}
		extra_args(sub_args, 1, design);

		int converted = 0, unsupported = 0;

		for (auto mod : design->selected_modules())
		{
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
					{ again = false; break; }
				}

				SvaCompiler comp(mod, SigSpec(), true, stringf("$sva%s", log_id(cell)), &decls);

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
					{ again = false; break; }
				}

				// Unwrap property instances: assert property (p(args)) has no
				// spec wrapper of its own; the clock/disable live in the
				// declaration body's spec.  Bind instance actuals to formals
				// along the chain.
				Cell *spec = topcell;
				std::vector<std::tuple<Cell *, std::vector<Wire *>, std::vector<SigSpec>>> chain;
				while (sva_op(spec) != "spec") {
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
				if (!spec || sva_op(spec) != "spec") {
					unsupported++;
					log("Skipping %s/%s: property is not a $sva spec.\n", log_id(mod), log_id(cell));
					{ again = false; break; }
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
				SigSpec body_sig;
				bool bad = false;
				for (int guard = 0; guard < 64; guard++) {
					clock = nullptr, disable = nullptr, body = nullptr;
					body_sig = SigSpec();
					bad = false;
					for (auto port : comp.ports_of(spec)) {
						SigSpec sp = spec->getPort(port);
						if (sp.empty() || sp.is_fully_const())
							{ again = false; break; }
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
					if (sva_op(spec) != "spec")
						break;
				}

				if (bad || (!body && body_sig.empty())) {
					unsupported++;
					log("Skipping %s/%s: malformed spec.\n", log_id(mod), log_id(cell));
					{ again = false; break; }
				}

				std::string clop = clock ? sva_op(clock) : "no_clock";
				SigSpec clksig = clock ? clock->getPort(ID(A)) : SigSpec();
				if (clop == "no_clock" || clksig.empty() || !clksig[0].is_wire()) {
					unsupported++;
					log("Skipping %s/%s: no usable sampling clock.\n", log_id(mod), log_id(cell));
					{ again = false; break; }
				}
				comp.clk = clksig[0];
				comp.clk_pos = clop != "negedge";
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

				SigSpec fail;
				SigSpec new_a;
				bool strong_until = false;
				SigSpec su_p, su_q;
				if (liveness) {
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
				} else {
					// v1.5 strong until: p s_until q == (p until q) && F q
					// with p, q plain booleans; the weak monitor keeps this
					// $check, the F q obligation is emitted below as a
					// "<name>$live" liveness twin (same scheme as the
					// eventually v1 lowering).  Temporal operands fall
					// through to prop_dispatch's unsupported-op error.
					bool su_with = false;
					if (body && (sva_op(body) == "s_until" || sva_op(body) == "s_until_with")) {
						auto bp = comp.ports_of(body);
						if (bp.size() == 2 && !comp.child_cell(body->getPort(bp[0])) &&
								!comp.child_cell(body->getPort(bp[1]))) {
							strong_until = true;
							su_with = sva_op(body) == "s_until_with";
							su_p = comp.reduce1(comp.resolve(body->getPort(bp[0])));
							su_q = comp.reduce1(comp.resolve(body->getPort(bp[1])));
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
						fail = comp.prop_dispatch(SigSpec(), body, Const(1, 1));
						if (!had_slots && comp.slots_mode) {
							// slot state is per-property: do not leak it into
							// the next $check of this module
							comp.slots_mode = false;
							comp.nslots = 0;
							comp.age = SigSpec();
							comp.slotfiles.clear();
							comp.pending_wr.clear();
						}
					} else {
						fail = mod->Not(NEW_ID, comp.reduce1(body_sig));
					}
					new_a = mod->Not(NEW_ID, fail);
				}

				std::string dop = disable ? sva_op(disable) : "no_disable";
				SigSpec dsig;
				if (dop == "disable_iff") {
					dsig = comp.reduce1(disable->getPort(ID(A)));
					new_a = mod->Or(NEW_ID, dsig, new_a);
				}

				cell->setPort(ID(EN), SigSpec(State::S1));
				cell->setPort(ID(A), new_a);
				if (liveness)
					cell->setParam(ID(FLAVOR), std::string("liveness"));
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
					SigSpec la = disable ? mod->Or(NEW_ID, dsig, su_q) : su_q;
					live->setPort(ID(A), la);
					live->setPort(ID(EN), SigSpec(State::S1));
					live->attributes[ID(sva2check)] = Const(1, 1);
				}

				if (expose_bad) {
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
