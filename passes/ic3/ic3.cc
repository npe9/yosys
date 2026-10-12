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

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

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

		if (!minisat_solver->solve(assumps))
			return false;

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

	// state vector: one entry per register bit (t=1 and t=2 literals)
	std::vector<int> state1, state2;
	int bad1, bad2;

	// frame activation literals and per-frame clause (cube) sets, 1-based;
	// index 0 is an unused placeholder so frame numbers match the literature.
	// init_act gates the initial-state constraints: they only belong to
	// frame 0, frames >= 1 range over arbitrary states
	std::vector<int> act;
	int init_act;
	std::vector<std::vector<std::vector<int>>> frames;

	int nsolves;
	long long n_bad, n_blockq, n_gen, n_init;

	Ic3Solver(RTLIL::Module *module, bool init_zero, int max_frames) :
			module(module), sigmap(module), ez(), satgen(&ez, &sigmap),
			init_zero(init_zero), max_frames(max_frames), nsolves(0),
			n_bad(0), n_blockq(0), n_gen(0), n_init(0)
	{
		act.push_back(0);
		frames.emplace_back();
		init_act = ez.frozen_literal("init");
		bad1 = bad2 = 0;
	}

	static bool is_formal_cell(IdString type)
	{
		return type.in(ID($assert), ID($assume), ID($cover), ID($check),
				ID($live), ID($fair), ID($print));
	}

	void encode(RTLIL::Cell *prop)
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
		for (auto cell : assume_cells) {
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

		// two-timeframe unraveling: comb logic at t=1 and t=2, FF transition
		// constraints tie Q@2 to D@1; Q@1 stays a free variable
		for (int t = 1; t <= 2; t++)
			for (auto cell : coi_cells) {
				if (cell->type == ID($mem) || cell->type == ID($pow))
					log_error("ic3: %s cell %s.%s is not supported -- run `memory_dff; memory_collect; opt_clean` first.\n",
							log_id(cell->type), log_id(module), log_id(cell));
				if (!satgen.importCell(cell, t))
					log_error("ic3: unsupported cell %s.%s (type %s) -- run `proc; async2sync; flatten; opt_clean` first.\n",
							log_id(module), log_id(cell), log_id(cell->type));
			}

		// assume-flavor checks are permanent constraints at both timeframes
		for (auto cell : assume_cells)
			for (int t = 1; t <= 2; t++) {
				std::vector<int> a = satgen.importSigSpec(cell->getPort(ID::A), t);
				std::vector<int> en = satgen.importSigSpec(cell->getPort(ID::EN), t);
				while (a.size() < en.size()) a.push_back(ez.CONST_TRUE);
				while (en.size() < a.size()) en.push_back(ez.CONST_FALSE);
				ez.assume(ez.vec_reduce_and(ez.vec_or(a, ez.vec_not(en))));
			}

		// initial state: \init attributes where present (Sx bits stay free),
		// everything else free unless -init-zero forces all registers to 0
		RTLIL::SigSpec big_lhs, big_rhs;
		if (init_zero) {
			RTLIL::SigSpec rem = satgen.initial_state.export_all();
			big_lhs.append(rem);
			big_rhs.append(RTLIL::SigSpec(RTLIL::State::S0, rem.size()));
		} else {
			for (auto wire : module->wires()) {
				if (wire->attributes.count(ID::init) == 0)
					continue;
				RTLIL::SigSpec lhs = sigmap(wire);
				RTLIL::SigSpec rhs = wire->attributes.at(ID::init);
				log_assert(lhs.size() == rhs.size());
				for (int i = 0; i < lhs.size(); i++) {
					RTLIL::SigSpec bit = lhs.extract(i, 1);
					if (rhs[i] == RTLIL::State::Sx || !satgen.initial_state.check_all(bit)) {
						lhs.remove(i, 1);
						rhs.remove(i, 1);
						i--;
					}
				}
				if (lhs.size()) {
					big_lhs.remove2(lhs, &big_rhs);
					big_lhs.append(lhs);
					big_rhs.append(rhs);
				}
			}
		}
		if (big_lhs.size())
			ez.assume(ez.OR(satgen.signals_eq(big_lhs, big_rhs, 1), init_act));

		// ordered state vector (register Q bits only)
		RTLIL::SigSpec istates = satgen.initial_state.export_all();
		for (auto bit : istates) {
			state1.push_back(satgen.importSigBit(bit, 1));
			state2.push_back(satgen.importSigBit(bit, 2));
		}

		// bad := en && !a  (per bit, padded vacuously with zeros)
		auto build_bad = [&](int t) {
			std::vector<int> a = satgen.importSigSpec(prop->getPort(ID::A), t);
			std::vector<int> en = satgen.importSigSpec(prop->getPort(ID::EN), t);
			while (a.size() < en.size()) a.push_back(ez.CONST_FALSE);
			while (en.size() < a.size()) en.push_back(ez.CONST_FALSE);
			return ez.vec_reduce_and(ez.vec_and(en, ez.vec_not(a)));
		};
		bad1 = build_bad(1);
		bad2 = build_bad(2);
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
	}

	bool solve_assum(const std::vector<int> &assumptions)
	{
		std::vector<int> exprs;
		std::vector<bool> vals;
		nsolves++;
		if (nsolves % 100000 == 0)
			fprintf(stderr, "ic3-debug: %d solves, %d frames, clauses=%d (blk=%lld gen=%lld init=%lld bad=%lld)\n",
					nsolves, (int)act.size(), ez.numCnfClauses(), n_blockq, n_gen, n_init, n_bad);
		return ez.solve(exprs, vals, assumptions);
	}

	bool solve_model(const std::vector<int> &assumptions, const std::vector<int> &svec, std::vector<bool> &model)
	{
		std::vector<int> exprs = svec;
		model.clear();
		nsolves++;
		if (nsolves % 100000 == 0)
			fprintf(stderr, "ic3-debug: %d solves, %d frames, clauses=%d (blk=%lld gen=%lld init=%lld bad=%lld)\n",
					nsolves, (int)act.size(), ez.numCnfClauses(), n_blockq, n_gen, n_init, n_bad);
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
	// Init and inductive relative to frame lvl
	std::vector<int> generalize(std::vector<int> cube, int lvl)
	{
		std::vector<int> ind_assumptions = act_assumptions(lvl);
		// with -init-zero the unique initial state is the all-zero state,
		// so Init-disjointness of a cube is just "has a positive literal"
		bool init_is_allzero = init_zero;
		std::vector<int> init_assumptions;
		push_assum(init_assumptions, -init_act);
		for (int x = 0; x < (int)cube.size(); x++) {
			if (cube[x] == 0)
				continue;
			int saved = cube[x];
			cube[x] = 0;
			n_gen++;
			std::vector<int> a = ind_assumptions;
			push_cube(a, cube, state2);
			bool ok = !solve_assum(a);
			if (ok && !init_is_allzero) {
				std::vector<int> b = init_assumptions;
				push_cube(b, cube, state1);
				ok = !solve_assum(b);
			}
			if (ok && init_is_allzero) {
				bool has_pos = false;
				for (int lit : cube)
					if (lit > 0)
						has_pos = true;
				ok = has_pos;
			}
			if (!ok)
				cube[x] = saved;
		}
		return cube;
	}

	bool block(std::vector<int> cube, int k)
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
					add_clause(generalize(cube, j-1), j, k);
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

	// 0 = proved, 1 = disproved (depth set), 2 = undecided (depth set)
	int prove(RTLIL::Cell *prop, int &depth)
	{
		encode(prop);
		fprintf(stderr, "ic3-debug: proving %s clauses=%d vars=%d states=%d\n", log_id(prop), ez.numCnfClauses(), ez.numCnfVariables(), GetSize(state1));

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
};

struct Ic3Pass : Pass
{
	Ic3Pass() : Pass("ic3", "prove safety properties via IC3/PDR") { }

	void help() override
	{
		log("\n");
		log("    ic3 [-top <name>] [-check-cells] [-init-zero] [-max <frames>] [-v]\n");
		log("\n");
		log("Proves (or disproves) the safety properties given as $assert cells and\n");
		log("$check cells with FLAVOR \"assert\" in the selected module, using the\n");
		log("IC3/PDR algorithm on a two-timeframe SAT encoding of the design.\n");
		log("\n");
		log("The design must be prepared with 'proc; async2sync; flatten; opt_clean'\n");
		log("first: only synchronous flip-flops and combinatorial cells can be\n");
		log("imported into the SAT database. Registers without an \\init attribute\n");
		log("have a free (nondeterministic) initial state unless -init-zero is\n");
		log("given, which forces all registers to start at zero. Disproofs are\n");
		log("reported with the frame at which a bad state was found.\n");
		log("\n");
		log("    -top <name>\n");
		log("        module to process (default: top module)\n");
		log("\n");
		log("    -check-cells\n");
		log("        also enumerate $check cells (default behavior; kept for\n");
		log("        compatibility with the sva2check flow)\n");
		log("\n");
		log("    -init-zero\n");
		log("        force all registers to a zero initial state\n");
		log("\n");
		log("    -max <frames>\n");
		log("        maximum number of IC3 frames before giving up as undecided\n");
		log("        (default: 1000)\n");
		log("\n");
		log("    -v\n");
		log("        print per-property solve statistics\n");
		log("\n");
	}

	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		std::string top_name;
		bool check_cells = false;
		bool init_zero = false;
		bool verbose = false;
		int max_frames = 1000;

		log_header(design, "Executing IC3 pass (safety model checking).\n");

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
			if (args[argidx] == "-init-zero") {
				init_zero = true;
				continue;
			}
			if (args[argidx] == "-max") {
				max_frames = atoi(args[++argidx].c_str());
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

		std::vector<RTLIL::Cell*> props;
		std::vector<RTLIL::Cell*> assumes;
		for (auto cell : module->cells()) {
			if (cell->type == ID($assert)) {
				props.push_back(cell);
			} else if (cell->type == ID($assume)) {
				assumes.push_back(cell);
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
				else
					log_warning("ic3: skipping %s.%s (flavor %s)\n", log_id(module), log_id(cell), flavor.c_str());
			}
		}

		if (props.empty()) {
			log("ic3: no properties found in module %s.\n", log_id(module));
			return;
		}

		if (!assumes.empty())
			log("ic3: %d assume constraint(s) will be applied at both timeframes.\n", GetSize(assumes));
		if (init_zero)
			log("ic3: all registers forced to zero initial state.\n");
		else
			log("ic3: registers without \\init attribute have a free initial state.\n");

		int n_proved = 0, n_disproved = 0, n_undecided = 0;
		for (auto cell : props) {
			std::string name = cell->name.unescape();
			std::string src;
			if (cell->attributes.count(ID::src))
				src = cell->attributes.at(ID::src).decode_string();

			Ic3Solver solver(module, init_zero, max_frames);
			solver.assume_cells = assumes;

			int depth = 0;
			int result = solver.prove(cell, depth);

			if (result == 0) {
				log("ic3: %s: proved\n", name.c_str());
				n_proved++;
			} else if (result == 1) {
				log("ic3: %s: disproved (depth %d)\n", name.c_str(), depth);
				n_disproved++;
			} else {
				log("ic3: %s: undecided\n", name.c_str());
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

		log("ic3: %d properties: proved=%d disproved=%d undecided=%d\n",
				GetSize(props), n_proved, n_disproved, n_undecided);
	}
} Ic3Pass;

PRIVATE_NAMESPACE_END
