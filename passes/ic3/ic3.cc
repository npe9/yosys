/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2012  Claire Xenia Wolf <claire@yosyshq.com>
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

// [[CITE]] SAT-Based Model Checking without Unrolling
// Aaron R. Bradley (IC3, CAV 2011)

#include "kernel/yosys.h"
#include "kernel/satgen.h"

#include "libs/minisat/Solver.h"

#include <chrono>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

// -timeout <s>: a per-query wall-clock budget shared by the main solver and
// its pre-pass solvers; every SAT call checks it and throws, the query is
// then reported undecided (one hard cover used to hold a 145-cover run for
// hours with nothing reported)
struct Ic3Timeout {};
static bool ic3_deadline_on = false;
static std::chrono::steady_clock::time_point ic3_deadline;
static void ic3_check_deadline()
{
	if (ic3_deadline_on && std::chrono::steady_clock::now() > ic3_deadline)
		throw Ic3Timeout();
}

// ezSAT backend on the plain (non-simplifying) in-tree MiniSat solver.
// The default ezMiniSAT uses SimpSolver whose variable elimination re-runs
// on every incremental solve; IC3 interleaves clause addition and solving
// hundreds of times, so elimination dominates the runtime there.
struct Ic3MiniSat : public ezSAT
{
	Minisat::Solver *minisat_solver;
	std::vector<int> minisat_vars;
	size_t clauses_since_simplify;

	Ic3MiniSat() : minisat_solver(nullptr), clauses_since_simplify(0) { }
	~Ic3MiniSat() override { delete minisat_solver; }

	bool solver(const std::vector<int> &modelExpressions, std::vector<bool> &modelValues,
			const std::vector<int> &assumptions) override
	{
		std::vector<int> extraClauses, modelIdx;

		for (auto id : assumptions)
			extraClauses.push_back(bind(id));
		for (auto id : modelExpressions)
			modelIdx.push_back(bind(id));

		if (minisat_solver == nullptr)
			minisat_solver = new Minisat::Solver;

		std::vector<std::vector<int>> cnf;
		consumeCnf(cnf);

		if (getenv("IC3_CNF_DUMP")) {
			fprintf(stderr, "ic3-cnf: dump: vars=%d newclauses=%zu assum=%zu model=%zu\n",
					numCnfVariables(), cnf.size(), assumptions.size(), modelExpressions.size());
			for (auto &cl : cnf) {
				fprintf(stderr, "ic3-cnf:  +");
				for (auto l : cl) fprintf(stderr, " %d", l);
				fprintf(stderr, "\n");
			}
		}
		if (getenv("IC3_MS_DUMP")) {
			static int ms_dump_ctr = 0;
			ms_dump_ctr++;
			if (ms_dump_ctr == atoi(getenv("IC3_MS_DUMP"))) {
				fprintf(stderr, "ic3-ms: vars=%d solver-clauses=%d\n", numCnfVariables(), minisat_solver->nClauses());
				int ci = 0;
				for (auto it = minisat_solver->clausesBegin(); it != minisat_solver->clausesEnd(); ++it, ci++) {
					const Minisat::Clause &c = *it;
					fprintf(stderr, "ic3-ms: c%d:", ci);
					for (int li = 0; li < c.size(); li++) {
						int v = Minisat::var(c[li]);
						bool s = Minisat::sign(c[li]);
						int cnfidx = -1;
						for (int k = 0; k < (int)minisat_vars.size(); k++)
							if (minisat_vars[k] == v) { cnfidx = k+1; break; }
						fprintf(stderr, " %s%d", s ? "-" : "+", cnfidx < 0 ? v : cnfidx);
					}
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			}
		}

		while ((int)minisat_vars.size() < numCnfVariables())
			minisat_vars.push_back(minisat_solver->newVar());

		for (auto &clause : cnf) {
			Minisat::vec<Minisat::Lit> ps;
			for (auto idx : clause)
				if (idx > 0)
					ps.push(Minisat::mkLit(minisat_vars.at(idx-1)));
				else
					ps.push(Minisat::mkLit(minisat_vars.at(-idx-1), true));
			if (!minisat_solver->addClause(ps))
				return false;
		}

		// IC3 interleaves clause addition and solving hundreds of thousands
		// of times; MiniSat's simplify() walks the whole clause DB, so it
		// must not run per batch. The solver self-cleans via reduceDB in
		// solve(); simplify() only kicks in on large batches.
		clauses_since_simplify += cnf.size();
		if (clauses_since_simplify >= 10000) {
			clauses_since_simplify = 0;
			if (!minisat_solver->simplify())
				return false;
		}

		Minisat::vec<Minisat::Lit> assumps;
		for (auto idx : extraClauses)
			if (idx > 0)
				assumps.push(Minisat::mkLit(minisat_vars.at(idx-1)));
			else
				assumps.push(Minisat::mkLit(minisat_vars.at(-idx-1), true));

		// Under -timeout a single solve must not overrun the budget (the
		// deadline used to be checked only between SAT calls: one hard
		// solve on a ~5M-clause unrolling ran past it).  Solve in
		// propagation-budget slices and check the deadline between them;
		// learnt clauses survive across slices, and an interrupted solve
		// leaves the solver at level 0, reusable for the next query.
		if (!ic3_deadline_on) {
			if (!minisat_solver->solve(assumps))
				return false;
		} else {
			while (1) {
				ic3_check_deadline();
				minisat_solver->setPropBudget(1 << 21);
				Minisat::lbool r = minisat_solver->solveLimited(assumps);
				if (r == Minisat::lbool((uint8_t)0))
					break;
				if (r == Minisat::lbool((uint8_t)1)) {
					minisat_solver->budgetOff();
					return false;
				}
			}
			minisat_solver->budgetOff();
		}

		modelValues.clear();
		modelValues.resize(modelIdx.size());

		for (size_t i = 0; i < modelIdx.size(); i++)
		{
			int idx = modelIdx[i];
			bool refvalue = true;

			if (idx < 0)
				idx = -idx, refvalue = false;

			Minisat::lbool value = minisat_solver->modelValue(minisat_vars.at(idx-1));
			modelValues[i] = (value == Minisat::lbool(refvalue));
		}

		return true;
	}
};

struct Ic3Solver
{
	RTLIL::Module *module;
	SigMap sigmap;
	Ic3MiniSat ez;
	SatGen satgen;

	bool init_zero;
	int max_frames;

	std::vector<RTLIL::Cell*> assume_cells;

	// further checks whose cones encode() must include (the batched BMC
	// pre-pass encodes every assert and cover on one unrolling)
	std::vector<RTLIL::Cell*> extra_seeds;

	// initial value of each state1 bit (set by encode()): 0/1 when the
	// initial state fixes it, -1 when it is free; init_fixed says every
	// bit is fixed (always so under -init-zero)
	std::vector<int> init_val;
	bool init_fixed = false;

	// fairness cells ($fair: assume(s_eventually ...), lowered by sva2check
	// from assume/restrict of unbounded eventually): they seed the cone of
	// influence in encode() but are NOT permanent constraints (that would
	// force fair states at every step); prove_liveness imports their A/EN
	// as fair predicates and prunes the gfp survivor set to states that
	// can visit each of them infinitely.
	std::vector<RTLIL::Cell*> fair_cells;

	// state vector: one entry per register bit (t=1 and t=2 literals)
	std::vector<int> state1, state2;
	int bad1, bad2;
	// register Q bit behind each state1 entry (set by encode()), for
	// translating gfp cubes back into netlist logic in prove_liveness
	RTLIL::SigSpec istate_bits;

	// frame activation literals and per-frame clause (cube) sets, 1-based;
	// index 0 is an unused placeholder so frame numbers match the literature.
	// init_act gates the initial-state constraints: they only belong to
	// frame 0, frames >= 1 range over arbitrary states
	std::vector<int> act;
	int init_act;
	std::vector<std::vector<std::vector<int>>> frames;

	// per-timestep state vectors (index 0 unused) and bad predicates,
	// filled by encode(prop, ntf); state1/state2 alias states[1]/states[2]
	std::vector<std::vector<int>> states;
	std::vector<int> badv;

	// k-induction depth tried before the IC3 loop (0 = disabled)
	int kind_max;

	// bounded reachability depth tried before everything else (0 = off);
	// finds deep counterexamples (t7-class CEXes near the ctr-wrap
	// boundary) in d cheap solves instead of d IC3 frames
	int bmc_max;

	// CTG-guided generalization recursion depth (0 = disabled)
	int ctg_depth_max;

	// state bits whose initial value is an explicit 1 (encode())
	int n_init_ones = 0;

	int nsolves;
	long long n_bad, n_blockq, n_gen, n_init, n_ctg;
	double t_addclause, t_generalize, t_encode;

	Ic3Solver(RTLIL::Module *module, bool init_zero, int max_frames) :
			module(module), sigmap(module), ez(), satgen(&ez, &sigmap),
			init_zero(init_zero), max_frames(max_frames), kind_max(0),
			bmc_max(0),
			ctg_depth_max(0),
			nsolves(0),
			n_bad(0), n_blockq(0), n_gen(0), n_init(0), n_ctg(0),
			t_addclause(0), t_generalize(0), t_encode(0)
	{
		act.push_back(0);
		frames.emplace_back();
		init_act = ez.frozen_literal("init");
		live2_act = ez.frozen_literal("live2");
		bad1 = bad2 = 0;
	}

	static bool is_formal_cell(IdString type)
	{
		return type.in(ID($assert), ID($assume), ID($cover), ID($check),
				ID($live), ID($fair), ID($print));
	}

