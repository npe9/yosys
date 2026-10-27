/*
 *  ic3 -engine bitwuzla: an in-process word-level BMC / k-induction engine
 *  on Bitwuzla's C API (register E8).  The module is lowered to the
 *  Functional IR (word-level ops, memories as arrays) and every timeframe
 *  is built as Bitwuzla terms directly -- no SMT-LIB text, no solver
 *  process, one incremental solver for all checks of the module.
 *
 *  Semantics follow the native engine (fe36): constraints bind the steps
 *  up to and including the step asked about; registers without a 0/1 init
 *  are free (or zero with -init-zero); x constants are 0 (setundef -zero,
 *  as SatGen reads them); undriven nets are free per step.
 *
 *  Building the IR calls log_error on designs it cannot represent, and
 *  Bitwuzla aborts on internal errors; both end the process.  The engine
 *  therefore runs in a forked child that reports one line per check
 *  through a pipe, so a failure there is an "unsupported" verdict, not a
 *  yosys crash.
 */

#include "kernel/yosys.h"
#include "kernel/functional.h"
#include "kernel/mem.h"
#include "passes/ic3/smt_bitwuzla.h"

#ifdef YOSYS_ENABLE_BITWUZLA

#include <bitwuzla/c/bitwuzla.h>
#include <chrono>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

YOSYS_NAMESPACE_BEGIN

namespace {

struct Unroller
{
	BitwuzlaTermManager *tm;
	Functional::IR &ir;
	bool init_zero;
	BitwuzlaSort bool_sort;
	dict<int, BitwuzlaSort> bv_sorts;
	BitwuzlaTerm one1, zero1;
	int nconsts = 0;

	// per frame: term of every IR node, and the state terms
	std::vector<std::vector<BitwuzlaTerm>> node_term;
	std::vector<dict<IdString, BitwuzlaTerm>> state_term;
	dict<IdString, BitwuzlaTerm> anyconst_term;

	Unroller(BitwuzlaTermManager *tm, Functional::IR &ir, bool init_zero) :
			tm(tm), ir(ir), init_zero(init_zero)
	{
		bool_sort = bitwuzla_mk_bool_sort(tm);
		one1 = bitwuzla_mk_bv_one(tm, bv(1));
		zero1 = bitwuzla_mk_bv_zero(tm, bv(1));
	}

	BitwuzlaSort bv(int w)
	{
		auto it = bv_sorts.find(w);
		if (it != bv_sorts.end())
			return it->second;
		return bv_sorts[w] = bitwuzla_mk_bv_sort(tm, w);
	}
	BitwuzlaSort sort_of(const Functional::Sort &s)
	{
		if (s.is_memory())
			return bitwuzla_mk_array_sort(tm, bv(s.addr_width()), bv(s.data_width()));
		return bv(s.width());
	}
	BitwuzlaTerm fresh(BitwuzlaSort s)
	{
		return bitwuzla_mk_const(tm, s, nullptr);
	}
	BitwuzlaTerm value(const RTLIL::Const &c)
	{
		std::string s;
		for (int i = GetSize(c); i-- > 0; )
			s += c[i] == State::S1 ? '1' : '0';
		return bitwuzla_mk_bv_value(tm, bv(GetSize(c)), s.c_str(), 2);
	}
	BitwuzlaTerm t1(BitwuzlaKind k, BitwuzlaTerm a) { return bitwuzla_mk_term1(tm, k, a); }
	BitwuzlaTerm t2(BitwuzlaKind k, BitwuzlaTerm a, BitwuzlaTerm b) { return bitwuzla_mk_term2(tm, k, a, b); }
	BitwuzlaTerm t3(BitwuzlaKind k, BitwuzlaTerm a, BitwuzlaTerm b, BitwuzlaTerm c) { return bitwuzla_mk_term3(tm, k, a, b, c); }
	BitwuzlaTerm from_bool(BitwuzlaTerm b) { return t3(BITWUZLA_KIND_ITE, b, one1, zero1); }
	BitwuzlaTerm to_bool(BitwuzlaTerm a) { return t2(BITWUZLA_KIND_EQUAL, a, one1); }