	// encode the design unrolled to ntf timeframes: comb logic at each
	// timestep, SatGen ties Q@t to D@(t-1) on FF import. The IC3 loop uses
	// ntf=2; the k-induction pre-pass uses ntf=k+1.
	void encode(RTLIL::Cell *prop, int ntf = 2)
	{
		if (!module->processes.empty())
			log_error("ic3: module %s still contains processes -- run `proc` first.\n", log_id(module));

		// Cone of influence of the property ports and the assume
		// constraints: the cells in the combinatorial fanin. Cubes are
		// built only over the COI register bits, so blocking and
		// generalization do not spend a SAT solve per irrelevant state bit
		// (t8: 130 register bits, of which a given property reads ~40).
		// FF cells are traversed like any other cell: their D cones feed
		// the transition encoding, and their Q bits form the state vector.
		//
		// Unknown cell types (leftover \$sva trees of unsupported checks
		// such as cover) report every port as an output via Cell::output,
		// so for them only \Y is trusted as a driver; a \$sva cell that is
		// a genuine cone member still fails the importCell check below.
		dict<RTLIL::SigBit, std::vector<RTLIL::Cell*>> drivers;
		for (auto cell : module->cells()) {
			// blackbox stubs auto-created by hierarchy for unknown types
			// (the \$sva trees of checks sva2check does not compile, e.g.
			// cover) declare every connection as an output; treat such
			// cells as untyped and trust only \Y, otherwise the COI walk
			// pulls unrelated input ports (\a, \b, ...) into the cone.
			RTLIL::Module *cm = module->design->module(cell->type);
			bool typed = yosys_celltypes.cell_known(cell->type) ||
					(cm && !cm->get_blackbox_attribute());
			for (auto &conn : cell->connections())
				if (typed ? cell->output(conn.first) : conn.first == ID::Y)
					for (auto bit : sigmap(conn.second))
						if (bit.wire)
							drivers[bit].push_back(cell);
		}

		pool<RTLIL::Cell*> coi_cells;
		pool<RTLIL::SigBit> visited;
		std::vector<RTLIL::SigBit> worklist;
		auto seed = [&](RTLIL::SigSpec sig) {
			for (auto bit : sigmap(sig))
				if (bit.wire)
					worklist.push_back(bit);
		};
		seed(prop->getPort(ID::A));
		seed(prop->getPort(ID::EN));
		for (auto cell : extra_seeds) {
			seed(cell->getPort(ID::A));
			seed(cell->getPort(ID::EN));
		}
		for (auto cell : assume_cells) {
			seed(cell->getPort(ID::A));
			seed(cell->getPort(ID::EN));
		}
		for (auto cell : fair_cells) {
			seed(cell->getPort(ID::A));
			seed(cell->getPort(ID::EN));
		}
		while (!worklist.empty()) {
			RTLIL::SigBit bit = worklist.back();
			worklist.pop_back();
			if (!visited.insert(bit).second)
				continue;
			for (auto cell : drivers[bit]) {
				if (is_formal_cell(cell->type))
					continue;
				if (!coi_cells.insert(cell).second)
					continue;
				for (auto &conn : cell->connections())
					if (cell->input(conn.first))
						for (auto bit : sigmap(conn.second))
							if (bit.wire)
								worklist.push_back(bit);
			}
		}

		// two-timeframe unraveling: comb logic at each timestep, FF
		// transition constraints tie Q@t to D@(t-1); Q@1 stays a free
		// variable modulo the init constraints below
		for (int t = 1; t <= ntf; t++)
			for (auto cell : coi_cells) {
				if (cell->type == ID($mem) || cell->type == ID($pow))
					log_error("ic3: %s cell %s.%s is not supported -- run `memory_dff; memory_collect; opt_clean` first.\n",
							log_id(cell->type), log_id(module), log_id(cell));
				if (!satgen.importCell(cell, t))
					log_error("ic3: unsupported cell %s.%s (type %s) -- run `proc; async2sync; flatten; opt_clean` first.\n",
							log_id(module), log_id(cell), log_id(cell->type));
			}

		// assume-flavor checks are permanent constraints at all timeframes
		for (auto cell : assume_cells)
			for (int t = 1; t <= ntf; t++) {
				std::vector<int> a = satgen.importSigSpec(cell->getPort(ID::A), t);
				std::vector<int> en = satgen.importSigSpec(cell->getPort(ID::EN), t);
				while (a.size() < en.size()) a.push_back(ez.CONST_TRUE);
				while (en.size() < a.size()) en.push_back(ez.CONST_FALSE);
				ez.assume(ez.vec_reduce_and(ez.vec_or(a, ez.vec_not(en))));
			}

		// initial state: \init attributes where present (Sx bits stay free),
		// everything else free.  -init-zero fixes the remaining bits (and
		// the Sx bits) to 0 but keeps every explicit 0/1 init: it used to
		// force ALL registers to 0, silently overriding `reg init = 1'b1`
		// -- a reset generator that never fired, so a harness DUT never saw
		// reset and covers were reached during what should be reset (peer
		// Mem_Controller/Deburster harnesses, 2026-10-22; smtbmc and the
		// sim replay, prepped with `setundef -init -zero`, honour the init)
		RTLIL::SigSpec big_lhs, big_rhs;
		for (auto wire : module->wires()) {
			if (wire->attributes.count(ID::init) == 0)
				continue;
			RTLIL::SigSpec lhs = sigmap(wire);
			RTLIL::Const rhs = wire->attributes.at(ID::init);
			log_assert(lhs.size() == rhs.size());
			RTLIL::SigSpec l, r;
			for (int i = 0; i < lhs.size(); i++) {
				RTLIL::SigSpec bit = lhs.extract(i, 1);
				if (!satgen.initial_state.check_all(bit))
					continue;
				if (rhs[i] != RTLIL::State::S0 && rhs[i] != RTLIL::State::S1) {
					if (!init_zero)
						continue;
					l.append(bit);
					r.append(RTLIL::State::S0);
				} else {
					l.append(bit);
					r.append(rhs[i]);
				}
			}
			if (l.size()) {
				big_lhs.remove2(l, &big_rhs);
				big_lhs.append(l);
				big_rhs.append(r);
			}
		}
		n_init_ones = 0;
		for (auto b : big_rhs)
			if (b == RTLIL::State::S1)
				n_init_ones++;
		if (init_zero) {
			pool<RTLIL::SigBit> fixed;
			for (auto bit : big_lhs)
				fixed.insert(bit);
			for (auto bit : satgen.initial_state.export_all())
				if (!fixed.count(bit)) {
					big_lhs.append(bit);
					big_rhs.append(RTLIL::State::S0);
				}
		}
		if (big_lhs.size())
			ez.assume(ez.OR(satgen.signals_eq(big_lhs, big_rhs, 1), init_act));

		// ordered state vector (register Q bits only) per timestep
		RTLIL::SigSpec istates = satgen.initial_state.export_all();
		istate_bits = istates;
		states.assign(ntf + 1, {});
		for (auto bit : istates)
			for (int t = 1; t <= ntf; t++)
				states[t].push_back(satgen.importSigBit(bit, t));
		state1 = states[1];
		state2 = ntf >= 2 ? states[2] : states[1];
		{
			dict<RTLIL::SigBit, int> iv;
			for (int i = 0; i < big_lhs.size(); i++)
				iv[big_lhs[i]] = big_rhs[i] == RTLIL::State::S1 ? 1 : 0;
			init_val.clear();
			init_fixed = true;
			for (auto bit : istates) {
				auto it = iv.find(bit);
				init_val.push_back(it == iv.end() ? -1 : it->second);
				if (it == iv.end())
					init_fixed = false;
			}
		}

		// bad := en && !a  (per bit, padded vacuously with zeros)
		auto build_bad = [&](int t) {
			std::vector<int> a = satgen.importSigSpec(prop->getPort(ID::A), t);
			std::vector<int> en = satgen.importSigSpec(prop->getPort(ID::EN), t);
			while (a.size() < en.size()) a.push_back(ez.CONST_FALSE);
			while (en.size() < a.size()) en.push_back(ez.CONST_FALSE);
			return ez.vec_reduce_and(ez.vec_and(en, ez.vec_not(a)));
		};
		badv.assign(ntf + 1, 0);
		for (int t = 1; t <= ntf; t++)
			badv[t] = build_bad(t);
		bad1 = badv[1];
		bad2 = badv[2];
	}

	int new_frame()
	{
		int i = act.size();
		act.push_back(ez.frozen_literal(stringf("act%d", i)));
		frames.emplace_back();
		return i;
	}

	// assumptions are expression ids or positive literal ids; a negated
	// literal must be turned into a NOT expression, not a negative id
	int assum_lit(int signed_lit)
	{
		return signed_lit > 0 ? signed_lit : ez.NOT(-signed_lit);
	}

	void push_assum(std::vector<int> &assumptions, int signed_lit)
	{
		assumptions.push_back(assum_lit(signed_lit));
	}

	// assumption set for "query frame upto": frame i ranges over its own
	// clause set only (F_i ⊇ states reachable within i steps), so exactly
	// act[upto] is activated; frame 0 is the initial-state constraints
	std::vector<int> act_assumptions(int upto)
	{
		std::vector<int> assumptions;
		for (int i = 1; i < (int)act.size(); i++)
			push_assum(assumptions, i == upto ? -act[i] : act[i]);
		push_assum(assumptions, upto == 0 ? -init_act : init_act);
		return assumptions;
	}

	// c subsumes d when every literal of c occurs in d (ignoring dropped
	// 0 positions): the clause for d is implied by the clause for c.
	static bool cube_subsumes(const std::vector<int> &c, const std::vector<int> &d)
	{
		for (int x = 0; x < (int)c.size(); x++)
			if (c[x] && c[x] != d[x])
				return false;
		return true;
	}

	// cube entries are signed state literals; 0 marks a dropped (don't
	// care) position in generalized cubes
	void push_cube(std::vector<int> &assumptions, const std::vector<int> &cube, const std::vector<int> &svec)
	{
		for (int i = 0; i < (int)cube.size(); i++)
			if (cube[i])
				push_assum(assumptions, cube[i] > 0 ? svec[i] : -svec[i]);
	}

	// add clause (negated cube) to all frames in [lo, hi]; the clause is
	// (id ∨ act[i]), i.e. enforced exactly when act[i] is false (see
	// act_assumptions). A new cube subsumes (removes) any weaker cube in
	// the same frame and is skipped when a stronger or equal cube is
	// already present -- without this, each frame accumulates a redundant
	// copy of every cube and SAT propagation dominates runtime.
	void add_clause(const std::vector<int> &cube, int lo, int hi)
	{
		auto t0 = std::chrono::steady_clock::now();
		std::vector<int> clause;
		clause.reserve(cube.size());
		for (int lit : cube)
			if (lit)
				clause.push_back(lit > 0 ? ez.NOT(lit) : -lit);
		int id = clause.size() == 1 ? clause[0] : ez.expression(ezSAT::OpOr, clause);
		for (int i = lo; i <= hi; i++) {
			auto &fc = frames[i];
			bool redundant = false;
			for (auto &c : fc)
				if (cube_subsumes(c, cube)) {
					redundant = true;
					break;
				}
			if (redundant)
				continue;
			for (int j = 0; j < (int)fc.size(); j++)
				if (cube_subsumes(cube, fc[j])) {
					fc.erase(fc.begin() + j);
					j--;
				}
			ez.assume(ez.OR(id, act[i]));
			fc.push_back(cube);
		}
		t_addclause += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	}

	bool solve_assum(const std::vector<int> &assumptions)
	{
		std::vector<int> exprs;
		std::vector<bool> vals;
		ic3_check_deadline();
		nsolves++;
		if (nsolves % 100000 == 0)
			fprintf(stderr, "ic3-debug: %d solves, %d frames, clauses=%d (blk=%lld gen=%lld init=%lld bad=%lld ctg=%lld) t_add=%.1fs t_gen=%.1fs\n",
					nsolves, (int)act.size(), ez.numCnfClauses(), n_blockq, n_gen, n_init, n_bad, n_ctg, t_addclause, t_generalize);
		return ez.solve(exprs, vals, assumptions);
	}

	bool solve_model(const std::vector<int> &assumptions, const std::vector<int> &svec, std::vector<bool> &model)
	{
		std::vector<int> exprs = svec;
		model.clear();
		ic3_check_deadline();
		nsolves++;
		if (nsolves % 100000 == 0)
			fprintf(stderr, "ic3-debug: %d solves, %d frames, clauses=%d (blk=%lld gen=%lld init=%lld bad=%lld ctg=%lld) t_add=%.1fs t_gen=%.1fs\n",
					nsolves, (int)act.size(), ez.numCnfClauses(), n_blockq, n_gen, n_init, n_bad, n_ctg, t_addclause, t_generalize);
		return ez.solve(exprs, model, assumptions);
	}

	// SAT(R_upto(S1) && T && bad@ts)? model = state literals at ts
	bool bad_sat(int upto, int ts, std::vector<bool> &model)
	{
		n_bad++;
		std::vector<int> assumptions = act_assumptions(upto);
		assumptions.push_back(ts == 1 ? bad1 : bad2);
		return solve_model(assumptions, ts == 1 ? state1 : state2, model);
	}

	// drop literals (set to don't-care) while the cube stays disjoint from
	// Init and inductive relative to frame lvl. MIC-style: repeat the drop
	// scan until a full pass drops nothing — a literal that was blocked
	// early may become droppable once other literals are gone, and smaller
	// cubes mean fewer block() rounds downstream.
	//
	// CTG-guided (abc-pdr style, experimental -- enable with -ctg): a failed
	// drop returns a predecessor state (counterexample to generalization);
	// recursively blocking it at this level can make the retry succeed and
	// shrinks cubes on control-heavy designs. Off by default: the recursion
	// multiplies block()/init obligations on easy properties (measured:
	// 60x more blk/init queries on the async FIFO), so it is only worth it
	// on designs where plain MIC stalls.
	std::vector<int> generalize(std::vector<int> cube, int lvl, int ctg_depth = 0)
	{
		auto t0 = std::chrono::steady_clock::now();
		std::vector<int> ind_assumptions = act_assumptions(lvl);
		// when the initial state is one fixed vector (always under
		// -init-zero), Init-disjointness of a cube is just "some literal
		// contradicts its bit's init value" -- no solve needed
		bool init_is_const = init_fixed;
		std::vector<int> init_assumptions;
		push_assum(init_assumptions, -init_act);
		for (int pass = 0; pass < 4; pass++) {
			bool dropped_any = false;
			for (int x = 0; x < (int)cube.size(); x++) {
				if (cube[x] == 0)
					continue;
				int saved = cube[x];
				cube[x] = 0;
				bool ok = false;
				if (ctg_depth_max == 0) {
					n_gen++;
					std::vector<int> a = ind_assumptions;
					push_cube(a, cube, state2);
					ok = !solve_assum(a);
				} else
				for (int attempt = 0; attempt < 4; attempt++) {
					n_gen++;
					std::vector<int> a = ind_assumptions;
					push_cube(a, cube, state2);
					std::vector<bool> cti_model;
					if (!solve_model(a, state1, cti_model)) {
						ok = true;
						break;
					}
					if (ctg_depth >= ctg_depth_max)
						break;
					std::vector<int> cti;
					cti.reserve(state1.size());
					for (int i = 0; i < (int)state1.size(); i++)
						cti.push_back(cti_model[i] ? state1[i] : -state1[i]);
					n_ctg++;
					if (!block(cti, lvl, ctg_depth + 1))
						break;
				}
				if (ok && !init_is_const) {
					std::vector<int> b = init_assumptions;
					push_cube(b, cube, state1);
					ok = !solve_assum(b);
				}
				if (ok && init_is_const) {
					bool clash = false;
					for (int i = 0; i < (int)cube.size(); i++)
						if (cube[i] && (cube[i] > 0 ? 1 : 0) != init_val[i])
							clash = true;
					ok = clash;
				}
				if (!ok)
					cube[x] = saved;
				else
					dropped_any = true;
			}
			if (!dropped_any)
				break;
		}
		t_generalize += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		return cube;
	}

	bool block(std::vector<int> cube, int k, int ctg_depth = 0)
	{
		// counterexample if an initial state is inside the cube
		std::vector<int> init_assumptions;
		push_cube(init_assumptions, cube, state1);
		push_assum(init_assumptions, -init_act);
		n_init++;
		if (solve_assum(init_assumptions))
			return false;
		for (int j = k; j >= 1; j--) {
			while (1) {
				n_blockq++;
				std::vector<int> assumptions = act_assumptions(j-1);
				push_cube(assumptions, cube, state2);
				std::vector<bool> model;
				if (!solve_model(assumptions, state1, model)) {
					add_clause(generalize(cube, j-1, ctg_depth), j, k);
					return true;
				}
				std::vector<int> cti;
				cti.reserve(state1.size());
				for (int i = 0; i < (int)state1.size(); i++)
					cti.push_back(model[i] ? state1[i] : -state1[i]);
				if (!block(cti, j-1))
					return false;
			}
		}
		log_abort();
	}

	// frame i inductive relative to itself? (F_i(S1) ∧ T ⇒ F_i, i.e. no
	// F_i state transitions into an F_i cube); together with the loop
	// invariant Init ∧ ¬bad and Img(F_j) ∧ ¬bad for all j <= n-1 this
	// proves reach ∩ bad = ∅ by induction over the transition relation
	bool inductive_at(int i)
	{
		std::vector<int> assumptions = act_assumptions(i);
		std::vector<int> disj;
		for (auto &cube : frames[i]) {
			std::vector<int> conj;
			for (int x = 0; x < (int)cube.size(); x++)
				if (cube[x])
					conj.push_back(cube[x] > 0 ? state2[x] : ez.NOT(state2[x]));
			disj.push_back(conj.size() == 1 ? conj[0] : ez.expression(ezSAT::OpAnd, conj));
		}
		int into_cube = disj.empty() ? ez.CONST_FALSE : ez.expression(ezSAT::OpOr, disj);
		assumptions.push_back(into_cube);
		return !solve_assum(assumptions);
	}

	// k-induction on the ntf=kmax+1 timeframe encoding from encode():
	// base case -- bad reachable from init within kmax steps (disproof);
	// step case -- k consecutive non-bad states cannot reach a bad state
	// (proof). Returns 0 proved, 1 disproved (depth = cex step), 2
	// inconclusive (fall through to the IC3 loop).
	int run_kind(int kmax, int &depth)
	{
		for (int d = 1; d <= kmax; d++) {
			std::vector<int> assumptions;
			push_assum(assumptions, -init_act);
			assumptions.push_back(badv.at(d));
			std::vector<bool> model;
			bool sat = solve_model(assumptions, states.at(d), model);
			if (getenv("IC3_BMC_DEBUG"))
				fprintf(stderr, "kind-debug: d=%d sat=%d badlit=%d initlit=%d nstates=%d clauses=%d vars=%d\n",
						d, (int)sat, badv.at(d), init_act, (int)states.at(d).size(),
						ez.numCnfClauses(), ez.numCnfVariables());
			if (sat) {
				depth = d - 1;
				return 1;
			}
		}
		for (int k = 1; k <= kmax; k++) {
			std::vector<int> assumptions;
			for (int t = 1; t <= k; t++)
				assumptions.push_back(ez.NOT(badv.at(t)));
			assumptions.push_back(badv.at(k + 1));
			if (!solve_assum(assumptions)) {
				depth = k;
				return 0;
			}
		}
		return 2;
	}

	// k-induction pre-pass on a throwaway encoding; 2 = "not decided, fall
	// through to the IC3 loop on the caller's solver"
	int kind_prepass(RTLIL::Cell *prop, int &depth)
	{
		if (kind_max <= 0)
			return 2;
		Ic3Solver ks(module, init_zero, max_frames);
		ks.assume_cells = assume_cells;
		ks.encode(prop, kind_max + 1);
		return ks.run_kind(kind_max, depth);
	}

	// safety IC3 core on THIS solver; requires that encode() has not run
	// yet on this solver.  0 = proved, 1 = disproved (depth set),
	// 2 = undecided (depth set)
	int prove_core(RTLIL::Cell *prop, int &depth)
	{
		auto t0 = std::chrono::steady_clock::now();
		encode(prop);
		t_encode += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		if (getenv("IC3_DEBUG"))
			fprintf(stderr, "ic3-debug: proving %s clauses=%d vars=%d states=%d t_encode=%.3fs\n", log_id(prop), ez.numCnfClauses(), ez.numCnfVariables(), GetSize(state1), t_encode);

		std::vector<bool> model;
		if (bad_sat(0, 1, model)) {
			depth = 0;
			return 1;
		}

		int n = new_frame();
		while (1) {
			while (bad_sat(n-1, 2, model)) {
				std::vector<int> cube;
				cube.reserve(state1.size());
				for (int i = 0; i < (int)state1.size(); i++)
					cube.push_back(model[i] ? state1[i] : -state1[i]);
				if (!block(cube, n)) {
					depth = n;
					return 1;
				}
				if (n > max_frames) {
					depth = n;
					return 2;
				}
			}

			// proof: some frame is inductive relative to itself
			for (int i = 1; i <= n-1; i++)
				if (inductive_at(i)) {
					depth = i;
					return 0;
				}

			if (n >= max_frames) {
				depth = n;
				return 2;
			}
			n = new_frame();
		}
	}

	// Bounded reachability pre-pass on a throwaway encoding: unroll the
	// design bmc_max+1 timeframes and ask bad@t for t = 1..bmc_max (the
	// transition CNF is shared, so each step is one cheap solve). A hit
	// is an exact counterexample at depth t (the IC3 loop would need t
	// frames to find it -- t7-class deep CEXes at the ctr-wrap boundary);
	// a miss is inconclusive and falls through.  2 = not run / no hit.
	int bmc_prepass(RTLIL::Cell *prop, int &depth)
	{
		if (bmc_max <= 0)
			return 2;
		Ic3Solver bs(module, init_zero, max_frames);
		bs.assume_cells = assume_cells;
		bs.encode(prop, bmc_max + 1);
		for (int t = 1; t <= bmc_max; t++) {
			std::vector<int> assumptions;
			// every id below lives in bs.ez: push_assum/solve_model must
			// be qualified with bs, otherwise this->ez (still empty here)
			// allocates the NOT(init) expression and the assumption is
			// garbage in bs's id namespace -- spurious SAT
			bs.push_assum(assumptions, -bs.init_act);
			assumptions.push_back(bs.badv[t]);
			std::vector<bool> model;
			bool sat = bs.solve_model(assumptions, bs.states.at(t), model);
			if (getenv("IC3_BMC_DEBUG"))
				fprintf(stderr, "bmc-debug: %s t=%d sat=%d badlit=%d initlit=%d nstates=%d clauses=%d vars=%d\n",
						log_id(prop), t, (int)sat, bs.badv[t], bs.init_act, (int)bs.states.at(t).size(),
						bs.ez.numCnfClauses(), bs.ez.numCnfVariables());
			if (sat) {
				if (getenv("IC3_BMC_DEBUG") && t <= 2) {
					std::string m;
					for (bool b : model) m += b ? '1' : '0';
					fprintf(stderr, "bmc-model: %s t=%d state1=%s\n", log_id(prop), t, m.c_str());
				}
				depth = t - 1;
				return 1;
			}
		}
		return 2;
	}

	// 0 = proved, 1 = disproved (depth set), 2 = undecided (depth set)
	int prove(RTLIL::Cell *prop, int &depth)
	{
		// A disproof decided by a throwaway pre-pass solver leaves this
		// solver unencoded, but that is fine with -cex: export_cex
		// re-derives the witness by an exact unroll on its own solver, so
		// no re-run of the IC3 loop is needed just to populate a state
		// vector (that fallback used to make -cex on a t7-class deep
		// pre-pass cex pay the full frame-by-frame walk).
		int r = bmc_prepass(prop, depth);
		if (r != 2)
			return r;
		r = kind_prepass(prop, depth);
		if (r != 2)
			return r;
		return prove_core(prop, depth);
	}