	// a: bits that are 0/1 in c are fixed, x bits free (or 0 with
	// init_zero); returns the constraint (true when nothing is fixed)
	BitwuzlaTerm init_eq(BitwuzlaTerm a, const RTLIL::Const &c)
	{
		std::string mask, val;
		bool any = false;
		for (int i = GetSize(c); i-- > 0; ) {
			bool def = c[i] == State::S0 || c[i] == State::S1 || init_zero;
			mask += def ? '1' : '0';
			val += c[i] == State::S1 ? '1' : '0';
			any = any || def;
		}
		if (!any)
			return bitwuzla_mk_true(tm);
		BitwuzlaTerm m = bitwuzla_mk_bv_value(tm, bv(GetSize(c)), mask.c_str(), 2);
		BitwuzlaTerm v = bitwuzla_mk_bv_value(tm, bv(GetSize(c)), val.c_str(), 2);
		return t2(BITWUZLA_KIND_EQUAL, t2(BITWUZLA_KIND_BV_AND, a, m), v);
	}

	// the initial-state constraint of frame 0's state terms; "" or why the
	// initial state cannot be expressed
	std::string initial(BitwuzlaTerm &init)
	{
		std::vector<BitwuzlaTerm> conj;
		for (auto st : ir.states()) {
			BitwuzlaTerm s = state_term[0].at(st->name);
			if (st->sort.is_signal()) {
				conj.push_back(init_eq(s, st->initial_value_signal()));
				continue;
			}
			const MemContents &mc = st->initial_value_memory();
			int aw = st->sort.addr_width();
			if (mc.empty()) {
				RTLIL::Const d = mc.default_value();
				if (d.is_fully_undef() && !init_zero)
					continue;	// free array
				// every word equals the (x -> 0) default
				conj.push_back(t2(BITWUZLA_KIND_EQUAL, s,
						bitwuzla_mk_const_array(tm, sort_of(st->sort), value(d))));
				continue;
			}
			if (aw > 12)
				return stringf("memory %s with %d address bits has initial contents", log_id(st->name), aw);
			for (int i = 0; i < (1 << aw); i++) {
				BitwuzlaTerm word = t2(BITWUZLA_KIND_ARRAY_SELECT, s, value(RTLIL::Const(i, aw)));
				conj.push_back(init_eq(word, mc[i]));
			}
		}
		init = conj.empty() ? bitwuzla_mk_true(tm) : conj.size() == 1 ? conj[0]
				: bitwuzla_mk_term(tm, BITWUZLA_KIND_AND, conj.size(), conj.data());
		return "";
	}