	// Witness I/O bookkeeping shared by the safety and liveness exporters:
	// which toplevel input ports carry data (recorded per step) and which
	// ones are flip-flop clocks.  sim -r regenerates clock waveforms from
	// the yw "clocks" header and only consumes per-step bits for the
	// remaining (data) inputs; a clock left in the data section would sit
	// at its arbitrary model value forever and never produce an edge --
	// the replayed trace stays at the initial state (observed on t10).
	struct WitnessIo {
		std::vector<Wire*> inports;
		std::vector<int> inwidth;
		std::vector<std::vector<int>> inlit;
		std::vector<Wire*> clkports;
		std::vector<int> clkoff;
		std::vector<std::string> clkedge;
		size_t total_bits = 0;
	};

	// split toplevel inputs into data ports and clocks; clock candidates
	// are netlist bits feeding the CLK pin of flip-flops, and only when
	// every consumer agrees on polarity (the yw format has one edge per
	// clock bit, so a mixed-polarity bit stays a data input)
	void collect_witness_io(WitnessIo &wio)
	{
		dict<RTLIL::SigBit, int> clkpol;
		for (auto cell : module->cells()) {
			if (!cell->is_builtin_ff() || !cell->hasPort(ID::CLK))
				continue;
			int pol = cell->getParam(ID::CLK_POLARITY).as_bool() ? 1 : -1;
			for (auto bit : sigmap(cell->getPort(ID::CLK))) {
				auto it = clkpol.find(bit);
				if (it == clkpol.end())
					clkpol[bit] = pol;
				else if (it->second != pol)
					it->second = 0;
			}
		}
		for (auto w : module->wires()) {
			if (!w->port_input)
				continue;
			int nclk = 0;
			for (auto bit : sigmap(w))
				if (clkpol.count(bit) && clkpol.at(bit) != 0)
					nclk++;
			if (nclk == GetSize(w)) {
				int off = 0;
				for (auto bit : sigmap(w)) {
					wio.clkports.push_back(w);
					wio.clkoff.push_back(off++);
					wio.clkedge.push_back(clkpol.at(bit) > 0 ? "posedge" : "negedge");
				}
				continue;
			}
			wio.inports.push_back(w);
			wio.inlit.push_back(satgen.importSigSpec(w, 1));
			wio.inwidth.push_back(GetSize(wio.inlit.back()));
			wio.total_bits += wio.inwidth.back();
		}
	}

	// pack the input section of a model vector into a yw step bit string;
	// the inputs start at in_base (the state part comes first, see the
	// exporters).  LSB at the highest string index, mirroring smtbmc.
	std::string pack_input_bits(const WitnessIo &wio, const std::vector<bool> &model, size_t in_base)
	{
		std::string bits(wio.total_bits, '0');
		size_t off = 0;
		for (size_t p = 0; p < wio.inports.size(); p++) {
			for (int b = 0; b < wio.inwidth[p]; b++) {
				if (model[in_base + off + b])
					bits[wio.total_bits - 1 - (off + b)] = '1';
			}
			off += wio.inwidth[p];
		}
		return bits;
	}

	// write the witness file: yw header (format/generator, clocks, data
	// signals) followed by the linear steps.  The yw format has no
	// standard lasso marker -- smtbmc --dump-yw writes linear steps only
	// and detects loops out of band -- so a lasso goes into a
	// non-standard "lasso" key holding the step the cycle repeats from;
	// both readers in the tree (kernel/yw.cc for sim -r and
	// backends/smt2/ywio.py) parse known keys only and ignore it.
	void write_yw(const std::string &file, const WitnessIo &wio, const std::vector<std::string> &steps, int lasso)
	{
		FILE *f = fopen(file.c_str(), "w");
		if (!f) {
			log_error("ic3: cannot open %s for writing.\n", file.c_str());
			return;
		}
		fprintf(f, "{\n  \"format\": \"Yosys Witness Trace\",\n  \"generator\": \"yosys-ic3\",\n");
		if (lasso >= 0)
			fprintf(f, "  \"lasso\": %d,\n", lasso);
		if (wio.clkports.empty()) {
			fprintf(f, "  \"clocks\": [],\n");
		} else {
			fprintf(f, "  \"clocks\": [\n");
			for (size_t c = 0; c < wio.clkports.size(); c++) {
				std::string esc;
				for (char ch : wio.clkports[c]->name.str())
					esc += ch == '\\' ? "\\\\" : std::string(1, ch);
				fprintf(f, "    {\"path\": [\"%s\"], \"edge\": \"%s\", \"offset\": %d}%s\n",
						esc.c_str(), wio.clkedge[c].c_str(), wio.clkoff[c],
						c + 1 == wio.clkports.size() ? "" : ",");
			}
			fprintf(f, "  ],\n");
		}
		fprintf(f, "  \"signals\": [\n");
		for (size_t p = 0; p < wio.inports.size(); p++) {
			std::string esc;
			for (char ch : wio.inports[p]->name.str())
				esc += ch == '\\' ? "\\\\" : std::string(1, ch);
			fprintf(f, "    {\"path\": [\"%s\"], \"width\": %d, \"offset\": 0, \"init_only\": false}%s\n",
					esc.c_str(), wio.inwidth[p], p + 1 == wio.inports.size() ? "" : ",");
		}
		fprintf(f, "  ],\n  \"steps\": [\n");
		for (size_t i = 0; i < steps.size(); i++)
			fprintf(f, "    {\"bits\": \"%s\"}%s\n", steps[i].c_str(), i + 1 == steps.size() ? "" : ",");
		fprintf(f, "  ]\n}\n");
		fclose(f);
	}

	// Write a Yosys witness (.yw) for a safety disproof at `depth`
	// (bad reachable from init in `depth` transitions).  The witness is
	// re-derived by an EXACT bounded unroll on a throwaway solver, not
	// walked through the IC3 frames: the frames over-approximate
	// reachability, so a forward walk through them can end in a state
	// whose successors are all good -- the old walk then failed its
	// closing "final state is bad" query and emitted a garbage trace
	// (tnoncons1: alternating inputs where four consecutive ones are
	// needed).  The unroll solves
	//    Init(x_0) && T(x_0,i_0,x_1) && ... && T(x_{d-1},i_{d-1},x_d)
	//    && bad(x_d, i_d)
	// exactly; step k of the witness records i_k (k = 0..depth), where
	// i_0..i_{depth-1} steer init to the bad state and i_depth is the
	// failing valuation sim displays at the bad-state slot (bad's comb
	// cone reads inputs).  Mirrors smtbmc's yw (input signals per step,
	// LSB at the highest string index); the replay design re-derives
	// register states itself, so it must be prepped EXACTLY like the ic3
	// run (same setundef -init -zero when -init-zero was used).
	void export_cex(RTLIL::Cell *prop, const std::string &file, int depth)
	{
		// No state-vector requirement here: the exact unroll below builds
		// its own encoding on the throwaway solver `bs`, and a stateless
		// design (states[1] legitimately empty, e.g. tprove0) still has a
		// well-defined depth-0 witness: the failing input valuation itself.
		WitnessIo wio;
		collect_witness_io(wio);

		// states[1] is the initial state (Q@1) and states[t+1] = D@t, so
		// x_depth is timeframe depth+1; the failing input is the input at
		// that same timeframe.
		Ic3Solver bs(module, init_zero, max_frames);
		bs.assume_cells = assume_cells;
		bs.encode(prop, depth + 1);

		// model vector: per-timeframe data-input bits, t = 1..depth+1,
		// same port order as wio (so pack_input_bits lines up)
		std::vector<int> mvec;
		for (int t = 1; t <= depth + 1; t++)
			for (size_t p = 0; p < wio.inports.size(); p++) {
				std::vector<int> v = bs.satgen.importSigSpec(wio.inports[p], t);
				log_assert(GetSize(v) == wio.inwidth[p]);
				mvec.insert(mvec.end(), v.begin(), v.end());
			}

		std::vector<int> assumptions;
		bs.push_assum(assumptions, -bs.init_act);
		assumptions.push_back(bs.badv.at(depth + 1));
		std::vector<bool> model;
		if (!bs.solve_model(assumptions, mvec, model)) {
			log_warning("ic3: exact unroll found no witness at depth %d -- internal error.\n", depth);
			return;
		}

		std::vector<std::string> steps;
		size_t per_step = wio.total_bits;
		for (int k = 0; k <= depth; k++)
			steps.push_back(pack_input_bits(wio, model, k * per_step));

		write_yw(file, wio, steps, -1);
		log("ic3: counterexample witness written to %s (%d steps)\n", file.c_str(), GetSize(steps));
		log("ic3: replay: same prep, then `sim -r %s -vcd trace.vcd` (assertion fires at the last step)\n", file.c_str());
	}

	// Write a Yosys witness (.yw) for a liveness disproof: s_0 (the Phase
	// C initial state, model passed in) followed by successors inside the
	// gfp survivor set until a state repeats at step k; the steps 0..end
	// are a linear stem-plus-cycle trace (s_end -> s_k closes the loop).
	// Each walk step reuses the Phase B consecution shape exactly:
	// act_assumptions(sf) asserts the prune clauses over state1, the
	// state2 twin clauses hang off -live2_act, and the round literals are
	// pinned like in Phase C so the per-round survivor exclusions are
	// neutralized.  bad1 pairs this step's recorded inputs with the
	// current state -- precisely the (state, input) pair sim displays at
	// the corresponding slot -- and bad2 mirrors the consecution query's
	// successor-side !p.  The fixpoint guarantees every reached state has
	// such a successor, so the walk cannot break; it ends when the state
	// bit string recurs (finite state space) or at a step cap, in which
	// case the witness is a partial stem without a "lasso" marker.
	void export_lasso(const std::string &file, int sf, const std::vector<int> &round_lits, const std::vector<bool> &s0)
	{
		WitnessIo wio;
		collect_witness_io(wio);
		// model vector: state2 (successor state) then all data input bits
		std::vector<int> mvec = state2;
		for (auto &l : wio.inlit)
			mvec.insert(mvec.end(), l.begin(), l.end());

		auto state_bits = [](const std::vector<bool> &m, int n) {
			std::string s(n, '0');
			for (int i = 0; i < n; i++)
				if (m[i])
					s[i] = '1';
			return s;
		};

		std::vector<int> curcube(state1.size());
		for (int i = 0; i < GetSize(state1); i++)
			curcube[i] = s0[i] ? state1[i] : -state1[i];
		std::map<std::string, int> seen;
		seen[state_bits(s0, GetSize(state1))] = 0;

		std::vector<std::string> steps;
		int loop_start = -1;
		int cap = 4 * GetSize(state1) + 64;
		for (int i = 0; i < cap; i++) {
			std::vector<int> assumptions = act_assumptions(sf);
			for (int rl : round_lits)
				push_assum(assumptions, rl);
			push_assum(assumptions, -live2_act);
			push_cube(assumptions, curcube, state1);
			assumptions.push_back(bad1);
			assumptions.push_back(bad2);
			std::vector<bool> model;
			if (!solve_model(assumptions, mvec, model))
				log_error("ic3: lasso walk broke at step %d -- internal error.\n", i);
			steps.push_back(pack_input_bits(wio, model, GetSize(state2)));
			std::string succ = state_bits(model, GetSize(state2));
			for (int j = 0; j < GetSize(state1); j++)
				curcube[j] = model[j] ? state1[j] : -state1[j];
			auto it = seen.find(succ);
			if (it != seen.end()) {
				loop_start = it->second;
				break;
			}
			seen[succ] = i + 1;
		}
		if (loop_start < 0)
			log_warning("ic3: lasso walk hit the step cap (%d) without a recurring state; witness is a partial stem.\n", cap);

		write_yw(file, wio, steps, loop_start);
		if (loop_start >= 0)
			log("ic3: lasso witness written to %s (%d steps, cycle repeats from step %d with period %d)\n",
					file.c_str(), GetSize(steps), loop_start, GetSize(steps) - loop_start);
		else
			log("ic3: partial lasso witness written to %s (%d steps, cycle not closed)\n", file.c_str(), GetSize(steps));
		log("ic3: replay: same prep, then `sim -r %s -vcd trace.vcd -append 1` to watch the cycle close\n", file.c_str());
	}