	struct Visitor : public Functional::AbstractVisitor<BitwuzlaTerm> {
		using Node = Functional::Node;
		Unroller &u;
		int t;
		Visitor(Unroller &u, int t) : u(u), t(t) {}
		BitwuzlaTerm n(Node x) { return u.node_term[t].at(x.id()); }
		BitwuzlaTerm ext(Node a, Node b, BitwuzlaKind k) {
			// shift with a distance of a different width: compute in the
			// wider width (a zero- or sign-extended), keep a's low bits
			int wa = a.width(), wb = b.width();
			BitwuzlaTerm x = n(a), y = n(b);
			if (wb < wa)
				y = bitwuzla_mk_term1_indexed1(u.tm, BITWUZLA_KIND_BV_ZERO_EXTEND, y, wa - wb);
			if (wb > wa) {
				x = bitwuzla_mk_term1_indexed1(u.tm, k == BITWUZLA_KIND_BV_ASHR ? BITWUZLA_KIND_BV_SIGN_EXTEND
						: BITWUZLA_KIND_BV_ZERO_EXTEND, x, wb - wa);
				return bitwuzla_mk_term1_indexed2(u.tm, BITWUZLA_KIND_BV_EXTRACT, u.t2(k, x, y), wa - 1, 0);
			}
			return u.t2(k, x, y);
		}
		BitwuzlaTerm buf(Node, Node a) override { return n(a); }
		BitwuzlaTerm slice(Node, Node a, int offset, int w) override {
			return bitwuzla_mk_term1_indexed2(u.tm, BITWUZLA_KIND_BV_EXTRACT, n(a), offset + w - 1, offset);
		}
		BitwuzlaTerm zero_extend(Node, Node a, int w) override {
			return w == a.width() ? n(a) : bitwuzla_mk_term1_indexed1(u.tm, BITWUZLA_KIND_BV_ZERO_EXTEND, n(a), w - a.width());
		}
		BitwuzlaTerm sign_extend(Node, Node a, int w) override {
			return w == a.width() ? n(a) : bitwuzla_mk_term1_indexed1(u.tm, BITWUZLA_KIND_BV_SIGN_EXTEND, n(a), w - a.width());
		}
		BitwuzlaTerm concat(Node, Node a, Node b) override { return u.t2(BITWUZLA_KIND_BV_CONCAT, n(b), n(a)); }
		BitwuzlaTerm add(Node, Node a, Node b) override { return u.t2(BITWUZLA_KIND_BV_ADD, n(a), n(b)); }
		BitwuzlaTerm sub(Node, Node a, Node b) override { return u.t2(BITWUZLA_KIND_BV_SUB, n(a), n(b)); }
		BitwuzlaTerm mul(Node, Node a, Node b) override { return u.t2(BITWUZLA_KIND_BV_MUL, n(a), n(b)); }
		BitwuzlaTerm unsigned_div(Node, Node a, Node b) override { return u.t2(BITWUZLA_KIND_BV_UDIV, n(a), n(b)); }
		BitwuzlaTerm unsigned_mod(Node, Node a, Node b) override { return u.t2(BITWUZLA_KIND_BV_UREM, n(a), n(b)); }
		BitwuzlaTerm bitwise_and(Node, Node a, Node b) override { return u.t2(BITWUZLA_KIND_BV_AND, n(a), n(b)); }
		BitwuzlaTerm bitwise_or(Node, Node a, Node b) override { return u.t2(BITWUZLA_KIND_BV_OR, n(a), n(b)); }
		BitwuzlaTerm bitwise_xor(Node, Node a, Node b) override { return u.t2(BITWUZLA_KIND_BV_XOR, n(a), n(b)); }
		BitwuzlaTerm bitwise_not(Node, Node a) override { return u.t1(BITWUZLA_KIND_BV_NOT, n(a)); }
		BitwuzlaTerm unary_minus(Node, Node a) override { return u.t1(BITWUZLA_KIND_BV_NEG, n(a)); }
		BitwuzlaTerm reduce_and(Node, Node a) override { return u.t1(BITWUZLA_KIND_BV_REDAND, n(a)); }
		BitwuzlaTerm reduce_or(Node, Node a) override { return u.t1(BITWUZLA_KIND_BV_REDOR, n(a)); }
		BitwuzlaTerm reduce_xor(Node, Node a) override { return u.t1(BITWUZLA_KIND_BV_REDXOR, n(a)); }
		BitwuzlaTerm equal(Node, Node a, Node b) override { return u.from_bool(u.t2(BITWUZLA_KIND_EQUAL, n(a), n(b))); }
		BitwuzlaTerm not_equal(Node, Node a, Node b) override { return u.from_bool(u.t2(BITWUZLA_KIND_DISTINCT, n(a), n(b))); }
		BitwuzlaTerm signed_greater_than(Node, Node a, Node b) override { return u.from_bool(u.t2(BITWUZLA_KIND_BV_SGT, n(a), n(b))); }
		BitwuzlaTerm signed_greater_equal(Node, Node a, Node b) override { return u.from_bool(u.t2(BITWUZLA_KIND_BV_SGE, n(a), n(b))); }
		BitwuzlaTerm unsigned_greater_than(Node, Node a, Node b) override { return u.from_bool(u.t2(BITWUZLA_KIND_BV_UGT, n(a), n(b))); }
		BitwuzlaTerm unsigned_greater_equal(Node, Node a, Node b) override { return u.from_bool(u.t2(BITWUZLA_KIND_BV_UGE, n(a), n(b))); }
		BitwuzlaTerm logical_shift_left(Node, Node a, Node b) override { return ext(a, b, BITWUZLA_KIND_BV_SHL); }
		BitwuzlaTerm logical_shift_right(Node, Node a, Node b) override { return ext(a, b, BITWUZLA_KIND_BV_SHR); }
		BitwuzlaTerm arithmetic_shift_right(Node, Node a, Node b) override { return ext(a, b, BITWUZLA_KIND_BV_ASHR); }
		BitwuzlaTerm mux(Node, Node a, Node b, Node s) override { return u.t3(BITWUZLA_KIND_ITE, u.to_bool(n(s)), n(b), n(a)); }
		BitwuzlaTerm constant(Node, RTLIL::Const const &c) override { return u.value(c); }
		BitwuzlaTerm memory_read(Node, Node mem, Node addr) override { return u.t2(BITWUZLA_KIND_ARRAY_SELECT, n(mem), n(addr)); }
		BitwuzlaTerm memory_write(Node, Node mem, Node addr, Node data) override {
			return u.t3(BITWUZLA_KIND_ARRAY_STORE, n(mem), n(addr), n(data));
		}
		BitwuzlaTerm input(Node self, IdString name, IdString kind) override {
			// $anyconst / $allconst hold one value for the whole trace
			if (kind.in(ID($anyconst), ID($allconst))) {
				auto it = u.anyconst_term.find(name);
				if (it != u.anyconst_term.end())
					return it->second;
				return u.anyconst_term[name] = u.fresh(u.sort_of(self.sort()));
			}
			return u.fresh(u.sort_of(self.sort()));
		}
		BitwuzlaTerm state(Node, IdString name, IdString) override { return u.state_term[t].at(name); }
	};