	// --- liveness (v1) ----------------------------------------------------
	// F p ("assert property (s_eventually p)", lowered by sva2check to a
	// FLAVOR "liveness" $check with A = p) is FALSE iff there is a
	// reachable cycle all of whose states satisfy !p.  Proof:
	//   Phase A: safety IC3 with bad := !p (exactly what encode() builds
	//     for this cell).  If it proves, !p is unreachable and F p holds
	//     trivially; otherwise the gfp below decides on this solver.
	//   Phase B: greatest-fixpoint pruning of S, seeded from top: repeatedly
	//     remove cubes c of (S ∧ !p) whose states cannot stay -- c(s1) ∧
	//     T(s1,s2) ∧ S(s2) ∧ !p(s2) UNSAT means every c-state must exit
	//     S ∧ !p, so no !p-cycle can pass through c.  Survivors (states
	//     with a successor in S ∧ !p) are excluded for the rest of the
	//     round via a per-round activation literal; rounds repeat until a
	//     full scan adds no clause (then S ∧ !p = S*).  Top is a valid
	//     seed because the gfp keeps every fair-cycle state from any
	//     over-approximation (the fixpoint is independent of the seed as
	//     long as it covers νZ); the Phase A frames must NOT seed it, as
	//     they hold bad-BLOCKING clauses (cubes of states that reach bad),
	//     whose negations exclude exactly the lasso states and collapsed
	//     S ∧ !p to UNSAT, false-proving liveness properties whose bad
	//     states are first reachable after init (e.g. F p with p true only
	//     finitely often).
	//   Phase B (fairness): with $fair cells in the design the survivor
	//     set must admit a cycle visiting every fair predicate infinitely:
	//     per predicate, the attractor of (S ∧ fair_i) within S is grown
	//     (seed = fair states; closure = states with a successor in the
	//     set) and S-states outside it are pruned -- the Emerson-Lei fair
	//     gfp.  Without $fair cells this block is skipped (empty fairv).
	//   Phase C: reachability of S* ∧ !p from Init, decided by the safety
	//     core on a fresh solver (member ∧ EN ∧ ¬A as a temporary $check):
	//     reachable gives the disproof, unreachable proves F p.
	// Returns 0 proved, 1 disproved (depth 0), 2 undecided.  With cex_file
	// non-empty, a disproof additionally exports the lasso witness (see
	// export_lasso).
	int prove_liveness(RTLIL::Cell *prop, int &depth, const std::string &cex_file = "")
	{
		// A k-induction proof of bad := !p also proves F p; any other
		// pre-pass outcome falls through to the IC3 loop on this solver,
		// whose encoding and frames Phase B/C reuse (they must live in
		// this solver -- a pre-pass that decides on a throwaway solver
		// leaves this one unencoded).
		int rk = kind_prepass(prop, depth);
		if (rk == 0)
			return 0;
		int r = prove_core(prop, depth);
		if (r == 0)
			return 0;
		bool trace = getenv("IC3_LIVE_TRACE") != nullptr;
		if (trace)
			fprintf(stderr, "ic3-live: %s phaseA r=%d depth=%d frames=%zu state=%zu\n",
					log_id(prop), r, depth, frames.size(), state1.size());

		// fairness predicates ($fair cells): state predicate "A holds at
		// this tick" (inputs existential -- a path chooses its inputs, so
		// "infinitely often" is over input valuations too).  fair_i = A ∧
		// EN, padded like build_bad but with CONST_TRUE (a missing EN bit
		// must not vacuously kill the predicate).
		std::vector<int> fairv;
		for (auto fc : fair_cells) {
			std::vector<int> a = satgen.importSigSpec(fc->getPort(ID::A), 1);
			std::vector<int> en = satgen.importSigSpec(fc->getPort(ID::EN), 1);
			while (a.size() < en.size()) a.push_back(ez.CONST_TRUE);
			while (en.size() < a.size()) en.push_back(ez.CONST_TRUE);
			fairv.push_back(ez.vec_reduce_and(ez.vec_and(a, en)));
		}
		if (!fairv.empty())
			log("ic3: %s: %d fairness constraint(s): disproof requires a fair !p-cycle\n",
					log_id(prop), GetSize(fairv));

		// Phase B: S seeded from top (see the proof sketch above; the Phase
		// A frame clauses would poison the gfp)
		int sf = new_frame();
		if (trace)
			fprintf(stderr, "ic3-live: S seeded from top, phaseA frames=%zu\n", frames.size());

		std::vector<int> round_lits;
		if (!state1.empty()) {
			bool pruned_any = true;
			while (pruned_any) {
				pruned_any = false;
				int rl = ez.frozen_literal(stringf("lvr%d", GetSize(round_lits)));
				round_lits.push_back(rl);
				int n_cand = 0, n_prune = 0;
				while (1) {
					// pick an unexamined state of S ∧ !p
					std::vector<int> assumptions = act_assumptions(sf);
					push_assum(assumptions, -rl);
					for (int i = 0; i+1 < GetSize(round_lits); i++)
						push_assum(assumptions, round_lits[i]);
					assumptions.push_back(bad1);
					std::vector<bool> model;
					n_bad++;
					if (!solve_model(assumptions, state1, model))
						break;
					n_cand++;
					std::vector<int> csub(state1.size());
					for (int i = 0; i < (int)state1.size(); i++)
						csub[i] = model[i] ? state1[i] : -state1[i];
					// consecution: does it have a successor in S ∧ !p?
					// (the S twin over state2 is activated here and in
					// generalization only -- never in candidate queries)
					std::vector<int> a2 = act_assumptions(sf);
					push_assum(a2, -live2_act);
					push_cube(a2, csub, state1);
					a2.push_back(bad2);
					n_blockq++;
					if (!solve_assum(a2))
						goto prune;
					{
						// survivor: exclude this exact state for the rest
						// of the round (permanent clause, active while the
						// round literal is assumed false)
						std::vector<int> clause;
						for (int i = 0; i < (int)csub.size(); i++)
							clause.push_back(csub[i] > 0 ? ez.NOT(csub[i]) : -csub[i]);
						clause.push_back(rl);
						int id = clause.size() == 1 ? clause[0] : ez.expression(ezSAT::OpOr, clause);
						ez.assume(id);
						continue;
					}
				prune:;
					// must exit: prune (generalized)
					std::vector<int> gen = generalize_prune(csub, sf);
					bool has_lit = false;
					for (int lit : gen)
						if (lit)
							has_lit = true;
					add_live_clause(has_lit ? gen : csub, sf);
					pruned_any = true;
					n_prune++;
				}
				// Fair pruning (Emerson-Lei style).  The gfp survivor
				// set must admit a cycle that visits EVERY fair
				// predicate infinitely; a state can lie on such a
				// cycle iff it can stay in S ∧ !p (the consecution
				// prunes above) and reach (S ∧ !p ∧ fair_i) within S
				// for every i.  The attractor Attr_i is a growing cube
				// set -- seeded from fair states, closed under "has a
				// successor in S ∩ Attr_i" -- and S-states outside it
				// are pruned.  The outer round loop then re-prunes
				// against the shrunken S, so the fixpoint is the
				// standard fair gfp: exactly the states that can stay
				// in !p while visiting each fair_i infinitely often.
				for (size_t fi = 0; fi < fairv.size(); fi++) {
					std::vector<std::vector<int>> attr;
					std::vector<int> attr_al;   // discovery exclusion lits
					size_t solves0 = nsolves;
					auto base_assum = [&]() {
						std::vector<int> a = act_assumptions(sf);
						// ALL transient survivor exclusions off: the
						// attractor ranges over all of S (permanent
						// prunes only), including states the consecution
						// loop already examined this round
						for (int rl : round_lits)
							push_assum(a, rl);
						a.push_back(bad1);
						return a;
					};
					// "svec state ∈ Attr" as one expression
					auto attr_expr = [&](const std::vector<int> &svec) {
						std::vector<int> disj;
						for (auto &cube : attr) {
							std::vector<int> conj;
							for (int i = 0; i < (int)cube.size(); i++)
								if (cube[i])
									conj.push_back(cube[i] > 0 ? svec[i] : ez.NOT(svec[i]));
							disj.push_back(conj.size() == 1 ? conj[0]
									: ez.expression(ezSAT::OpAnd, conj));
						}
						return disj.empty() ? ez.CONST_FALSE
								: disj.size() == 1 ? disj[0]
								: ez.expression(ezSAT::OpOr, disj);
					};
					auto add_attr_cube = [&](const std::vector<bool> &model) {
						std::vector<int> cube(state1.size());
						for (int i = 0; i < (int)state1.size(); i++)
							cube[i] = model[i] ? state1[i] : -state1[i];
						attr.push_back(cube);
						// unnamed frozen literal: named literals cache by
						// string in ezSAT, and this name restarts every
						// round -- a named literal would alias the previous
						// round's exclusion clauses and over-exclude
						int al = ez.frozen_literal();
						attr_al.push_back(al);
						std::vector<int> cl;
						for (int i = 0; i < (int)cube.size(); i++)
							cl.push_back(cube[i] > 0 ? ez.NOT(cube[i]) : -cube[i]);
						cl.push_back(al);
						ez.assume(ez.expression(ezSAT::OpOr, cl));
					};
					// seed: enumerate S ∧ !p states where a fair visit can
					// ALSO stay in S ∧ !p (∃ input: fair now and the
					// successor under that input remains -- a fair tick
					// that immediately exits !p cannot be part of a fair
					// !p-cycle)
					while (1) {
						std::vector<int> a = base_assum();
						a.push_back(fairv[fi]);
						push_assum(a, -live2_act);   // S over state2
						a.push_back(bad2);           // successor stays ¬p
						for (int al : attr_al)
							push_assum(a, -al);
						std::vector<bool> model;
						n_bad++;
						if (!solve_model(a, state1, model))
							break;
						add_attr_cube(model);
					}
					// closure: states with a successor in S ∩ Attr
					while (1) {
						std::vector<int> a = base_assum();
						for (int al : attr_al)
							push_assum(a, -al);
						push_assum(a, -live2_act);   // S over state2
						a.push_back(attr_expr(state2));
						std::vector<bool> model;
						n_bad++;
						if (!solve_model(a, state1, model))
							break;
						add_attr_cube(model);
					}
					// prune: S ∧ !p states outside Attr cannot reach
					// fair_i within S, so no fair cycle passes them
					while (1) {
						std::vector<int> a = base_assum();
						for (int al : attr_al)
							push_assum(a, al);   // exclusions off
						a.push_back(ez.NOT(attr_expr(state1)));
						std::vector<bool> model;
						n_bad++;
						if (!solve_model(a, state1, model))
							break;
						std::vector<int> csub(state1.size());
						for (int i = 0; i < (int)state1.size(); i++)
							csub[i] = model[i] ? state1[i] : -state1[i];
						// generalize: drop literals while the cube still
						// misses Attr entirely (cube ∧ Attr UNSAT)
						std::vector<int> gen = csub;
						for (int pass = 0; pass < 4; pass++) {
							bool dropped = false;
							for (int x = 0; x < (int)gen.size(); x++) {
								if (!gen[x])
									continue;
								int saved = gen[x];
								gen[x] = 0;
								std::vector<int> ga;
								push_cube(ga, gen, state1);
								ga.push_back(attr_expr(state1));
								n_gen++;
								if (solve_assum(ga))
									gen[x] = saved;
								else
									dropped = true;
							}
							if (!dropped)
								break;
						}
						bool has_lit = false;
						for (int lit : gen)
							if (lit)
								has_lit = true;
						if (trace) {
							std::string bits;
							for (int i = 0; i < (int)csub.size(); i++)
								bits += csub[i] ? (csub[i] > 0 ? "1" : "0") : "x";
							fprintf(stderr, "ic3-live: fair%d: prune cube %s (gen %d lits)\n",
									(int)fi, bits.c_str(), has_lit ? 1 : 0);
						}
						add_live_clause(has_lit ? gen : csub, sf);
						pruned_any = true;
						n_prune++;
					}
					if (trace)
						fprintf(stderr, "ic3-live: fair%d: attr=%zu solves+%zu S-clauses=%zu\n",
								(int)fi, attr.size(), nsolves - solves0, frames[sf].size());
					if (nsolves - solves0 > 200000) {
						log_warning("ic3: %s: fairness attractor did not converge after 200k solves -- undecided.\n", log_id(prop));
						return 2;
					}
				}
				if (trace)
					fprintf(stderr, "ic3-live: round %d: cand=%d pruned=%d S-clauses=%zu\n",
							GetSize(round_lits), n_cand, n_prune, frames[sf].size());
			}
		}

		// Phase C: is a fair-cycle state REACHABLE from Init?  The
		// disproof of F p needs a reachable state of S* ∧ !p, not an
		// initial one: the bad path starts once a fair-cycle state is
		// entered, so every genuine lasso has a (possibly empty) stem
		// and requiring Init ∈ S* ∧ !p misses all of them and
		// false-proves the property (observed: lasso-style violations
		// reported "proved" while simulation showed the assertion
		// failing forever).  Membership in S* is the conjunction of the
		// prune clauses (¬cube) over the state register bits, so
		// compile reach_bad = member ∧ EN ∧ ¬A into a temporary $check
		// and let the safety core decide reachability on a fresh
		// solver (a hand-nested second fixpoint would need an unrolled
		// reachability encoding this 2-timeframe solver does not
		// have).  member = top (no prune clauses) covers the constant-
		// bad case, where Init itself is the witness.  An undecided
		// reachability result propagates as an undecided liveness
		// verdict rather than a guess.
		SigSpec member;
		for (auto &cube : frames[sf]) {
			SigSpec clause;
			for (int i = 0; i < GetSize(cube); i++) {
				if (!cube[i])
					continue;
				// clause = "some bit differs from the cube" (the cube's
				// match-set is what pruning removed from S)
				SigSpec lit = cube[i] > 0 ? module->Not(NEW_ID, istate_bits.extract(i, 1))
						: istate_bits.extract(i, 1);
				clause = clause.empty() ? lit : module->Or(NEW_ID, clause, lit);
			}
			log_assert(!clause.empty());
			member = member.empty() ? clause : module->And(NEW_ID, member, clause);
		}
		if (member.empty())
			member = SigSpec(State::S1);
		RTLIL::Cell *rc = module->addCell("$ic3_live_reach", ID($check));
		rc->setPort(ID(A), module->Or(NEW_ID, module->Not(NEW_ID, member),
				prop->getPort(ID(A))));
		rc->setPort(ID(EN), SigSpec(State::S1));
		rc->setPort(ID(TRG), SigSpec());
		Ic3Solver reach_solver(module, init_zero, max_frames);
		reach_solver.assume_cells = assume_cells;
		reach_solver.kind_max = kind_max;
		reach_solver.bmc_max = bmc_max;
		reach_solver.ctg_depth_max = ctg_depth_max;
		int rdepth = 0;
		int rr = reach_solver.prove(rc, rdepth);
		module->remove(rc);
		if (trace)
			fprintf(stderr, "ic3-live: phaseC reachability r=%d (depth %d)\n", rr, rdepth);
		if (rr == 2)
			return 2;
		if (rr == 1) {
			depth = 0;
			if (!cex_file.empty() && !state1.empty()) {
				// any S* ∧ !p state seeds the lasso walk; the witness
				// covers the cycle (the reachability stem from Init is
				// not part of the export)
				std::vector<int> assumptions = act_assumptions(sf);
				for (int rl : round_lits)
					push_assum(assumptions, rl);
				assumptions.push_back(bad1);
				std::vector<bool> s0;
				if (solve_model(assumptions, state1, s0))
					export_lasso(cex_file, sf, round_lits, s0);
			}
			return 1;
		}
		return 0;
	}

private:
	// S clauses are asserted over the state1 variables by add_clause and
	// additionally over state2 by this twin, so the consecution query can
	// constrain the successor side as well; add_clause keeps the single
	// bookkeeping entry (subsumption) in frames[sf].  The twin hangs off
	// its OWN activation literal: it may only constrain the consecution
	// query (and generalization), never candidate or Phase C queries --
	// there state2 is the T-forced successor of state1, so an active twin
	// would silently turn "s1 in S" into "s1's successor in S" and invert
	// the gfp (observed on xlive1: state 3 vanished from the candidate
	// pool the moment state 4 was pruned, and the fixpoint collapsed to a
	// false disproof).
	int live2_act;
	void add_live_clause(const std::vector<int> &cube, int sf)
	{
		add_clause(cube, sf, sf);
		std::vector<int> clause;
		for (int i = 0; i < (int)cube.size(); i++)
			if (cube[i])
				clause.push_back(cube[i] > 0 ? ez.NOT(state2[i]) : state2[i]);
		int id = clause.size() == 1 ? clause[0] : ez.expression(ezSAT::OpOr, clause);
		ez.assume(ez.OR(id, live2_act));
	}

	// drop literals while "no successor in S ∧ !p" holds (MIC-style)
	std::vector<int> generalize_prune(std::vector<int> cube, int sf)
	{
		auto t0 = std::chrono::steady_clock::now();
		for (int pass = 0; pass < 4; pass++) {
			bool dropped_any = false;
			for (int x = 0; x < (int)cube.size(); x++) {
				if (cube[x] == 0)
					continue;
				int saved = cube[x];
				cube[x] = 0;
				std::vector<int> a = act_assumptions(sf);
				push_assum(a, -live2_act);
				push_cube(a, cube, state1);
				a.push_back(bad2);
				n_gen++;
				if (solve_assum(a))
					cube[x] = saved;
				else
					dropped_any = true;
			}
			if (!dropped_any)
				break;
		}
		t_generalize += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		return cube;
	}
};

struct Ic3Pass : Pass
{
	Ic3Pass() : Pass("ic3", "prove safety and liveness properties via IC3/PDR") { }

	void help() override
	{
		log("\n");
		log("    ic3 [-top <name>] [-check-cells] [-init-zero] [-max <frames>] [-kind <k>] [-timeout <s>]\n");
		log("        [-ctg <depth>] [-cex <prefix>] [-v]\n");
		log("\n");
		log("Proves (or disproves) the safety properties given as $assert cells and\n");
		log("$check cells with FLAVOR \"assert\" in the selected module, using the\n");
		log("IC3/PDR algorithm on a two-timeframe SAT encoding of the design.\n");
		log("Liveness properties -- $check cells with FLAVOR \"liveness\" (lowered by\n");
		log("sva2check from unbounded eventually/s_eventually, A = p) and $live\n");
		log("cells -- are proved by a two-phase argument: safety IC3 with bad := !p\n");
		log("yields a reachable-state over-approximation, then a greatest-fixpoint\n");
		log("pruning removes states that cannot stay in !p forever; F p holds iff no\n");
		log("initial state survives the pruning.\n");
		log("\n");
		log("The design must be prepared with 'proc; async2sync; flatten; opt_clean'\n");
		log("first: only synchronous flip-flops and combinatorial cells can be\n");
		log("imported into the SAT database. Registers without an \\init attribute\n");
		log("have a free (nondeterministic) initial state unless -init-zero is\n");
		log("given, which forces all registers to start at zero. Disproofs are\n");
		log("reported with the frame at which a bad state was found.\n");
		log("\n");
		log("With -cex <prefix>, each disproved safety property additionally writes a\n");
		log("Yosys witness <prefix><property>.yw of the counterexample (input\n");
		log("valuations per step; replay with the SAME prep and\n");
		log("`sim -r <file>.yw -vcd trace.vcd`). Disproved liveness properties\n");
		log("write a lasso witness: stem followed by one traversal of the\n");
		log("violating cycle, with the loop-start step recorded in a\n");
		log("non-standard \"lasso\" header field and printed in the log (sim -r\n");
		log("ignores the field; pass -append to extend the replay past the\n");
		log("closing step).\n");
		log("\n");
		log("    -top <name>\n");
		log("        module to process (default: top module)\n");
		log("\n");
		log("    -check-cells\n");
		log("        also enumerate $check cells (default behavior; kept for\n");
		log("        compatibility with the sva2check flow)\n");
		log("\n");
		log("    -allow-degraded\n");
		log("        run on a design whose modules carry sv_degraded / sv_permissive\n");
		log("        (printed first either way; refused by default: register v3 B5)\n");
		log("\n");
		log("    -init-zero\n");
		log("        registers without a 0/1 \\init value (no attribute, or x bits)\n");
		log("        start at 0; explicit \\init values are kept (as after\n");
		log("        `setundef -init -zero')\n");
		log("\n");
		log("    -max <frames>\n");
		log("        maximum number of IC3 frames before giving up as undecided\n");
		log("        (default: 1000)\n");
		log("\n");
		log("    -kind <k>\n");
		log("        try k-induction up to depth k before the IC3 loop\n");
		log("        (default: 8; 0 disables)\n");
		log("\n");
		log("    -timeout <seconds>\n");
		log("        wall-clock budget per property / cover query; an exhausted\n");
		log("        query is reported undecided and the next one starts\n");
		log("\n");
		log("    -bmc <depth>\n");
		log("        bounded-model-checking pre-pass: every assert and cover on one\n");
		log("        shared unrolling, asked at t = 1..depth before k-induction and\n");
		log("        IC3; a hit disproves the assert / reaches the cover exactly,\n");
		log("        and each model is scanned for further hits.  Progress is\n");
		log("        logged per depth\n");
		log("        (default: 0 = disabled)\n");
		log("\n");
		log("    -ctg <depth>\n");
		log("        CTG-guided generalization recursion depth, abc-pdr style\n");
		log("        (default: 0 = disabled; experimental, can multiply\n");
		log("        obligations on easy properties)\n");
		log("\n");
		log("    -cex <prefix>\n");
		log("        write a Yosys witness (.yw) per disproved property (safety:\n");
		log("        counterexample; liveness: lasso); replay with the same prep\n");
		log("        and `sim -r <prefix><property>.yw -vcd trace.vcd`\n");
		log("\n");
		log("    -v\n");
		log("        print per-property solve statistics\n");
		log("\n");
	}

	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		std::string top_name;
		std::string cex_prefix;
		bool check_cells = false;
		bool init_zero = false;
		bool allow_degraded = false;
		bool verbose = false;
		int max_frames = 1000;
		double timeout_s = 0;
		// arm the per-query budget (0: none)
		auto arm = [&]() {
			ic3_deadline_on = timeout_s > 0;
			if (ic3_deadline_on)
				ic3_deadline = std::chrono::steady_clock::now() +
						std::chrono::milliseconds((long long)(timeout_s * 1000));
		};
		int kind_max = 8;
		int bmc_max = 0;
		int ctg_depth_max = 0;