	// build frame t: its state terms are fresh (t = 0) or the previous
	// frame's next values
	void add_frame()
	{
		int t = GetSize(node_term);
		state_term.emplace_back();
		for (auto st : ir.states())
			state_term[t][st->name] = t == 0 ? fresh(sort_of(st->sort))
					: node_term[t-1].at(st->next_value().id());
		node_term.emplace_back();
		Visitor v(*this, t);
		for (auto node : ir) {
			if (GetSize(node_term[t]) <= node.id())
				node_term[t].resize(node.id() + 1, nullptr);
			node_term[t][node.id()] = node.visit(v);
		}
	}
	BitwuzlaTerm output(int t, const Functional::IROutput *o) { return node_term[t].at(o->value().id()); }
};

int32_t terminate_cb(void *state)
{
	auto *deadline = (std::chrono::steady_clock::time_point *)state;
	return std::chrono::steady_clock::now() > *deadline;
}

// solve the checks in `group` with a fresh solver over the shared terms
// of `u` (frames are added on demand), write "<idx> <verdict> <depth>"
// lines; returns the solving time
double solve_group(Unroller &u, BitwuzlaOptions *bo, BitwuzlaTerm init_act, BitwuzlaTerm init,
		const std::vector<int> &out_of_check, const std::vector<int> &group,
		const SmtBitwuzlaOptions &opt, std::chrono::steady_clock::time_point deadline, FILE *out)
{
	int nchecks = GetSize(group);
	BitwuzlaTermManager *tm = u.tm;
	Bitwuzla *bz = bitwuzla_new(tm, bo);
	bitwuzla_set_termination_callback(bz, terminate_cb, &deadline);
	auto asserts = u.ir.outputs(ID($assert));
	auto assumes = u.ir.outputs(ID($assume));
	bitwuzla_assert(bz, bitwuzla_mk_term2(tm, BITWUZLA_KIND_IMPLIES, init_act, init));
	// verdict per check: -1 open, 0 proved, 1 disproved, 2 unknown
	std::vector<int> verdict(nchecks, -1), depth(nchecks, 0);
	std::vector<std::vector<BitwuzlaTerm>> good(nchecks);
	int nopen = nchecks;
	auto t0 = std::chrono::steady_clock::now();
	bool trace = getenv("IC3_SMT_TRACE") != nullptr;
	auto tq = t0;
	auto lap = [&](const char *what, int c, int k, BitwuzlaResult r) {
		if (!trace)
			return;
		auto now = std::chrono::steady_clock::now();
		fprintf(stderr, "smt-trace %s c=%d k=%d r=%d %.3fs\n", what, c, k, (int)r,
				std::chrono::duration<double>(now - tq).count());
		tq = now;
	};
	for (int k = 0; k <= opt.depth_max && nopen > 0; k++) {
		while (GetSize(u.node_term) <= k)
			u.add_frame();
		// constraints of step k: asserted now, so every query below (bad
		// at step <= k) sees exactly the constraints up to its step
		for (auto a : assumes)
			bitwuzla_assert(bz, u.to_bool(u.output(k, a)));
		for (int c = 0; c < nchecks; c++) {
			if (verdict[c] != -1)
				continue;
			BitwuzlaTerm g = u.to_bool(u.output(k, asserts[out_of_check[group[c]]]));
			BitwuzlaTerm bad = u.t1(BITWUZLA_KIND_NOT, g);
			// base: bad at step k from an initial state
			BitwuzlaTerm base[2] = {init_act, bad};
			BitwuzlaResult r = bitwuzla_check_sat_assuming(bz, 2, base);
			lap("base", c, k, r);
			if (r == BITWUZLA_SAT) {
				verdict[c] = 1, depth[c] = k, nopen--;
				good[c].push_back(g);
				continue;
			}
			if (r == BITWUZLA_UNKNOWN) {
				verdict[c] = 2, depth[c] = k, nopen--;
				continue;
			}
			// step: k good steps from any state, then bad at step k
			std::vector<BitwuzlaTerm> step = good[c];
			step.push_back(bad);
			r = bitwuzla_check_sat_assuming(bz, step.size(), step.data());
			lap("step", c, k, r);
			good[c].push_back(g);
			if (r == BITWUZLA_UNSAT) {
				verdict[c] = 0, depth[c] = k, nopen--;
			} else if (r == BITWUZLA_UNKNOWN) {
				verdict[c] = 2, depth[c] = k, nopen--;
			}
		}
	}
	double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	for (int c = 0; c < nchecks; c++)
		fprintf(out, "check %d %d %d\n", group[c], verdict[c] == -1 ? 3 : verdict[c], depth[c]);
	fflush(out);
	bitwuzla_delete(bz);
	return secs;
}

// the child: one solver per check, over one term manager.  A shared
// solver accumulates every queried check's cone over all frames, and each
// SAT answer (a failed induction step) must assign all of it: on caliptra
// fv_montmultiplier (42 checks) shared took 68.7 s / 2.0 GB, per-check
// 15.4 s / 0.42 GB, same verdicts.  The terms are built once: a solver
// bit-blasts only what its asserts and queries reach.
void run_child(Functional::IR &ir, const std::vector<int> &out_of_check, int nchecks,
		const SmtBitwuzlaOptions &opt, FILE *out)
{
	BitwuzlaTermManager *tm = bitwuzla_term_manager_new();
	BitwuzlaOptions *bo = bitwuzla_options_new();
	if (opt.timeout_s > 0)
		bitwuzla_set_option(bo, BITWUZLA_OPT_TIME_LIMIT_PER, (uint64_t)(opt.timeout_s * 1000));
	// experiments: IC3_SMT_OPTS="<long name>=<value>,..."
	if (const char *env = getenv("IC3_SMT_OPTS")) {
		for (auto &kv : split_tokens(env, ",")) {
			size_t eq = kv.find('=');
			if (eq == std::string::npos)
				continue;
			std::string name = kv.substr(0, eq), val = kv.substr(eq + 1);
			for (int o = 0; o < (int)BITWUZLA_OPT_NUM_OPTS; o++) {
				BitwuzlaOptionInfo info;
				bitwuzla_get_option_info(bo, (BitwuzlaOption)o, &info);
				if (name != info.lng)
					continue;
				if (info.is_mode)
					bitwuzla_set_option_mode(bo, (BitwuzlaOption)o, val.c_str());
				else if (info.is_numeric)
					bitwuzla_set_option(bo, (BitwuzlaOption)o, std::stoull(val));
				fprintf(stderr, "smt-opt %s=%s\n", name.c_str(), val.c_str());
			}
		}
	}
	Unroller u(tm, ir, opt.init_zero);
	BitwuzlaTerm init_act = bitwuzla_mk_const(tm, u.bool_sort, "init");
	BitwuzlaTerm init;
	u.add_frame();
	std::string err = u.initial(init);
	if (!err.empty()) {
		fprintf(out, "unsupported %s\n", err.c_str());
		return;
	}
	// -total is one budget across all the solvers
	auto deadline = std::chrono::steady_clock::now() + std::chrono::hours(24 * 365);
	if (opt.total_s > 0)
		deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds((long long)(opt.total_s * 1000));
	double secs = 0;
	for (int c = 0; c < nchecks; c++)
		secs += solve_group(u, bo, init_act, init, out_of_check, {c}, opt, deadline, out);
	fprintf(out, "stats %d solver(s) %.3fs\n", nchecks, secs);
	fflush(out);
}

}

namespace {

// The IR orders whole cells and wires, so a wide bitwise cell or a wire
// whose bits feed each other is a false loop.  Split the bitwise cells on
// a cell-level loop into 1-bit cells (returns how many were split); the
// caller then splits wires at driver boundaries.  splitcells is not
// enough: it splits by output-bit usage, which leaves a chain inside one
// used slice (`assign n = ~{n[2:0], x}`) a loop.
int split_loop_cells(RTLIL::Design *d)
{
	Pass::call(d, "scc -select");
	int n = 0;
	for (auto mod : d->selected_modules()) {
		for (auto cell : mod->selected_cells()) {
			bool unary = cell->type.in(ID($not), ID($pos));
			bool binary = cell->type.in(ID($and), ID($or), ID($xor), ID($xnor));
			bool mux = cell->type == ID($mux);
			if (!unary && !binary && !mux)
				continue;
			SigSpec y = cell->getPort(ID::Y);
			int w = GetSize(y);
			if (w < 2)
				continue;
			SigSpec a = cell->getPort(ID::A), b, sel;
			if (mux) {
				b = cell->getPort(ID::B);
				sel = cell->getPort(ID::S);
			} else {
				bool sgn = cell->getParam(ID::A_SIGNED).as_bool() &&
						(unary || cell->getParam(ID::B_SIGNED).as_bool());
				a.extend_u0(w, sgn);
				if (binary) {
					b = cell->getPort(ID::B);
					b.extend_u0(w, sgn);
				}
			}
			for (int i = 0; i < w; i++) {
				RTLIL::Cell *c = mod->addCell(NEW_ID, cell->type);
				c->attributes = cell->attributes;
				c->setPort(ID::A, a[i]);
				c->setPort(ID::Y, y[i]);
				if (mux) {
					c->setParam(ID::WIDTH, 1);
					c->setPort(ID::B, b[i]);
					c->setPort(ID::S, sel);
					continue;
				}
				c->setParam(ID::A_SIGNED, 0);
				c->setParam(ID::A_WIDTH, 1);
				c->setParam(ID::Y_WIDTH, 1);
				if (binary) {
					c->setParam(ID::B_SIGNED, 0);
					c->setParam(ID::B_WIDTH, 1);
					c->setPort(ID::B, b[i]);
				}
			}
			mod->remove(cell);
			n++;
		}
	}
	Pass::call(d, "select -clear");
	return n;
}

}