		log_header(design, "Executing IC3 pass (safety and liveness model checking).\n");

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-top") {
				top_name = args[++argidx];
				continue;
			}
			if (args[argidx] == "-check-cells") {
				check_cells = true;
				continue;
			}
			if (args[argidx] == "-allow-degraded") {
				allow_degraded = true;
				continue;
			}
			if (args[argidx] == "-init-zero") {
				init_zero = true;
				continue;
			}
			if (args[argidx] == "-max") {
				max_frames = atoi(args[++argidx].c_str());
				continue;
			}
			if (args[argidx] == "-kind") {
				kind_max = atoi(args[++argidx].c_str());
				continue;
			}
			if (args[argidx] == "-ctg") {
				ctg_depth_max = atoi(args[++argidx].c_str());
				continue;
			}
			if (args[argidx] == "-bmc") {
				bmc_max = atoi(args[++argidx].c_str());
				continue;
			}
			if (args[argidx] == "-timeout") {
				timeout_s = atof(args[++argidx].c_str());
				continue;
			}
			if (args[argidx] == "-cex") {
				cex_prefix = args[++argidx];
				continue;
			}
			if (args[argidx] == "-v") {
				verbose = true;
				continue;
			}
			break;
		}
		extra_args(args, argidx, design);

		RTLIL::Module *module;
		if (!top_name.empty()) {
			module = design->module(top_name);
			if (!module)
				log_error("ic3: no such module: %s\n", top_name.c_str());
		} else {
			module = design->top_module();
			if (!module)
				log_error("ic3: no top module found, use -top to specify one.\n");
		}
		(void) check_cells;

		// $initstate is 1 in the initial state only.  async2sync gates
		// every initial-block check with it (`initial assert property`,
		// `expect`, `initial assume`); SatGen sets it from setInitState(),
		// which this engine never calls (its frames are not time steps), so
		// it read as constant 0 at every step and each such assert was
		// PROVED vacuously -- `initial assert property (@(posedge clk) a)`
		// with a free input a (register v3 C1, 2026-10-19; the SMT BMC
		// lane gave cex@0).  Model it as a state bit: Y = !n with n = 0
		// initially (an init attribute, and what -init-zero forces) and 1
		// from the next step on.
		{
			std::vector<RTLIL::Cell *> inits;
			for (auto cell : module->cells())
				if (cell->type == ID($initstate))
					inits.push_back(cell);
			for (auto cell : inits) {
				RTLIL::SigSpec y = cell->getPort(ID::Y);
				RTLIL::Wire *n = module->addWire(NEW_ID);
				n->attributes[ID::init] = RTLIL::Const(0, 1);
				module->addFf(NEW_ID, RTLIL::State::S1, n);
				module->remove(cell);
				module->addNot(NEW_ID, n, y);
			}
			if (!inits.empty())
				log("ic3: modelling %d $initstate cell(s) as an initial-state register.\n", GetSize(inits));
		}

		// Register v3 B5: a verdict on a degraded design is not evidence.
		// The frontend marks every module read with -permissive
		// (* sv_permissive *) and every module where a leniency fired
		// (* sv_degraded = "<reason> @file:line; ..." *).  Print them first;
		// refuse unless the caller says -allow-degraded (the ladder and the
		// mutation leg measure degraded files knowingly).
		{
			int n_deg = 0;
			for (auto mod : design->modules()) {
				if (mod->attributes.count(ID(sv_degraded))) {
					log("ic3: design carries sv_degraded: %s: %s\n", log_id(mod),
							mod->attributes.at(ID(sv_degraded)).decode_string().c_str());
					n_deg++;
				} else if (mod->attributes.count(ID(sv_permissive))) {
					log("ic3: design carries sv_permissive: %s (read with -permissive)\n", log_id(mod));
					n_deg++;
				}
			}
			if (n_deg && !allow_degraded)
				log_error("ic3: refusing a design that carries sv_degraded/sv_permissive modules "
						"(%d): verdicts on a degraded design are not evidence; pass -allow-degraded "
						"to run anyway.\n", n_deg);
		}

		std::vector<RTLIL::Cell*> props;
		std::vector<RTLIL::Cell*> covers;
		std::vector<RTLIL::Cell*> live_props;
		std::vector<RTLIL::Cell*> assumes;
		std::vector<RTLIL::Cell*> fairs;
		for (auto cell : module->cells()) {
			if (cell->type == ID($assert)) {
				props.push_back(cell);
			} else if (cell->type == ID($assume)) {
				assumes.push_back(cell);
			} else if (cell->type == ID($fair)) {
				// assume(s_eventually ...): fairness constraint for the
				// liveness engine (lowered by sva2check or chformal -fair)
				fairs.push_back(cell);
			} else if (cell->type == ID($live)) {
				// $check FLAVOR "live" cells reach $live form via
				// chformal -lower (async2sync); same F p semantics
				live_props.push_back(cell);
			} else if (cell->type == ID($check)) {
				if (cell->getParam(ID::TRG_ENABLE).as_bool())
					log_error("ic3: $check cell %s.%s is edge-triggered -- run `async2sync` first.\n",
							log_id(module), log_id(cell));
				std::string flavor = "assert";
				if (cell->parameters.count(ID(FLAVOR)))
					flavor = cell->getParam(ID(FLAVOR)).decode_string();
				if (flavor == "assert")
					props.push_back(cell);
				else if (flavor == "assume" || flavor == "restrict")
					assumes.push_back(cell);
				else if (flavor == "liveness" || flavor == "live")
					live_props.push_back(cell);
				else if (flavor == "cover")
					covers.push_back(cell);
				else
					log_warning("ic3: skipping %s.%s (flavor %s)\n", log_id(module), log_id(cell), flavor.c_str());
			} else if (cell->type == ID($cover)) {
				covers.push_back(cell);
			}
		}
		// A check sva2check could not lower carries a placeholder driver
		// (sva_lowering = "placeholder:<why>"): any verdict on it is about
		// the placeholder, not the property (a tautology was printed as
		// disproved).  Asserts are reported not checked; an assumption
		// that cannot be modelled makes every other verdict suspect, so it
		// is refused unless -allow-degraded (register v3 C1).
		auto placeholder = [](RTLIL::Cell *c) -> std::string {
			if (!c->attributes.count(ID(sva_lowering)))
				return "";
			std::string t = c->attributes.at(ID(sva_lowering)).decode_string();
			return t.compare(0, 12, "placeholder:") == 0 ? t : "";
		};
		for (auto *vec : {&props, &live_props}) {
			std::vector<RTLIL::Cell *> keep;
			for (auto cell : *vec) {
				std::string t = placeholder(cell);
				if (t.empty())
					keep.push_back(cell);
				else
					log("ic3: %s: not checked [lowering=%s]\n", cell->name.unescape().c_str(), t.c_str());
			}
			*vec = keep;
		}
		for (auto cell : assumes) {
			std::string t = placeholder(cell);
			if (t.empty())
				continue;
			if (!allow_degraded)
				log_error("ic3: assumption %s has no lowering [%s]; every verdict would rest on a placeholder "
						"(pass -allow-degraded to proceed).\n", cell->name.unescape().c_str(), t.c_str());
			log_warning("ic3: assumption %s has no lowering [%s]; verdicts below rest on a placeholder.\n",
					cell->name.unescape().c_str(), t.c_str());
		}

		// Covers that sva2check could not lower are said so; the rest are
		// checked below as reachability (register v3 D4)
		{
			std::vector<RTLIL::Cell *> keep;
			for (auto cell : covers) {
				std::string t = placeholder(cell);
				if (!t.empty()) {
					std::string tag = cell->attributes.count(ID(sva_lowering))
							? cell->attributes.at(ID(sva_lowering)).decode_string() : "untagged";
					log("ic3: %s: cover not checked [lowering=%s]\n", cell->name.unescape().c_str(), tag.c_str());
				} else
					keep.push_back(cell);
			}
			covers = keep;
		}

		if (props.empty() && live_props.empty() && covers.empty()) {
			log("ic3: no properties found in module %s.\n", log_id(module));
			return;
		}

		if (!assumes.empty())
			log("ic3: %d assume constraint(s) will be applied at both timeframes.\n", GetSize(assumes));
		if (init_zero)
			log("ic3: registers without a 0/1 \\init value start at zero (explicit \\init values are kept).\n");
		else
			log("ic3: registers without \\init attribute have a free initial state.\n");

		// Batched BMC pre-pass (-bmc N).  The per-query pre-pass built a
		// fresh N+1-step unrolling for every check: on the Deburster FVIP
		// harness (152 covers, ~5M clauses per unrolling) that alone ate
		// most of each 60s query budget and 138 covers came back undecided
		// that smtbmc -c reaches in 2s.  Encode the union cone of every
		// assert and cover ONCE, ask each open target at t = 1..N, and
		// harvest from each model every other target it also hits (as
		// smtbmc does).  Hits are exact (depth = t-1); misses fall through
		// to k-induction and IC3 with the per-query pre-pass off.
		dict<RTLIL::Cell*, int> bmc_hit;
		if (bmc_max > 0 && (!props.empty() || !covers.empty())) {
			auto t0 = std::chrono::steady_clock::now();
			auto elapsed = [&]() {
				return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			};
			std::vector<RTLIL::Cell*> targets = props;
			int first_cover = GetSize(targets);
			targets.insert(targets.end(), covers.begin(), covers.end());
			int ntargets = GetSize(targets);
			Ic3Solver bs(module, init_zero, max_frames);
			bs.assume_cells = assumes;
			bs.extra_seeds = targets;
			bs.encode(targets[0], bmc_max + 1);
			log("ic3: bmc pre-pass: %d assert(s) and %d cover(s) on one %d-step unrolling "
					"(%d clauses, %d variables, encoded in %.1fs)\n",
					first_cover, ntargets - first_cover, bmc_max + 1,
					bs.ez.numCnfClauses(), bs.ez.numCnfVariables(), elapsed());
			// hit := EN && !A for an assert; EN && A for a cover (the
			// inverted assert the cover loop below checks)
			auto target_lit = [&](int k, int t) {
				RTLIL::Cell *c = targets[k];
				std::vector<int> a = bs.satgen.importSigSpec(c->getPort(ID::A), t);
				std::vector<int> en = bs.satgen.importSigSpec(c->getPort(ID::EN), t);
				if (k >= first_cover)
					a = bs.ez.vec_not(a);
				while (a.size() < en.size()) a.push_back(bs.ez.CONST_FALSE);
				while (en.size() < a.size()) en.push_back(bs.ez.CONST_FALSE);
				return bs.ez.vec_reduce_and(bs.ez.vec_and(en, bs.ez.vec_not(a)));
			};
			std::vector<bool> open(ntargets, true);
			int nopen = ntargets;
			int t;
			for (t = 1; t <= bmc_max && nopen > 0; t++) {
				std::vector<int> lits(ntargets, 0);
				for (int k = 0; k < ntargets; k++)
					if (open[k])
						lits[k] = target_lit(k, t);
				int hits = 0;
				for (int k = 0; k < ntargets; k++) {
					if (!open[k])
						continue;
					std::vector<int> assumptions;
					bs.push_assum(assumptions, -bs.init_act);
					assumptions.push_back(lits[k]);
					std::vector<int> watch, idx;
					for (int j = 0; j < ntargets; j++)
						if (open[j]) {
							watch.push_back(lits[j]);
							idx.push_back(j);
						}
					std::vector<bool> model;
					bool sat;
					arm();
					try {
						sat = bs.solve_model(assumptions, watch, model);
					} catch (Ic3Timeout &) {
						ic3_deadline_on = false;
						open[k] = false;
						nopen--;
						log("ic3: %s: bmc pre-pass budget of %gs exhausted at depth %d\n",
								targets[k]->name.unescape().c_str(), timeout_s, t - 1);
						continue;
					}
					ic3_deadline_on = false;
					if (!sat)
						continue;
					for (int i = 0; i < GetSize(idx); i++)
						if (model[i] && open[idx[i]]) {
							open[idx[i]] = false;
							nopen--;
							hits++;
							bmc_hit[targets[idx[i]]] = t - 1;
							log("ic3: %s: bmc pre-pass hit at depth %d\n",
									targets[idx[i]]->name.unescape().c_str(), t - 1);
						}
				}
				log("ic3: bmc pre-pass: depth %d: %d hit(s), %d open (%.1fs)\n", t - 1, hits, nopen, elapsed());
			}
			log("ic3: bmc pre-pass: %d of %d target(s) hit within depth %d (%.1fs)\n",
					GetSize(bmc_hit), ntargets, t - 2, elapsed());
		}

		// lowering provenance from sva2check (register v3 A3): printed
		// next to every verdict so no PROVED hides an approximation
		auto lowering_tag = [](RTLIL::Cell *c) -> std::string {
			if (!c->attributes.count(ID(sva_lowering)))
				return std::string(" [lowering=untagged]");
			return " [lowering=" + c->attributes.at(ID(sva_lowering)).decode_string() + "]";
		};
		int n_proved = 0, n_disproved = 0, n_undecided = 0;
		for (auto cell : props) {
			std::string name = cell->name.unescape();
			std::string src;
			if (cell->attributes.count(ID::src))
				src = cell->attributes.at(ID::src).decode_string();

			Ic3Solver solver(module, init_zero, max_frames);
			solver.assume_cells = assumes;
			solver.fair_cells = fairs;
			solver.kind_max = kind_max;
			solver.bmc_max = 0;	// run batched above
			solver.ctg_depth_max = ctg_depth_max;

			int depth = 0;
			int result;
			if (bmc_hit.count(cell)) {
				result = 1;
				depth = bmc_hit.at(cell);
			} else {
				arm();
				try {
					result = solver.prove(cell, depth);
				} catch (Ic3Timeout &) {
					result = 2;
					log("ic3: %s: query budget of %gs exhausted\n", name.c_str(), timeout_s);
				}
				ic3_deadline_on = false;
			}

			if (result == 0) {
				log("ic3: %s: proved%s\n", name.c_str(), lowering_tag(cell).c_str());
				n_proved++;
			} else if (result == 1) {
				log("ic3: %s: disproved (depth %d)%s\n", name.c_str(), depth, lowering_tag(cell).c_str());
				n_disproved++;
				if (!cex_prefix.empty()) {
					std::string safe = name;
					for (auto &ch : safe)
						if (!isalnum((unsigned char)ch) && ch != '_' && ch != '-')
							ch = '_';
					solver.export_cex(cell, stringf("%s%s.yw", cex_prefix.c_str(), safe.c_str()), depth);
				}
			} else {
				log("ic3: %s: undecided%s\n", name.c_str(), lowering_tag(cell).c_str());
				n_undecided++;
			}

			if (verbose) {
				int nclauses = 0;
				for (int i = 1; i < GetSize(solver.frames); i++)
					nclauses += GetSize(solver.frames[i]);
				log("ic3: %s: src=%s frames=%d clauses=%d solves=%d (bad=%lld blk=%lld gen=%lld init=%lld)\n",
						name.c_str(), src.c_str(), GetSize(solver.act)-1, nclauses, solver.nsolves,
						solver.n_bad, solver.n_blockq, solver.n_gen, solver.n_init);
			}
		}

		// Register v3 D4: a cover is REACHED iff EN && A holds in some
		// reachable state of a step -- i.e. iff the assertion "A never
		// holds" (A' = !A, same EN) is disproved; its counterexample step is
		// the reach step, a proof means unreachable.  Assumptions constrain
		// as for asserts.  Each cover is its own query.
		int n_reached = 0, n_unreachable = 0, n_cover_undecided = 0;
		for (auto cell : covers) {
			std::string name = cell->name.unescape();
			RTLIL::Wire *na = module->addWire(NEW_ID);
			RTLIL::Cell *ncell = module->addNot(NEW_ID, cell->getPort(ID::A), na);
			RTLIL::Cell *tmp;
			if (cell->type == ID($check)) {
				tmp = module->addCell(NEW_ID, ID($check));
				tmp->parameters = cell->parameters;
				tmp->setParam(ID(FLAVOR), std::string("assert"));
				for (auto &conn : cell->connections())
					tmp->setPort(conn.first, conn.second);
				tmp->setPort(ID::A, na);
			} else {
				// $cover (chformal -lower / async2sync form)
				tmp = module->addAssert(NEW_ID, na, cell->getPort(ID::EN));
			}

			Ic3Solver solver(module, init_zero, max_frames);
			solver.assume_cells = assumes;
			solver.fair_cells = fairs;
			solver.kind_max = kind_max;
			solver.bmc_max = 0;	// run batched above
			solver.ctg_depth_max = ctg_depth_max;
			int depth = 0;
			int result;
			if (bmc_hit.count(cell)) {
				result = 1;
				depth = bmc_hit.at(cell);
			} else {
				arm();
				try {
					result = solver.prove(tmp, depth);
				} catch (Ic3Timeout &) {
					result = 2;
					log("ic3: %s: query budget of %gs exhausted\n", name.c_str(), timeout_s);
				}
				ic3_deadline_on = false;
			}
			if (result == 1) {
				log("ic3: %s: cover reached (depth %d)%s\n", name.c_str(), depth, lowering_tag(cell).c_str());
				n_reached++;
				if (!cex_prefix.empty()) {
					std::string safe = name;
					for (auto &ch : safe)
						if (!isalnum((unsigned char)ch) && ch != '_' && ch != '-')
							ch = '_';
					solver.export_cex(tmp, stringf("%s%s.yw", cex_prefix.c_str(), safe.c_str()), depth);
				}
			} else if (result == 0) {
				log("ic3: %s: cover unreachable%s\n", name.c_str(), lowering_tag(cell).c_str());
				n_unreachable++;
			} else {
				log("ic3: %s: cover undecided%s\n", name.c_str(), lowering_tag(cell).c_str());
				n_cover_undecided++;
			}
			module->remove(tmp);
			module->remove(ncell);
			module->remove({na});
		}
		if (!covers.empty())
			log("ic3: covers: %d reached, %d unreachable, %d undecided.\n", n_reached, n_unreachable, n_cover_undecided);

		for (auto cell : live_props) {
			std::string name = cell->name.unescape();
			std::string src;
			if (cell->attributes.count(ID::src))
				src = cell->attributes.at(ID::src).decode_string();

			Ic3Solver solver(module, init_zero, max_frames);
			solver.assume_cells = assumes;
			solver.fair_cells = fairs;
			solver.kind_max = kind_max;
			solver.bmc_max = 0;	// run batched above
			solver.ctg_depth_max = ctg_depth_max;

			std::string cex_file;
			if (!cex_prefix.empty()) {
				std::string safe = name;
				for (auto &ch : safe)
					if (!isalnum((unsigned char)ch) && ch != '_' && ch != '-')
						ch = '_';
				cex_file = stringf("%s%s.yw", cex_prefix.c_str(), safe.c_str());
			}

			int depth = 0;
			int result;
			arm();
			try {
				result = solver.prove_liveness(cell, depth, cex_file);
			} catch (Ic3Timeout &) {
				result = 2;
				log("ic3: %s: query budget of %gs exhausted\n", name.c_str(), timeout_s);
			}
			ic3_deadline_on = false;

			if (result == 0) {
				log("ic3: %s: proved%s\n", name.c_str(), lowering_tag(cell).c_str());
				n_proved++;
			} else if (result == 1) {
				log("ic3: %s: disproved (depth %d)%s\n", name.c_str(), depth, lowering_tag(cell).c_str());
				n_disproved++;
			} else {
				log("ic3: %s: undecided%s\n", name.c_str(), lowering_tag(cell).c_str());
				n_undecided++;
			}

			if (verbose) {
				int nclauses = 0;
				for (int i = 1; i < GetSize(solver.frames); i++)
					nclauses += GetSize(solver.frames[i]);
				log("ic3: %s: src=%s frames=%d clauses=%d solves=%d liveness (bad=%lld blk=%lld gen=%lld init=%lld)\n",
						name.c_str(), src.c_str(), GetSize(solver.act)-1, nclauses, solver.nsolves,
						solver.n_bad, solver.n_blockq, solver.n_gen, solver.n_init);
			}
		}

		log("ic3: %d properties: proved=%d disproved=%d undecided=%d\n",
				GetSize(props) + GetSize(live_props), n_proved, n_disproved, n_undecided);
	}
} Ic3Pass;

PRIVATE_NAMESPACE_END