std::string smt_bitwuzla_prove(RTLIL::Design *design, RTLIL::Module *module,
		const std::vector<RTLIL::Cell*> &checks, const pool<RTLIL::Cell*> &covers,
		const SmtBitwuzlaOptions &opt, std::vector<SmtBitwuzlaResult> &results, std::string &stats)
{
	results.assign(checks.size(), SmtBitwuzlaResult());

	// a copy of the module, prepared for the Functional IR (as the AIGER
	// export: chformal -lower, undriven nets free, x constants 0), with
	// covers as the inverted asserts the native cover loop proves, every
	// FF on the global clock and memories kept as arrays
	std::unique_ptr<RTLIL::Design> d(new RTLIL::Design);
	RTLIL::Module *m = module->clone();
	d->add(m);
	m->set_bool_attribute(ID::top);
	dict<RTLIL::IdString, int> check_idx;
	for (int i = 0; i < GetSize(checks); i++)
		check_idx[checks[i]->name] = i;
	std::vector<RTLIL::Cell*> drop, cov;
	for (auto c : m->cells()) {
		std::string flavor = c->type == ID($check) && c->parameters.count(ID(FLAVOR))
				? c->getParam(ID(FLAVOR)).decode_string() : "assert";
		bool is_cover = c->type == ID($cover) || (c->type == ID($check) && flavor == "cover");
		bool is_assert = c->type == ID($assert) || (c->type == ID($check) && flavor == "assert");
		if (c->type.in(ID($live), ID($fair), ID($print), ID($scopeinfo)) ||
				(c->type == ID($check) && (flavor == "live" || flavor == "liveness")))
			drop.push_back(c);
		else if ((is_cover || is_assert) && !check_idx.count(c->name))
			drop.push_back(c);	// not asked about
		else if (is_cover)
			cov.push_back(c);
		else if (is_assert)
			c->attributes[ID(ic3_smt_id)] = RTLIL::Const(check_idx.at(c->name));
		else if (!yosys_celltypes.cell_known(c->type)) {
			RTLIL::Module *cm = design->module(c->type);
			if (!cm || cm->get_blackbox_attribute())
				drop.push_back(c);
		}
	}
	for (auto c : drop)
		m->remove(c);
	for (auto c : cov) {
		RTLIL::Wire *na = m->addWire(NEW_ID);
		m->addNot(NEW_ID, c->getPort(ID::A), na);
		RTLIL::Cell *a = m->addAssert(NEW_ID, na, c->getPort(ID::EN));
		a->attributes[ID(ic3_smt_id)] = RTLIL::Const(check_idx.at(c->name));
		m->remove(c);
	}
	(void)covers;

	// fork before anything that can log_error: the IR build and the solver
	log_flush();
	fflush(stdout);
	fflush(stderr);
	int fds[2];
	if (pipe(fds) != 0)
		return "pipe failed";
	pid_t pid = fork();
	if (pid < 0)
		return "fork failed";
	if (pid == 0) {
		close(fds[0]);
		FILE *out = fdopen(fds[1], "w");
		// the child's log lines would interleave with the parent's
		{
			auto quiet = logger().make_debug_scope(true);
			Pass::call(d.get(), "chformal -lower");
			Pass::call(d.get(), "memory_collect");
			Pass::call(d.get(), "memory_nordff");
			Pass::call(d.get(), "dffunmap");
			Pass::call(d.get(), "formalff -clk2ff -anyinit2ff");
			Pass::call(d.get(), "async2sync");
			Pass::call(d.get(), "opt_clean");
			// write ports of a memory with a clocked write: async2sync
			// does not unclock them; memory_map is the fallback
			bool clocked = false;
			for (auto &mem : Mem::get_all_memories(m))
				for (auto &wr : mem.wr_ports)
					clocked = clocked || wr.clk_enable;
			if (clocked) {
				Pass::call(d.get(), "memory_map");
				Pass::call(d.get(), "opt_clean");
				fprintf(out, "note memories bit-blasted (clocked write ports)\n");
			}
			// last: opt_clean would remove the $anyseq drivers of unused
			// undriven bits, which the IR build refuses.  -zero first: x
			// constants are 0, as SatGen reads them; `setundef -undriven
			// -anyseq` alone gives every x constant in a cell port its own
			// free input (tdtype wc2 was disproved: `d9[11:0]` with a
			// 9-bit d9, its x bits differing between two reads)
			Pass::call(d.get(), "setundef -zero");
			Pass::call(d.get(), "setundef -undriven -anyseq");
			// false loops of the IR (OpenTitan csrng aes cipher FSM through
			// prim_buf: cells; sram_ctrl prim_prince data_state: wires).
			// Splitting a wire can expose a new cell-level loop: iterate
			for (int round = 0; round < 8; round++) {
				int split = split_loop_cells(d.get());
				Pass::call(d.get(), "splitnets -driver");
				if (split == 0)
					break;
			}
		}
		if (getenv("IC3_SMT_DUMP"))
			Pass::call(d.get(), stringf("write_rtlil %s", getenv("IC3_SMT_DUMP")));
		std::vector<int> out_of_check(checks.size(), -1);
		Functional::IR ir = Functional::IR::from_module(m);
		auto asserts = ir.outputs(ID($assert));
		for (int i = 0; i < GetSize(asserts); i++) {
			RTLIL::Cell *c = m->cell(asserts[i]->name);
			if (c && c->attributes.count(ID(ic3_smt_id)))
				out_of_check[c->attributes.at(ID(ic3_smt_id)).as_int()] = i;
		}
		for (int i = 0; i < GetSize(checks); i++)
			if (out_of_check[i] < 0) {
				fprintf(out, "unsupported check %s has no IR output\n", log_id(checks[i]));
				fflush(out);
				_exit(0);
			}
		fprintf(out, "ir %d nodes %d states %d inputs\n", GetSize(ir), GetSize(ir.states()), GetSize(ir.all_inputs()));
		run_child(ir, out_of_check, GetSize(checks), opt, out);
		fflush(out);
		_exit(0);
	}
	close(fds[1]);
	std::string text;
	{
		char buf[4096];
		ssize_t n;
		while ((n = read(fds[0], buf, sizeof(buf))) > 0)
			text.append(buf, n);
	}
	close(fds[0]);
	int status = 0;
	waitpid(pid, &status, 0);

	std::string err;
	std::istringstream in(text);
	std::string line;
	bool any = false;
	while (std::getline(in, line)) {
		std::istringstream ls(line);
		std::string tag;
		ls >> tag;
		if (tag == "check") {
			int c, v, dep;
			ls >> c >> v >> dep;
			if (c >= 0 && c < GetSize(results))
				results[c] = SmtBitwuzlaResult{v, dep};
			any = true;
		} else if (tag == "stats" || tag == "ir" || tag == "note")
			stats += (stats.empty() ? "" : "; ") + line;
		else if (tag == "unsupported")
			err = line.substr(12);
	}
	if (!err.empty())
		return err;
	if (!any) {
		if (WIFSIGNALED(status))
			return stringf("engine process died (signal %d)", WTERMSIG(status));
		return stringf("engine process failed (exit %d; the Functional IR refused the design?)",
				WIFEXITED(status) ? WEXITSTATUS(status) : -1);
	}
	return "";
}

YOSYS_NAMESPACE_END

#else

YOSYS_NAMESPACE_BEGIN
std::string smt_bitwuzla_prove(RTLIL::Design *, RTLIL::Module *, const std::vector<RTLIL::Cell*> &,
		const pool<RTLIL::Cell*> &, const SmtBitwuzlaOptions &, std::vector<SmtBitwuzlaResult> &, std::string &)
{
	return "this yosys was built without Bitwuzla";
}
YOSYS_NAMESPACE_END

#endif
