/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2026  sv-yosys contributors
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
//
// cdc -- structural clock-domain crossing analysis
//
// Infers clock domains from FF clock pins, propagates domain tags through
// combinatorial logic to a fixpoint, and classifies FF input crossings:
// two-flop synchronizers (OK), Gray-coded buses (OK), bare bits and buses
// (ERROR), synchronized buses (WARNING), and asynchronous resets from
// foreign domains (WARNING when the deassertion is not synchronized in
// the destination domain, OK when it is).
//
// Clock networks are analyzed per domain: generated clocks (register
// dividers, gated clocks, clock muxes) are traced back to their parent
// domain(s), a clock family (ultimate root) is derived so crossings
// between related domains downgrade to OK same-clock-family, and the
// gating structure is classified: latch-held enables are OK (ICG),
// register-held enables are a runt-pulse WARNING, and asynchronous or
// unheld enables are an ERROR (combinatorial gating glitch).
//
// The design must be process-free: run 'proc' before this pass.

#include "kernel/yosys.h"
#include "kernel/sigtools.h"
#include "kernel/ff.h"
#include "kernel/log_help.h"
#include "passes/cmds/sdc_constraints.h"
#include "libs/json11/json11.hpp"

#include "kernel/satgen.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct CdcFinding {
	std::string severity;
	std::string cls;
	std::string src;
	std::string dst;
	std::string bit;
	std::string cell;
	std::string module;
};

struct Crossing {
	int dst;
	std::set<int> srcs;
	std::string bit;
	SigBit dbit;	// sigmap'd crossing net: the destination-FF D bit
	std::string cell;
	bool synced;
	Cell *ffcell;
	int qidx;
	// synchronized crossings only: the first (metastable) stage's Q also
	// feeds something other than the second stage; the crossing D bit is
	// combinational logic rather than a launch flop's Q
	bool fanout = false;
	bool comb = false;
};

struct CdcAnalyzer
{
	Module *module;
	SigMap sigmap;

	// clock bit -> domain id, with parallel name tables
	dict<SigBit, int> clk_domains;
	std::vector<std::string> domain_names;
	std::vector<SigBit> domain_clks;

	// resolved bit -> domains present on the logic driving it
	std::map<SigBit, std::set<int>> domains;
	// resolved output bit -> (driving cell, index within the output port)
	dict<SigBit, std::pair<Cell *, int>> drivers;
	// resolved input bit -> cells reading it
	dict<SigBit, pool<Cell *>> consumers;

	std::vector<CdcFinding> findings;
	int num_crossing_nets = 0;
	// bounded searches that gave up (their fallback is the conservative
	// answer: not held / not a clock path / not Gray); reported per module
	int bound_hits = 0;

	CdcAnalyzer(Module *module) : module(module), sigmap(module) { }

	static bool is_formal(Cell *cell)
	{
		return cell->type.in(ID($assert), ID($assume), ID($check), ID($cover));
	}

	std::string bit_name(SigBit bit)
	{
		if (!bit.wire)
			return "<const>";
		std::string name = bit.wire->name.unescape();
		if (bit.wire->width == 1)
			return name;
		return stringf("%s[%d]", name.c_str(), bit.offset);
	}

	std::set<int> bit_domains(SigBit bit)
	{
		auto it = domains.find(bit);
		if (it == domains.end())
			return std::set<int>();
		return it->second;
	}

	std::string join_domains(const std::set<int> &doms)
	{
		std::string s;
		for (int d : doms) {
			if (!s.empty())
				s += ",";
			s += d >= 0 && d < GetSize(domain_names) ? domain_names[d] : "?";
		}
		return s;
	}

	int ff_domain(Cell *cell)
	{
		FfData ff(nullptr, cell);
		if (!ff.has_clk || ff.sig_clk.size() != 1)
			return -1;
		SigBit clkbit = sigmap(ff.sig_clk[0]);
		auto it = clk_domains.find(clkbit);
		if (it == clk_domains.end())
			return -1;
		return it->second;
	}

	void build_maps()
	{
		for (auto cell : module->cells()) {
			if (is_formal(cell))
				continue;
			for (auto &conn : cell->connections()) {
				bool input = cell->input(conn.first);
				bool output = cell->output(conn.first);
				if (!input && !output)
					continue;
				SigSpec sig = sigmap(conn.second);
				for (int i = 0; i < GetSize(sig); i++) {
					SigBit bit = sig[i];
					if (!bit.wire)
						continue;
					if (input)
						consumers[bit].insert(cell);
					if (output && !input)
						drivers[bit] = std::make_pair(cell, i);
				}
			}
		}
	}

	void infer_domains()
	{
		for (auto cell : module->cells()) {
			if (!cell->is_builtin_ff())
				continue;
			FfData ff(nullptr, cell);
			if (!ff.has_clk || ff.sig_clk.size() != 1)
				continue;
			SigBit clkbit = sigmap(ff.sig_clk[0]);
			auto it = clk_domains.find(clkbit);
			int dom;
			if (it != clk_domains.end()) {
				dom = it->second;
			} else {
				dom = GetSize(domain_names);
				clk_domains[clkbit] = dom;
				domain_names.push_back(clkbit.wire ? clkbit.wire->name.unescape() : "<const>");
				domain_clks.push_back(clkbit);
			}
			for (auto bit : sigmap(ff.sig_q))
				if (bit.wire)
					domains[bit].insert(dom);
		}
	}

	// Worklist fixpoint: a combinatorial output bit takes the union of the
	// domains of the cell's input bits; repeat until no set grows anymore.
	void propagate()
	{
		std::vector<SigBit> work;
		work.reserve(drivers.size());
		for (auto &it : drivers)
			work.push_back(it.first);

		while (!work.empty()) {
			SigBit bit = work.back();
			work.pop_back();

			auto drit = drivers.find(bit);
			if (drit == drivers.end())
				continue;
			Cell *drv = drit->second.first;
			if (drv->is_builtin_ff())
				continue;

			std::set<int> u;
			for (auto &conn : drv->connections()) {
				if (!drv->input(conn.first))
					continue;
				for (auto inbit : sigmap(conn.second)) {
					if (!inbit.wire)
						continue;
					auto dit = domains.find(inbit);
					if (dit != domains.end())
						u.insert(dit->second.begin(), dit->second.end());
				}
			}

			std::set<int> &cur = domains[bit];
			bool changed = false;
			for (int dom : u) {
				if (!cur.count(dom)) {
					cur.insert(dom);
					changed = true;
				}
			}
			if (!changed)
				continue;

			auto cit = consumers.find(bit);
			if (cit == consumers.end())
				continue;
			for (Cell *cell : cit->second) {
				for (auto &conn : cell->connections()) {
					if (!cell->output(conn.first))
						continue;
					for (auto obit : sigmap(conn.second))
						if (obit.wire)
							work.push_back(obit);
				}
			}
		}
	}

	// ---------------- clock network analysis ----------------
	//
	// infer_domains() keys a domain by the exact clock bit at FF clock
	// pins, so generated clocks (dividers, gated clocks, clock muxes)
	// each became their own unrelated root domain -- a divide-by-2 clock
	// was treated as fully asynchronous to the clock it divides.  This
	// pass walks each domain's clock net back through the driver graph to
	// find the parent domain(s) it derives from, derives a family (root
	// domain) for same-family crossing softening, and classifies the
	// gating structure (glitch hazard vs latch-based ICG vs register
	// divider).

	std::vector<int> domain_parent;	// immediate parent domain, -1 = root
	std::vector<int> domain_family;	// ultimate root domain id

	static bool is_gate_cell(Cell *cell)
	{
		return cell->type.in(ID($and), ID($nand), ID($or), ID($nor),
				ID($xor), ID($xnor), ID($not), ID($mux));
	}

	int family_of(int d)
	{
		if (domain_family.empty())
			return d;
		return domain_family[d];
	}

	// Does the backward cone of `bit` contain a holding element (FF or
	// latch)?  An enable that passes through a register or latch cannot
	// change while the parent clock is active; a bare comb enable can,
	// which is what makes combinatorial clock ANDing glitch-prone.
	bool cone_has_hold(SigBit bit, bool &has_latch, int depth = 0)
	{
		pool<SigBit> seen;
		std::vector<SigBit> work;
		work.push_back(bit);
		seen.insert(bit);
		while (!work.empty()) {
			if (depth >= 64) {
				bound_hits++;
				break;
			}
			SigBit b = work.back();
			work.pop_back();
			depth++;
			auto dit = drivers.find(b);
			if (dit == drivers.end())
				continue;
			Cell *drv = dit->second.first;
			// NB: is_builtin_ff() is TRUE for latches too ($dl/$dlatch
			// share the FF framework), so the latch check must come
			// first
			if (drv->type.in(ID($dl), ID($dlatch))) {
				has_latch = true;
				return true;
			}
			if (drv->is_builtin_ff())
				return true;
			for (auto &conn : drv->connections()) {
				if (!drv->input(conn.first))
					continue;
				for (auto inbit : sigmap(conn.second))
					if (inbit.wire && seen.insert(inbit).second)
						work.push_back(inbit);
			}
		}
		return false;
	}

	// bounded backward search: does the cone of `bit` contain any domain
	// clock bit?  Distinguishes cascaded clock gates (clk & a) & b -- 'a'
	// continues the clock path -- from data enables (clk & f(...)), whose
	// cone must instead be checked for holding elements.
	bool reaches_clock(SigBit bit)
	{
		pool<SigBit> seen;
		std::vector<SigBit> work;
		work.push_back(bit);
		seen.insert(bit);
		int steps = 0;
		while (!work.empty()) {
			if (steps >= 256) {
				bound_hits++;
				break;
			}
			SigBit b = work.back();
			work.pop_back();
			steps++;
			if (clk_domains.count(b))
				return true;
			auto dit = drivers.find(b);
			if (dit == drivers.end())
				continue;
			Cell *drv = dit->second.first;
			if (drv->is_builtin_ff() || drv->type.in(ID($dl), ID($dlatch)))
				continue;	// data register: cone continues, clocks don't
			for (auto &conn : drv->connections()) {
				if (!drv->input(conn.first))
					continue;
				for (auto inbit : sigmap(conn.second))
					if (inbit.wire && seen.insert(inbit).second)
						work.push_back(inbit);
			}
		}
		return false;
	}

	// Walk one domain's clock net backwards.  Domain-clock input bits of
	// path cells are parent candidates; port inputs that look like clocks
	// (name contains "clk"/"clock") are external parents; every other
	// non-constant input is an enable bit whose cone must hold for
	// glitch-free gating.  Returns parents (deduped); fills path_cells,
	// enbits, and ext_parent.
	void analyze_clock(int d, std::set<int> &parents, pool<Cell *> &path_cells,
			pool<SigBit> &enbits, bool &ext_parent, bool &ambiguous)
	{
		auto looks_like_clock = [&](SigBit bit) {
			std::string n = bit.wire->name.unescape();
			std::string lo;
			for (char ch : n)
				lo += tolower((unsigned char)ch);
			return lo.find("clk") != std::string::npos ||
					lo.find("clock") != std::string::npos;
		};
		pool<SigBit> seen;
		std::vector<SigBit> work;
		SigBit root = domain_clks[d];
		work.push_back(root);
		seen.insert(root);
		while (!work.empty()) {
			SigBit bit = work.back();
			work.pop_back();
			auto dit = drivers.find(bit);
			if (dit == drivers.end())
				continue;	// port input or undriven: a root
			Cell *drv = dit->second.first;
			if (drv->is_builtin_ff() || drv->type.in(ID($dl), ID($dlatch))) {
				// register/latch-generated clock (divider, gated-FF):
				// the clock parent is the register's own clock domain.
				if (drv->is_builtin_ff()) {
					int p = ff_domain(drv);
					if (p >= 0 && p != d)
						parents.insert(p);
				}
				continue;
			}
			if (!is_gate_cell(drv))
				continue;	// unrelated logic driving the net: stop
			path_cells.insert(drv);
			int ext_cands = 0;
			for (auto &conn : drv->connections()) {
				if (!drv->input(conn.first))
					continue;
				for (auto inbit : sigmap(conn.second)) {
					if (!inbit.wire)
						continue;
					auto cit = clk_domains.find(inbit);
					if (cit != clk_domains.end()) {
						if (cit->second != d)
							parents.insert(cit->second);
						continue;
					}
					if (inbit.wire->port_input && looks_like_clock(inbit)) {
						// a top-level clock port this gate derives from
						// (by NAME: two such operands of one gate cannot
						// be told apart, which the caller reports)
						ext_parent = true;
						if (++ext_cands > 1)
							ambiguous = true;
						continue;
					}
					if (!seen.count(inbit) && reaches_clock(inbit))
						work.push_back(inbit);
					else
						enbits.insert(inbit);
				}
			}
		}
	}

	void analyze_clock_network()
	{
		int ndom = GetSize(domain_names);
		domain_parent.assign(ndom, -1);

		// ---- pass 1: parents and gate classification ------------------
		std::vector<std::string> gate_cls(ndom), gate_sev(ndom), gate_cell(ndom, "");
		for (int d = 0; d < ndom; d++) {
			std::set<int> parents;
			pool<Cell *> path_cells;
			pool<SigBit> enbits;
			bool ext_parent = false, ambiguous = false;
			analyze_clock(d, parents, path_cells, enbits, ext_parent, ambiguous);
			if (parents.empty() && !ext_parent)
				continue;	// a genuine root (port/undriven clock)
			// (c) a clock mux (several parents) belongs to no parent's
			// family: it is asynchronous to every one of them
			if (GetSize(parents) == 1)
				domain_parent[d] = *parents.begin();
			std::string parent_names;
			for (int p : parents) {
				if (!parent_names.empty())
					parent_names += ",";
				parent_names += domain_names[p];
			}
			if (ext_parent) {
				if (!parent_names.empty())
					parent_names += ",";
				parent_names += "(external)";
			}
			std::string clkname = bit_name(domain_clks[d]);
			std::string cellname;
			if (!path_cells.empty()) {
				std::vector<Cell *> sorted(path_cells.begin(), path_cells.end());
				std::sort(sorted.begin(), sorted.end(), [](Cell *a, Cell *b) { return a->name < b->name; });
				cellname = sorted.front()->name.unescape();
			} else {
				// register-generated: name the generating register
				auto dit = drivers.find(domain_clks[d]);
				cellname = dit != drivers.end() ? dit->second.first->name.unescape() : clkname;
			}
			if (parents.size() > 1) {
				// clock mux: asynchronous to every parent; the select
				// line is checked like any other enable below
				gate_sev[d] = "WARNING";
				gate_cls[d] = "clock-mux";
			}
			if (enbits.empty()) {
				if (gate_cls[d].empty()) {
					gate_sev[d] = "OK";
					gate_cls[d] = "generated-clock";
				}
			} else {
				bool any_glitch = false, any_reg = false, all_latch = true;
				for (SigBit en : enbits) {
					bool has_latch = false;
					bool hold = cone_has_hold(en, has_latch);
					// enable not generated in (one of) the parent
					// domain(s): it can change asynchronously w.r.t.
					// the gate it feeds -> glitch, held or not
					bool foreign = false;
					for (int ed : bit_domains(en))
						if (!parents.count(ed))
							foreign = true;
					if (foreign || !hold)
						any_glitch = true;
					else if (!has_latch)
						any_reg = true;
					all_latch = all_latch && has_latch;
				}
				if (any_glitch) {
					gate_sev[d] = "ERROR";
					gate_cls[d] = "clock-gating-glitch";
				} else if (any_reg) {
					gate_sev[d] = "WARNING";
					gate_cls[d] = "clock-gate-reg";
				} else if (all_latch) {
					gate_sev[d] = "OK";
					gate_cls[d] = "clock-gate-icg";
				}
			}
			if (ambiguous && gate_sev[d] != "ERROR") {
				gate_sev[d] = "WARNING";
				gate_cls[d] = "clock-gating-ambiguous";
			}
			if (!gate_cls[d].empty())
				findings.push_back({gate_sev[d], gate_cls[d], parent_names,
						domain_names[d], clkname, cellname});
		}

		// ---- pass 2: family = ultimate root (multi-parent domains are
		// their own family: a clock mux is asynchronous to everything)
		domain_family.assign(ndom, -1);
		for (int d = 0; d < ndom; d++) {
			std::vector<int> path;
			int cur = d;
			while (domain_parent[cur] >= 0 && domain_family[cur] < 0) {
				path.push_back(cur);
				cur = domain_parent[cur];
				if (GetSize(path) > ndom)
					break;	// parent cycle: treat cur as a root
			}
			int root = domain_family[cur] >= 0 ? domain_family[cur] : cur;
			for (int x : path)
				domain_family[x] = root;
			domain_family[d] = root;
		}
	}

	// ---------------- reset deassert classification ----------------
	//
	// An async reset ASSERTED from a foreign domain is safe (that is what
	// async-reset flops are for); the hazard is DEASSERTION skew and
	// recovery-time violations, which is why resets are synchronized in
	// the destination domain.  v1 warned for every foreign/external
	// async reset; here we recognize the canonical pattern: a chain of
	// >=2 same-domain single-bit FFs, all async-reset by one raw reset R,
	// head D tied to 1 (assert value released one clock after deassert),
	// each later stage D = previous stage Q.  Chain members are reported
	// OK reset-sync-stage; flops whose reset net is a chain tail's Q are
	// OK reset-sync-deassert; everything else keeps the v1 WARNING.

	// canonical raw reset behind an async-reset net: walk back through
	// inverters ($not / $xor-1) only; stops at FF Q, port, or logic.
	SigBit arst_base(SigBit bit)
	{
		pool<SigBit> seen;
		while (seen.insert(bit).second) {
			auto dit = drivers.find(bit);
			if (dit == drivers.end())
				break;
			Cell *drv = dit->second.first;
			int yidx = dit->second.second;
			SigBit nxt;
			if (drv->type == ID($not)) {
				SigSpec a = sigmap(drv->getPort(ID::A));
				if (yidx < 0 || yidx >= GetSize(a))
					break;
				nxt = a[yidx];
			} else if (drv->type == ID($xor)) {
				SigSpec a = sigmap(drv->getPort(ID::A));
				SigSpec b = sigmap(drv->getPort(ID::B));
				if (yidx < 0 || yidx >= GetSize(a) || yidx >= GetSize(b))
					break;
				if (a[yidx].wire == nullptr && a[yidx].data == RTLIL::S1)
					nxt = b[yidx];
				else if (b[yidx].wire == nullptr && b[yidx].data == RTLIL::S1)
					nxt = a[yidx];
				else
					break;
			} else
				break;
			bit = sigmap(nxt);
		}
		return bit;
	}

	// lazily-computed per-(domain, base-reset) synchronizer table:
	// stage_members = cells belonging to a >=2 chain; tail_qs = Q bits of
	// chain tails (the nets it is safe to fan a reset tree out from)
	dict<std::pair<int, SigBit>, std::pair<pool<Cell *>, pool<SigBit>>> reset_chains;
	bool reset_chains_built = false;

	// D tied to one constant 0/1 bit: returns it, else Sx
	RTLIL::State d_const(SigSpec sig)
	{
		sig = sigmap(sig);
		if (GetSize(sig) != 1 || sig[0].wire || (sig[0].data != RTLIL::S0 && sig[0].data != RTLIL::S1))
			return RTLIL::Sx;
		return sig[0].data;
	}

	void build_reset_chains()
	{
		reset_chains_built = true;
		// index single-bit async-reset FFs by (domain, base reset)
		dict<std::pair<int, SigBit>, pool<Cell *>> by_base;
		for (auto cell : module->cells()) {
			if (!cell->is_builtin_ff())
				continue;
			FfData ff(nullptr, cell);
			if (!ff.has_clk || ff.width != 1 || !ff.has_arst || GetSize(ff.sig_arst) != 1)
				continue;
			int d = ff_domain(cell);
			if (d < 0)
				continue;
			by_base[std::make_pair(d, arst_base(sigmap(ff.sig_arst[0])))].insert(cell);
		}
		for (auto &it : by_base) {
			pool<Cell *> &stages = it.second;
			if (stages.size() < 2)
				continue;
			// link each stage to the stage sampling its Q
			dict<Cell *, Cell *> nxt;
			pool<Cell *> heads;
			for (Cell *c : stages) {
				FfData cf(nullptr, c);
				SigBit q = sigmap(cf.sig_q[0]);
				RTLIL::State dc = d_const(cf.sig_d);
				if (dc != RTLIL::Sx && GetSize(cf.val_arst) == 1 &&
						cf.val_arst[0] == (dc == RTLIL::S1 ? RTLIL::S0 : RTLIL::S1))
					heads.insert(c);
				for (Cell *o : stages) {
					if (o == c)
						continue;
					FfData of(nullptr, o);
					if (sigmap(of.sig_d[0]) == q) {
						nxt[c] = o;
						break;
					}
				}
			}
			// walk head chains; a chain of >=2 stages is a synchronizer
			for (Cell *h : heads) {
				std::vector<Cell *> chain;
				pool<Cell *> seen;
				Cell *cur = h;
				FfData hf(nullptr, h);
				while (cur && seen.insert(cur).second) {
					FfData sf(nullptr, cur);
					if (GetSize(sf.val_arst) != 1 || sf.val_arst[0] != hf.val_arst[0])
						break;	// a stage resetting to the other value
					chain.push_back(cur);
					auto nit = nxt.find(cur);
					cur = nit != nxt.end() ? nit->second : nullptr;
				}
				if (GetSize(chain) < 2)
					continue;
				auto &slot = reset_chains[it.first];
				for (Cell *c : chain)
					slot.first.insert(c);
				FfData tf(nullptr, chain.back());
				slot.second.insert(sigmap(tf.sig_q[0]));
			}
		}
	}

	// classify one async-reset net on a domain-d FF:
	//   "reset-sync-stage"     the FF itself belongs to a >=2 chain
	//   "reset-sync-deassert"  its reset net is driven by a chain tail Q
	//   ""                     no synchronizer found (caller warns)
	std::string reset_class(int d_ff, Cell *cell, SigBit arst)
	{
		if (!reset_chains_built)
			build_reset_chains();
		SigBit base = arst_base(arst);
		auto it = reset_chains.find(std::make_pair(d_ff, base));
		if (it != reset_chains.end() && it->second.first.count(cell))
			return "reset-sync-stage";
		for (auto &rc : reset_chains)
			if (rc.first.first == d_ff && rc.second.second.count(base))
				return "reset-sync-deassert";
		return "";
	}

	// ---------------- SDC pairing (read_sdc scratchpad data) ----------------
	//
	// When read_sdc has stored constraints on the design scratchpad, clock
	// targets map SDC clock names onto this module's domain names (domains
	// are named after their clock port), and set_clock_groups
	// -asynchronous / set_false_path clock pairs become an async-pair
	// set.  An unsynchronized crossing whose every (src clock, dst clock)
	// combination is declared asynchronous pairs with the declaration:
	// downgraded from ERROR to WARNING async-declared (the designer
	// waived it -- but a synchronizer is still recommended, so it stays
	// visible).  Synchronized crossings are unaffected.

	std::map<std::string, std::vector<std::string>> sdc_port_clocks;
	std::set<std::pair<std::string, std::string>> sdc_async_pairs;
	bool sdc_active = false;

	static bool sdc_glob(const std::string &pat, const std::string &s)
	{
		// * and ? only, iterative DP
		std::vector<char> prev(s.size() + 1), cur(s.size() + 1);
		prev[0] = 1;
		for (size_t i = 0; i <= pat.size(); i++) {
			char pc = i ? pat[i - 1] : 0;
			if (i) {
				cur.assign(s.size() + 1, 0);
				for (size_t j = 0; j <= s.size(); j++) {
					if (!prev[j])
						continue;
					if (pc == '*') {
						for (size_t k = j; k <= s.size(); k++)
							cur[k] = 1;
					} else if (j < s.size() && (pc == '?' || pc == s[j])) {
						cur[j + 1] = 1;
					}
				}
				prev.swap(cur);
			}
		}
		return prev[s.size()];
	}

	std::vector<std::string> sdc_match_domains(const SdcTarget &t)
	{
		std::vector<std::string> hits;
		if (t.kind != "port" && t.kind != "pin")
			return hits;
		for (auto &name : domain_names) {
			if (t.wildcard ? sdc_glob(t.pattern, name)
					: t.pattern == name)
				hits.push_back(name);
		}
		return hits;
	}

	void load_sdc(Design *design)
	{
		SdcConstraints c;
		if (!sdc_load_constraints(design, c))
			return;
		// clock name -> patterns from clock objects
		std::map<std::string, SdcClock *> by_name;
		for (auto &k : c.clocks)
			by_name[k.name] = &k;
		std::set<std::string> clock_names;
		for (auto &k : c.clocks)
			clock_names.insert(k.name);
		auto match_clock_names = [&](const std::vector<SdcTarget> &targets) {
			std::vector<std::string> out;
			for (auto &t : targets) {
				if (t.kind != "clock")
					continue;
				for (auto &cn : clock_names)
					if (t.wildcard ? sdc_glob(t.pattern, cn) : t.pattern == cn)
						out.push_back(cn);
			}
			return out;
		};
		for (auto &k : c.clocks)
			for (auto &t : k.targets)
				for (auto &dom : sdc_match_domains(t))
					sdc_port_clocks[dom].push_back(k.name);
		auto add_pairs = [&](const std::vector<std::string> &a,
				const std::vector<std::string> &b) {
			for (auto &x : a)
				for (auto &y : b) {
					sdc_async_pairs.insert(std::make_pair(x, y));
					sdc_async_pairs.insert(std::make_pair(y, x));
				}
		};
		for (auto &g : c.clock_groups) {
			if (!g.asynchronous)
				continue;
			std::vector<std::vector<std::string>> names;
			for (auto &grp : g.groups)
				names.push_back(match_clock_names(grp));
			for (size_t i = 0; i < names.size(); i++)
				for (size_t j = i + 1; j < names.size(); j++) {
					add_pairs(names[i], names[j]);
					add_pairs(names[j], names[i]);
				}
		}
		for (auto &p : c.path_exceptions) {
			if (p.kind != "false_path")
				continue;
			// a false path narrowed by -through waives only some paths
			// between the two clocks: it never waives the clock pair
			if (!p.through.empty()) {
				log("cdc: %s: set_false_path with -through not used for clock pairing\n", log_id(module->name));
				continue;
			}
			add_pairs(match_clock_names(p.from), match_clock_names(p.to));
		}
		sdc_active = !sdc_port_clocks.empty() || !sdc_async_pairs.empty();
		if (sdc_active)
			log("cdc: %s: SDC pairing active (%d clocks mapped, %d async pairs)\n",
					log_id(module->name), GetSize(sdc_port_clocks),
					GetSize(sdc_async_pairs));
	}

	// every (src clock, dst clock) combination declared async?
	bool async_declared(const std::set<int> &srcs, int dst)
	{
		if (!sdc_active)
			return false;
		auto dit = sdc_port_clocks.find(domain_names[dst]);
		if (dit == sdc_port_clocks.end() || dit->second.empty())
			return false;
		for (int s : srcs) {
			auto sit = sdc_port_clocks.find(domain_names[s]);
			if (sit == sdc_port_clocks.end() || sit->second.empty())
				return false;
			for (auto &sn : sit->second)
				for (auto &dn : dit->second)
					if (!sdc_async_pairs.count(std::make_pair(sn, dn)))
						return false;
		}
		return true;
	}

	// Follow the synchronizer chain forward from the crossing FF: collect
	// consecutive flip-flops of domain d_ff, each Q bit feeding the next D
	// bit. Returns the chain (crossing FF first, synchronizer tail last);
	// 2+ entries mean the crossing is synchronized.
	std::vector<std::pair<Cell *, int>> sync_chain(Cell *cell, int qidx, int d_ff)
	{
		std::set<std::pair<Cell *, int>> visited;
		std::vector<std::pair<Cell *, int>> chain;
		visited.insert(std::make_pair(cell, qidx));
		chain.push_back(std::make_pair(cell, qidx));
		Cell *cur_cell = cell;
		int cur_idx = qidx;

		for (int hop = 0; hop < 8; hop++) {
			FfData cur_ff(nullptr, cur_cell);
			if (cur_idx < 0 || cur_idx >= GetSize(cur_ff.sig_q))
				break;
			SigBit cur = sigmap(cur_ff.sig_q[cur_idx]);

			auto cit = consumers.find(cur);
			if (cit == consumers.end())
				break;

			std::vector<std::pair<Cell *, int>> cands;
			for (Cell *c : cit->second) {
				if (!c->is_builtin_ff())
					continue;
				if (ff_domain(c) != d_ff)
					continue;
				FfData g(nullptr, c);
				SigSpec gd = sigmap(g.sig_d);
				for (int j = 0; j < GetSize(gd); j++) {
					if (gd[j] != cur)
						continue;
					if (visited.count(std::make_pair(c, j)))
						continue;
					bool pure = true;
					for (int d : bit_domains(gd[j]))
						if (d != d_ff) {
							pure = false;
							break;
						}
					if (!pure)
						continue;
					cands.push_back(std::make_pair(c, j));
				}
			}
			if (cands.empty())
				break;
			std::sort(cands.begin(), cands.end(), [](const std::pair<Cell *, int> &a, const std::pair<Cell *, int> &b) {
				if (a.first->name != b.first->name)
					return a.first->name < b.first->name;
				return a.second < b.second;
			});

			visited.insert(cands.front());
			chain.push_back(cands.front());
			cur_cell = cands.front().first;
			cur_idx = cands.front().second;
		}
		return chain;
	}

	bool is_synchronized(Cell *cell, int qidx, int d_ff)
	{
		return GetSize(sync_chain(cell, qidx, d_ff)) >= 2;
	}

	// Forward cone of a sync FF's Q bit: every comb cell traversed (into
	// `comb`) and every FF reached as a sink (any clock domain -- a sink
	// ends the walk), at (cell, bit index) granularity: a 4-bit vector
	// synchronizer's bits all terminate at the same next-stage FF cell,
	// and cell-level sink sharing would falsely call them reconvergent.
	// Two synced crossings of one group whose cones share a comb cell or
	// a sink (cell,idx) pair reconverge in the destination domain; a
	// crossing whose cone shares nothing with its group siblings is an
	// independent synchronizer even when the group carries a bus.
	void q_cone(Cell *ff, int qidx, pool<Cell *> &comb, pool<std::pair<Cell *, int>> &sinks)
	{
		FfData ffdata(nullptr, ff);
		if (qidx < 0 || qidx >= GetSize(ffdata.sig_q))
			return;
		pool<SigBit> seen;
		std::vector<SigBit> work;
		SigBit start = sigmap(ffdata.sig_q[qidx]);
		seen.insert(start);
		work.push_back(start);
		while (!work.empty()) {
			SigBit bit = work.back();
			work.pop_back();
			auto cit = consumers.find(bit);
			if (cit == consumers.end())
				continue;
			for (Cell *c : cit->second) {
				if (c->is_builtin_ff()) {
					int didx = -1;
					FfData cf(nullptr, c);
					SigSpec d = sigmap(cf.sig_d);
					for (int j = 0; j < GetSize(d); j++)
						if (d[j] == bit) {
							didx = j;
							break;
						}
					sinks.insert(std::make_pair(c, didx));
					continue;
				}
				if (!comb.insert(c).second)
					continue;
				for (auto &conn : c->connections())
					if (c->output(conn.first))
						for (auto obit : sigmap(conn.second))
							if (obit.wire && seen.insert(obit).second)
								work.push_back(obit);
			}
		}
	}

	// Source side of a Gray-coded bus: every seed bit must be produced by a
	// $xor encoder of the shape gray[i] = w[i] ^ w[i+1] over one common
	// source vector w (matching bin -> bin ^ (bin >> 1)), with the bits
	// spanning a contiguous, monotonic run of w and only the last bit's
	// neighbor constant (gray[MSB] = w[MSB] ^ 0).  The encoder is located
	// on the seed bit itself; a registered encoder output is hopped across
	// before checking.  Seeds are the crossing D bits (ordered by (ffcell,
	// qidx)) for the crossing case and the $eq operand bits for the
	// EQ-compare case.  When `dom` >= 0, every flip-flop hopped through on
	// the way back to the encoder $xor must belong to clock domain `dom`,
	// so a foreign/source-domain vector is never accepted as local Gray.
	bool gray_encoder(const std::vector<SigBit> &dbits, int dom, std::vector<SigBit> *wbits_out = nullptr)
	{
		int n = GetSize(dbits);
		std::vector<std::vector<std::pair<SigBit, SigBit>>> opts(n);
		for (int k = 0; k < n; k++) {
			SigBit dbit = dbits[k];
			Cell *x = nullptr;
			for (int hop = 0; hop < 4; hop++) {
				auto dit = drivers.find(dbit);
				if (dit == drivers.end())
					return false;
				x = dit->second.first;
				if (!x->is_builtin_ff())
					break;
				if (dom >= 0 && ff_domain(x) != dom)
					return false;
				FfData ff(nullptr, x);
				SigSpec q = sigmap(ff.sig_q), d = sigmap(ff.sig_d);
				int idx = -1;
				for (int i = 0; i < GetSize(q); i++)
					if (q[i] == dbit) {
						idx = i;
						break;
					}
				if (idx < 0 || idx >= GetSize(d))
					return false;
				dbit = sigmap(d[idx]);
			}
			if (x == nullptr || x->type != ID($xor))
				return false;
			SigSpec y = sigmap(x->getPort(ID::Y));
			SigSpec a = sigmap(x->getPort(ID::A));
			SigSpec b = sigmap(x->getPort(ID::B));
			int yidx = -1;
			for (int i = 0; i < GetSize(y); i++)
				if (y[i] == dbit) {
					yidx = i;
					break;
				}
			if (yidx < 0 || yidx >= GetSize(a) || yidx >= GetSize(b))
				return false;
			SigBit in[2] = {a[yidx], b[yidx]};
			for (int swap = 0; swap < 2; swap++) {
				SigBit cur = in[swap], nxt = in[swap ^ 1];
				if (cur.wire == nullptr)
					continue;
				if (nxt.wire != nullptr) {
					if (nxt.wire != cur.wire)
						continue;
					int dn = nxt.offset - cur.offset;
					if (dn != 1 && dn != -1)
						continue;
				} else if (k != n - 1) {
					continue;
				}
				opts[k].push_back(std::make_pair(cur, nxt));
			}
			if (opts[k].empty())
				return false;
		}
		// Pick one orientation per bit so adjacent bits share the w
		// neighbor: gray[i+1]'s w bit must be gray[i]'s nxt bit, with the
		// offsets stepping one way through a single wire.
		long nodes = 0;
		std::vector<SigBit> chosen(n);
		std::function<bool(int, Wire *, int, int, SigBit)> dfs = [&](int k, Wire *w, int prev_off, int step, SigBit prev_nxt) {
			if (++nodes > 100000) {
				bound_hits++;
				return false;
			}
			if (k == n)
				return true;
			for (auto &opt : opts[k]) {
				SigBit cur = opt.first, nxt = opt.second;
				if (w != nullptr && cur.wire != w)
					continue;
				if (k > 0) {
					if (cur != prev_nxt)
						continue;
					int d = cur.offset - prev_off;
					if (d != 1 && d != -1)
						continue;
					if (step != 0 && d != step)
						continue;
					if (step == 0)
						step = d;
				}
				chosen[k] = cur;
				if (dfs(k + 1, w != nullptr ? w : cur.wire, cur.offset, step, nxt))
					return true;
			}
			return false;
		};
		if (!dfs(0, nullptr, 0, 0, SigBit()))
			return false;
		if (wbits_out)
			*wbits_out = chosen;
		return true;
	}

	// Gray step check: a Gray-coded value changes at most one bit per
	// source clock only if the binary vector it encodes steps by +1, -1 or
	// holds.  The vector must be register Q bits; SAT proves D - Q in
	// {0, +1, -1} (mod 2^n) over the D cone for EVERY current state and
	// input (a sufficient check: a counter that loads arbitrary values, or
	// steps by 3, fails it).  Unsupported cells or a non-register vector:
	// unverified.
	bool gray_step_ok(std::vector<SigBit> wbits)
	{
		int n = GetSize(wbits);
		if (n < 1)
			return false;
		std::sort(wbits.begin(), wbits.end(), [](const SigBit &a, const SigBit &b) {
			if (a.wire != b.wire)
				return a.wire->name < b.wire->name;
			return a.offset < b.offset;
		});
		for (int i = 1; i < n; i++)
			if (wbits[i].wire != wbits[0].wire || wbits[i].offset != wbits[i - 1].offset + 1)
				return false;
		SigSpec q, dsig;
		for (auto b : wbits) {
			auto dit = drivers.find(sigmap(b));
			if (dit == drivers.end() || !dit->second.first->is_builtin_ff())
				return false;
			FfData ff(nullptr, dit->second.first);
			int idx = dit->second.second;
			if (!ff.has_clk || idx < 0 || idx >= GetSize(ff.sig_d))
				return false;
			q.append(sigmap(b));
			dsig.append(sigmap(ff.sig_d[idx]));
		}
		// backward comb cone of the D bits, stopping at FF Q / ports
		pool<Cell *> cone;
		std::vector<SigBit> work;
		pool<SigBit> seen;
		for (auto b : dsig)
			if (b.wire && seen.insert(b).second)
				work.push_back(b);
		while (!work.empty()) {
			SigBit b = work.back();
			work.pop_back();
			auto dit = drivers.find(b);
			if (dit == drivers.end())
				continue;
			Cell *drv = dit->second.first;
			if (drv->is_builtin_ff() || drv->type.in(ID($dl), ID($dlatch)))
				continue;
			if (!cone.insert(drv).second)
				continue;
			if (GetSize(cone) > 2000) {
				bound_hits++;
				return false;
			}
			for (auto &conn : drv->connections()) {
				if (!drv->input(conn.first))
					continue;
				for (auto ib : sigmap(conn.second))
					if (ib.wire && seen.insert(ib).second)
						work.push_back(ib);
			}
		}
		ezSatPtr ez;
		SatGen satgen(ez.get(), &sigmap);
		for (Cell *c : cone)
			if (!satgen.importCell(c))
				return false;
		std::vector<int> qv = satgen.importSigSpec(q), dv = satgen.importSigSpec(dsig);
		std::vector<int> one = ez->vec_const_unsigned(1, n);
		int same = ez->vec_eq(dv, qv);
		int up = ez->vec_eq(dv, ez->vec_add(qv, one));
		int down = ez->vec_eq(dv, ez->vec_sub(qv, one));
		return !ez->solve(ez->NOT(ez->OR(same, ez->OR(up, down))));
	}

	bool gray_encoder(const std::vector<Crossing> &comp)
	{
		std::vector<SigBit> dbits;
		for (auto &c : comp)
			dbits.push_back(c.dbit);
		return gray_encoder(dbits, -1);
	}

	// Destination side of a Gray-coded bus: the synchronizer tail Q bits
	// must feed a prefix-XOR decoder chain -- n synced bits reconverge in
	// exactly n-1 shared $xor cells, the chain head taking two fresh
	// synced Q bits and every further cell the running prefix (the previous
	// cell's Y) plus one fresh synced Q bit, consuming all n synced Q bits
	// exactly once.  A parity tree also has n-1 cells but its inner nodes
	// take two prefixes, and an $eq compare is not $xor at all, so neither
	// upgrades.  The chain is searched in the union of the bits' forward
	// cones (a prefix chain's head is unreachable from the low bits, so a
	// plain cone intersection can never contain it).
	bool gray_decoder(const std::vector<Crossing> &comp)
	{
		int n = GetSize(comp);
		std::vector<SigBit> qbits(n);
		pool<Cell *> xorpool;
		for (int k = 0; k < n; k++) {
			auto chain = sync_chain(comp[k].ffcell, comp[k].qidx, comp[k].dst);
			if (GetSize(chain) < 2)
				return false;
			Cell *tail = chain.back().first;
			FfData ff(nullptr, tail);
			if (chain.back().second < 0 || chain.back().second >= GetSize(ff.sig_q))
				return false;
			qbits[k] = sigmap(ff.sig_q[chain.back().second]);
			pool<Cell *> comb;
			pool<std::pair<Cell *, int>> sinks;
			q_cone(tail, chain.back().second, comb, sinks);
			for (Cell *c : comb)
				if (c->type == ID($xor))
					xorpool.insert(c);
		}
		if (xorpool.empty())
			return false;
		dict<SigBit, int> qidx;
		for (int k = 0; k < n; k++)
			qidx[qbits[k]] = k;
		struct Cand {
			SigBit y, p, q;
		};
		std::vector<Cand> cands;
		for (Cell *c : xorpool) {
			SigSpec y = sigmap(c->getPort(ID::Y));
			SigSpec a = sigmap(c->getPort(ID::A));
			SigSpec b = sigmap(c->getPort(ID::B));
			for (int i = 0; i < GetSize(y) && i < GetSize(a) && i < GetSize(b); i++) {
				if (a[i] == b[i])
					continue;
				cands.push_back({y[i], a[i], b[i]});
			}
		}
		long nodes = 0;
		std::function<bool(SigBit, std::vector<char> &, int)> dfs = [&](SigBit prefy, std::vector<char> &used, int depth) {
			if (++nodes > 100000) {
				bound_hits++;
				return false;
			}
			if (depth == n - 1)
				return true;
			for (auto &cd : cands) {
				int qi = -1;
				if (cd.p == prefy && qidx.count(cd.q))
					qi = qidx[cd.q];
				else if (cd.q == prefy && qidx.count(cd.p))
					qi = qidx[cd.p];
				else
					continue;
				if (used[qi])
					continue;
				used[qi] = 1;
				if (dfs(cd.y, used, depth + 1))
					return true;
				used[qi] = 0;
			}
			return false;
		};
		for (auto &cd : cands) {
			if (!qidx.count(cd.p) || !qidx.count(cd.q))
				continue;
			std::vector<char> used(n, 0);
			used[qidx[cd.p]] = 1;
			used[qidx[cd.q]] = 1;
			if (dfs(cd.y, used, 1))
				return true;
		}
		return false;
	}

	// EQ-compare Gray recognition (async FIFO full/empty style): the synced
	// tail Q bits of the component reconverge at a $eq cell against a local
	// Gray-coded vector.  One operand, bit by bit, must resolve through a
	// bounded chain of single-bit inverters ($not, or $xor with a constant
	// 1 -- so per-bit polarity such as FIFO full-flag inversion is allowed)
	// to exactly the set of synced tail Q bits, each used once; the other
	// operand must pass the Gray-encoder shape test with every hopped
	// flip-flop in the component's destination domain.  A binary (non-Gray)
	// local vector fails the shape test, and a foreign/source-domain vector
	// fails the domain check, so neither upgrades.
	bool eq_gray_compare(const std::vector<Crossing> &comp)
	{
		int n = GetSize(comp);
		std::vector<SigBit> qbits(n);
		pool<Cell *> eqpool;
		for (int k = 0; k < n; k++) {
			auto chain = sync_chain(comp[k].ffcell, comp[k].qidx, comp[k].dst);
			if (GetSize(chain) < 2)
				return false;
			Cell *tail = chain.back().first;
			FfData ff(nullptr, tail);
			if (chain.back().second < 0 || chain.back().second >= GetSize(ff.sig_q))
				return false;
			qbits[k] = sigmap(ff.sig_q[chain.back().second]);
			pool<Cell *> comb;
			pool<std::pair<Cell *, int>> sinks;
			q_cone(tail, chain.back().second, comb, sinks);
			for (Cell *c : comb)
				if (c->type == ID($eq))
					eqpool.insert(c);
		}
		if (eqpool.empty())
			return false;
		dict<SigBit, int> qidx;
		for (int k = 0; k < n; k++)
			qidx[qbits[k]] = k;

		// Resolve one synced-side operand bit back through up to 4 hops of
		// single-bit inverters to a synced tail Q bit; returns its component
		// index or -1.  A direct (uninverted) tail Q bit resolves in 0 hops.
		auto resolve_q = [&](SigBit bit) -> int {
			for (int hop = 0; hop < 4; hop++) {
				auto qit = qidx.find(bit);
				if (qit != qidx.end())
					return qit->second;
				auto dit = drivers.find(bit);
				if (dit == drivers.end())
					return -1;
				Cell *x = dit->second.first;
				int yidx = dit->second.second;
				SigBit nxt;
				if (x->type == ID($not)) {
					SigSpec a = sigmap(x->getPort(ID::A));
					if (yidx < 0 || yidx >= GetSize(a))
						return -1;
					nxt = a[yidx];
				} else if (x->type == ID($xor)) {
					SigSpec a = sigmap(x->getPort(ID::A));
					SigSpec b = sigmap(x->getPort(ID::B));
					if (yidx < 0 || yidx >= GetSize(a) || yidx >= GetSize(b))
						return -1;
					if (a[yidx].wire == nullptr && a[yidx].data == RTLIL::S1)
						nxt = b[yidx];
					else if (b[yidx].wire == nullptr && b[yidx].data == RTLIL::S1)
						nxt = a[yidx];
					else
						return -1;
				} else
					return -1;
				bit = sigmap(nxt);
			}
			return qidx.count(bit) ? qidx[bit] : -1;
		};

		for (Cell *eq : eqpool) {
			SigSpec a = sigmap(eq->getPort(ID::A));
			SigSpec b = sigmap(eq->getPort(ID::B));
			if (GetSize(a) != n || GetSize(b) != n)
				continue;
			for (int side = 0; side < 2; side++) {
				SigSpec synced = side ? b : a;
				SigSpec local = side ? a : b;
				std::vector<char> used(n, 0);
				bool sok = true;
				for (int i = 0; i < n; i++) {
					int qi = resolve_q(sigmap(synced[i]));
					if (qi < 0 || used[qi]) {
						sok = false;
						break;
					}
					used[qi] = 1;
				}
				if (!sok)
					continue;
				std::vector<SigBit> lbits;
				for (int i = 0; i < n; i++)
					lbits.push_back(sigmap(local[i]));
				if (gray_encoder(lbits, comp[0].dst))
					return true;
			}
		}
		return false;
	}

	void find_crossings()
	{
		std::vector<Crossing> crossings;

		for (auto cell : module->cells()) {
			if (!cell->is_builtin_ff())
				continue;
			FfData ff(nullptr, cell);
			if (!ff.has_clk)
				continue;
			int d_ff = ff_domain(cell);
			if (d_ff < 0)
				continue;
			std::string cname = cell->name.unescape();

			if (GetSize(ff.sig_d) == ff.width) {
				SigSpec dsig = sigmap(ff.sig_d);
				for (int i = 0; i < ff.width; i++) {
					std::set<int> foreign, kin;
					for (int d : bit_domains(dsig[i]))
						if (d != d_ff) {
							if (family_of(d) == family_of(d_ff))
								kin.insert(d);
							else
								foreign.insert(d);
						}
					if (!kin.empty())
						findings.push_back({"OK", "same-clock-family", join_domains(kin),
								domain_names[d_ff], bit_name(dsig[i]), cname});
					if (foreign.empty())
						continue;
					Crossing c{d_ff, foreign, bit_name(dsig[i]), dsig[i], cname,
							is_synchronized(cell, i, d_ff), cell, i};
					if (c.synced) {
						// the first stage is metastable: only the second
						// stage may read it (assertions that observe it are
						// not hardware and do not count)
						auto chain = sync_chain(cell, i, d_ff);
						SigBit q1 = sigmap(ff.sig_q[i]);
						if (q1.wire && q1.wire->port_output)
							c.fanout = true;
						auto cit = consumers.find(q1);
						if (cit != consumers.end())
							for (Cell *u : cit->second)
								if (u != chain[1].first && !is_formal(u))
									c.fanout = true;
						// the crossing net must be a launch flop's Q: logic
						// between the source flops and the synchronizer can
						// glitch, and a glitch is what the synchronizer then
						// captures
						auto dit = drivers.find(dsig[i]);
						if (dit != drivers.end() && !dit->second.first->is_builtin_ff())
							c.comb = true;
					}
					crossings.push_back(c);
				}
			}

			auto check_control = [&](SigSpec sig, const char *tag) {
				if (GetSize(sig) != 1)
					return;
				SigBit bit = sigmap(sig[0]);
				std::set<int> foreign, kin;
				for (int d : bit_domains(bit))
					if (d != d_ff) {
						if (family_of(d) == family_of(d_ff))
							kin.insert(d);
						else
							foreign.insert(d);
					}
				if (!kin.empty())
					findings.push_back({"OK", "same-clock-family", join_domains(kin),
							domain_names[d_ff], bit_name(bit) + tag, cname});
				if (foreign.empty())
					return;
				crossings.push_back({d_ff, foreign, bit_name(bit) + tag, bit, cname, false});
			};
			if (ff.has_ce)
				check_control(ff.sig_ce, " (ce)");
			if (ff.has_srst)
				check_control(ff.sig_srst, " (srst)");

			if (ff.has_arst && GetSize(ff.sig_arst) == 1) {
				SigBit bit = sigmap(ff.sig_arst[0]);
				std::set<int> foreign;
				for (int d : bit_domains(bit))
					if (d != d_ff && family_of(d) != family_of(d_ff))
						foreign.insert(d);
				bool external = foreign.empty() && bit.wire && bit.wire->port_input &&
						domain_clks[d_ff] != bit;
				std::string cls = reset_class(d_ff, cell, bit);
				if (!foreign.empty() || external) {
					std::string srcs = foreign.empty() ? "(external)" : join_domains(foreign);
					if (cls.empty())
						findings.push_back({"WARNING", "reset-cdc", srcs,
								domain_names[d_ff], bit_name(bit), cname});
					else
						findings.push_back({"OK", cls, srcs,
								domain_names[d_ff], bit_name(bit), cname});
				} else if (cls == "reset-sync-deassert") {
					// same-domain async reset, but generated by a reset
					// synchronizer tail: deassertion is sequenced
					findings.push_back({"OK", cls, "(synchronized)",
							domain_names[d_ff], bit_name(bit), cname});
				}
			}
		}

		std::map<std::pair<int, std::set<int>>, std::vector<int>> groups;
		for (int i = 0; i < GetSize(crossings); i++)
			groups[std::make_pair(crossings[i].dst, crossings[i].srcs)].push_back(i);

		for (auto &g : groups) {
			// partition: unsynchronized members are errors (a multi-bit
			// unsynchronized group is a bus); synchronized members go
			// through the reconvergence/Gray split below
			std::vector<int> unsynced, synced;
			for (int i : g.second)
				(crossings[i].synced ? synced : unsynced).push_back(i);
			for (int i : unsynced) {
				Crossing &c = crossings[i];
				std::string sev = "ERROR";
				std::string cls = GetSize(unsynced) > 1 ?
						"unsynchronized-bus" : "unsynchronized-bit";
				// SDC pairing: a crossing the constraints declared
				// asynchronous is the designer's waiver -- keep it
				// visible but downgrade to a warning
				if (async_declared(c.srcs, c.dst)) {
					sev = "WARNING";
					cls = "async-declared";
				}
				findings.push_back({sev, cls, join_domains(c.srcs),
						domain_names[c.dst], c.bit, c.cell});
			}
			{
				std::vector<int> keep;
				for (int i : synced) {
					Crossing &c = crossings[i];
					if (c.fanout)
						findings.push_back({"ERROR", "sync-fanout", join_domains(c.srcs),
								domain_names[c.dst], c.bit, c.cell});
					else if (c.comb)
						findings.push_back({"WARNING", "comb-before-sync", join_domains(c.srcs),
								domain_names[c.dst], c.bit, c.cell});
					else
						keep.push_back(i);
				}
				synced = keep;
			}
			if (synced.empty())
				continue;
			if (GetSize(synced) == 1) {
				// a lone synchronized member (of a mixed group) is an
				// independent synchronizer
				Crossing &c = crossings[synced.front()];
				findings.push_back({"OK", "synchronizer-2ff", join_domains(c.srcs),
						domain_names[c.dst], c.bit, c.cell});
				continue;
			}
			{
				// Reconvergence split: a synced crossing whose forward cone
				// shares no comb cell and no sink FF with its group
				// siblings is an independent synchronizer, not part of a
				// bus -- promote it to OK.  Bits whose cones merge stay a
				// synced-bus WARNING unless the component structurally is
				// a Gray-coded bus, which upgrades to OK gray-bus.
				int n = GetSize(synced);
				std::vector<int> parent(n);
				for (int i = 0; i < n; i++)
					parent[i] = i;
				std::function<int(int)> find = [&](int x) {
					while (parent[x] != x) {
						parent[x] = parent[parent[x]];
						x = parent[x];
					}
					return x;
				};
				std::vector<pool<Cell *>> comb(n), comb_tail(n);
				std::vector<pool<std::pair<Cell *, int>>> sinks(n);
				for (int a = 0; a < n; a++) {
					q_cone(crossings[synced[a]].ffcell, crossings[synced[a]].qidx, comb[a], sinks[a]);
					// the reconvergence that makes a synced group a bus
					// (Gray decoder chain, full/empty EQ compare) usually
					// sits on the synchronizer TAIL side, not on the
					// crossing FF: a plain vector synchronizer's tail
					// cones are bit-disjoint, while Gray/eq-reconverged
					// tails share comb cells
					pool<std::pair<Cell *, int>> tail_sinks;
					auto chain = sync_chain(crossings[synced[a]].ffcell,
							crossings[synced[a]].qidx, g.first.first);
					q_cone(chain.back().first, chain.back().second,
							comb_tail[a], tail_sinks);
				}
				auto share = [&](int a, int b) {
					for (Cell *c : comb[a])
						if (comb[b].count(c))
							return true;
					for (auto &c : sinks[a])
						if (sinks[b].count(c))
							return true;
					for (Cell *c : comb_tail[a])
						if (comb_tail[b].count(c))
							return true;
					return false;
				};
				for (int a = 0; a < n; a++)
					for (int b = a + 1; b < n; b++)
						if (share(a, b))
							parent[find(a)] = find(b);
				std::map<int, std::vector<int>> comps;
				for (int a = 0; a < n; a++)
					comps[find(a)].push_back(a);
				// Gray recognition applies only to merged components; a
				// recognized Gray bus needs the bin->gray $xor encoder on
				// the source side and, on the destination side, either the
				// prefix-XOR decoder chain or an EQ-compare against a local
				// Gray-coded vector (FIFO full/empty style, possibly with
				// per-bit polarity on the synced operand).
				std::map<int, bool> comp_gray, comp_step;
				for (auto &it : comps) {
					if (GetSize(it.second) < 2)
						continue;
					std::vector<Crossing> comp;
					for (int a : it.second)
						comp.push_back(crossings[synced[a]]);
					std::sort(comp.begin(), comp.end(), [](const Crossing &x, const Crossing &y) {
						if (x.ffcell->name != y.ffcell->name)
							return x.ffcell->name < y.ffcell->name;
						return x.qidx < y.qidx;
					});
					comp_gray[it.first] = (gray_encoder(comp) && gray_decoder(comp)) || eq_gray_compare(comp);
					if (comp_gray[it.first]) {
						std::vector<SigBit> dbits, wbits;
						for (auto &c : comp)
							dbits.push_back(c.dbit);
						comp_step[it.first] = gray_encoder(dbits, -1, &wbits) && gray_step_ok(wbits);
					}
				}
				for (int a = 0; a < n; a++) {
					Crossing &c = crossings[synced[a]];
					bool lone = GetSize(comps[find(a)]) == 1;
					std::string sev = "WARNING", cls = "synced-bus";
					if (lone) {
						sev = "OK";
						cls = "synchronizer-2ff";
					} else if (comp_gray[find(a)]) {
						// Gray shape is not Gray behaviour: OK only once
						// the encoded vector is proved to step by one
						sev = comp_step[find(a)] ? "OK" : "WARNING";
						cls = comp_step[find(a)] ? "gray-bus" : "gray-bus-unverified";
					}
					findings.push_back({sev, cls, join_domains(c.srcs), domain_names[c.dst], c.bit, c.cell});
				}
			}
		}

		std::sort(findings.begin(), findings.end(), [](const CdcFinding &a, const CdcFinding &b) {
			if (a.cell != b.cell)
				return a.cell < b.cell;
			if (a.bit != b.bit)
				return a.bit < b.bit;
			return a.cls < b.cls;
		});
	}

	void run(Design *design)
	{
		build_maps();
		infer_domains();
		propagate();
		analyze_clock_network();
		if (design)
			load_sdc(design);

		for (auto &it : domains) {
			if (it.first.wire && it.second.size() > 1) {
				num_crossing_nets++;
				log_debug("cdc: module %s: crossing net %s (domains:", log_id(module->name), bit_name(it.first).c_str());
				for (int d : it.second)
					log_debug(" %s", domain_names[d].c_str());
				log_debug(")\n");
			}
		}

		find_crossings();
		report_unanalyzed(design);
	}

	// (e) everything the structural analysis skips, said once per module
	void report_unanalyzed(Design *design)
	{
		int latches = 0, mems = 0, subs = 0, noclk = 0;
		for (auto cell : module->cells()) {
			if (cell->type.in(ID($dlatch), ID($adlatch), ID($dlatchsr), ID($dl)) ||
					cell->type.begins_with("$_DLATCH"))
				latches++;
			else if (cell->is_builtin_ff()) {
				FfData ff(nullptr, cell);
				if (!ff.has_clk || GetSize(ff.sig_clk) != 1)
					noclk++;
			} else if (cell->is_mem_cell())
				mems++;
			else if (design && design->module(cell->type) != nullptr)
				subs++;
		}
		if (latches)
			log_warning("cdc: module %s: %d latch(es) not analyzed: crossings through latches are not reported.\n", log_id(module), latches);
		if (mems)
			log_warning("cdc: module %s: %d memory cell(s) not analyzed: run memory_map before cdc.\n", log_id(module), mems);
		if (subs)
			log_warning("cdc: module %s: %d submodule instance(s): crossings into or out of them are not analyzed; run flatten before cdc.\n", log_id(module), subs);
		if (noclk)
			log_warning("cdc: module %s: %d flip-flop(s) without a single clock bit ($ff/global-clock or multi-bit clock) not analyzed.\n", log_id(module), noclk);
		// input ports reaching FF data inputs: no launch domain is known
		// (an SDC set_input_delay -clock is stored but not used)
		pool<std::string> ports;
		for (auto cell : module->cells()) {
			if (!cell->is_builtin_ff())
				continue;
			FfData ff(nullptr, cell);
			if (!ff.has_clk)
				continue;
			pool<SigBit> seen;
			std::vector<SigBit> work;
			for (auto b : sigmap(ff.sig_d))
				if (b.wire && seen.insert(b).second)
					work.push_back(b);
			while (!work.empty() && GetSize(seen) < 512) {
				SigBit b = work.back();
				work.pop_back();
				if (b.wire->port_input && !clk_domains.count(b)) {
					ports.insert(b.wire->name.unescape());
					continue;
				}
				auto dit = drivers.find(b);
				if (dit == drivers.end() || dit->second.first->is_builtin_ff())
					continue;
				for (auto &conn : dit->second.first->connections())
					if (dit->second.first->input(conn.first))
						for (auto ib : sigmap(conn.second))
							if (ib.wire && seen.insert(ib).second)
								work.push_back(ib);
			}
		}
		if (!ports.empty()) {
			std::vector<std::string> v(ports.begin(), ports.end());
			std::sort(v.begin(), v.end());
			std::string list;
			for (int i = 0; i < GetSize(v) && i < 4; i++)
				list += (list.empty() ? "" : ", ") + v[i];
			if (GetSize(v) > 4)
				list += stringf(", ... (%d total)", GetSize(v));
			log_warning("cdc: module %s: input port(s) %s reach flip-flop data inputs with no known launch clock: those paths are not checked.\n",
					log_id(module), list.c_str());
		}
		if (bound_hits)
			log_warning("cdc: module %s: %d bounded search(es) gave up; their findings take the conservative answer.\n", log_id(module), bound_hits);
	}
};

struct CdcPass : public Pass {
	CdcPass() : Pass("cdc", "structural clock-domain crossing analysis") { }
	bool formatted_help() override {
		auto *help = PrettyHelp::get_current();
		help->set_group("passes/status");
		return false;
	}
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    cdc [options] [selection]\n");
		log("\n");
		log("This command performs a structural clock-domain crossing (CDC) analysis of\n");
		log("the selected modules. The design must be prepared first: run 'proc' (usually\n");
		log("followed by 'opt_clean') before this pass. A selected module that still\n");
		log("contains processes is a fatal error.  The analysis is per module and does\n");
		log("not look through latches or memories: run 'flatten' and 'memory_map' first.\n");
		log("What a module leaves unanalyzed (latches, memory cells, submodule\n");
		log("instances, flip-flops without a single clock bit, input ports that reach\n");
		log("flip-flops with no known launch clock, bounded searches that gave up) is\n");
		log("reported as a warning, never silently.\n");
		log("\n");
		log("Clock domains are inferred from the clock pins of all flip-flop cells in\n");
		log("each module; a top-level input port driving FF clock pins becomes a clock\n");
		log("domain named after that port. The domains of all other signals are computed\n");
		log("by propagating domain tags through combinatorial cells to a fixpoint. Nets\n");
		log("on which multiple domains mix are recorded as crossing nets (see the debug\n");
		log("log with 'yosys -g' or the 'debug' command).\n");
		log("\n");
		log("A flip-flop whose data input carries a domain other than the flip-flop's\n");
		log("own clock domain is reported as a crossing, classified as follows:\n");
		log("\n");
		log("  - synchronizer-2ff   (OK)      crossing followed by a 2+ stage chain of\n");
		log("                                 same-domain flip-flops (Q feeding next D),\n");
		log("                                 launched from a flip-flop Q, first stage\n");
		log("                                 read only by the second stage\n");
		log("  - sync-fanout        (ERROR)   the first (metastable) synchronizer stage\n");
		log("                                 also feeds logic or a port\n");
		log("  - comb-before-sync   (WARNING) the synchronized net is combinational\n");
		log("                                 logic, not a launch flop's Q: it can\n");
		log("                                 glitch, and the glitch is what gets sampled\n");
		log("  - same-clock-family  (OK)      crossing between domains derived from one\n");
		log("                                 clock (e.g. a divide-by-2 generated clock):\n");
		log("                                 synchronous, not a metastability risk\n");
		log("  - generated-clock    (OK)      domain clock derived from a parent domain\n");
		log("                                 without combinatorial gating (divider FF)\n");
		log("  - clock-gate-icg     (OK)      gated clock whose enable cone holds a latch\n");
		log("  - clock-gate-reg     (WARNING) enable held only by registers: combinational\n");
		log("                                 clock ANDing can still emit runt pulses\n");
		log("  - clock-gating-glitch (ERROR)  enable asynchronous to the parent clock or\n");
		log("                                 never registered/latched: glitch hazard\n");
		log("  - clock-mux          (WARNING) domain clock muxed from multiple parent\n");
		log("                                 clocks: asynchronous to every one of them\n");
		log("                                 (it joins no parent's clock family)\n");
		log("  - clock-gating-ambiguous (WARNING) a clock gate with two input ports that\n");
		log("                                 both look like clocks by name: which one is\n");
		log("                                 the enable cannot be decided structurally\n");
		log("  - reset-sync-stage   (OK)      the flop belongs to a >=2-stage reset\n");
		log("                                 synchronizer chain (async assert, the\n");
		log("                                 chain synchronizes the deassertion): head\n");
		log("                                 D tied to c, every stage resets to !c\n");
		log("  - reset-sync-deassert (OK)     async reset driven by a reset synchronizer\n");
		log("                                 tail in the destination domain\n");
		log("  - gray-bus           (OK)      synced bus whose bits are Gray-coded:\n");
		log("                                 bin->gray $xor encoder in the source\n");
		log("                                 domain, and in the destination domain either\n");
		log("                                 a prefix-XOR decoder chain over all synced\n");
		log("                                 bits or an $eq compare against a local\n");
		log("                                 Gray-coded vector (FIFO full/empty style);\n");
		log("                                 and SAT proves the encoded binary register\n");
		log("                                 steps by +1/-1 or holds (one Gray bit per clock)\n");
		log("  - gray-bus-unverified (WARNING) Gray-shaped bus whose one-bit-per-clock\n");
		log("                                 step could not be proved\n");
		log("  - unsynchronized-bit (ERROR)   single-bit crossing without a synchronizer\n");
		log("  - async-declared     (WARNING) unsynchronized crossing between clocks\n");
		log("                                 declared asynchronous or false-pathed via\n");
		log("                                 read_sdc constraints: waived, kept visible\n");
		log("  - synced-bus         (WARNING) all bits of a multi-bit crossing use 2+\n");
		log("                                 stage synchronizers; check Gray coding or\n");
		log("                                 handshake coherence between the domains\n");
		log("  (Handshake / multi-cycle-path data buses qualified by a synchronized\n");
		log("  enable are not recognized: their data bits report unsynchronized-bus.)\n");
		log("  - unsynchronized-bus (ERROR)   a multi-bit crossing with unsynchronized bits\n");
		log("  - reset-cdc          (WARNING) asynchronous reset input driven from another\n");
		log("                                 clock domain or from an external input port\n");
		log("                                 whose deassertion is not synchronized in the\n");
		log("                                 destination domain\n");
		log("\n");
		log("Each domain's clock net is traced back through the driver graph to the\n");
		log("domain(s) it derives from (register dividers, gated clocks, clock muxes),\n");
		log("and a clock family (ultimate root domain) is derived.  Crossings between\n");
		log("domains of one family are reported OK same-clock-family instead of being\n");
		log("treated as asynchronous.  Gated clocks are additionally classified by\n");
		log("their enable's holding structure (see the class list above).\n");
		log("\n");
		log("Clock enable and synchronous reset inputs are checked like data inputs.\n");
		log("Findings are printed as a table and counted in a final summary; the pass\n");
		log("never aborts on findings.\n");
		log("\n");
		log("If the design carries constraints from the read_sdc pass (scratchpad key\n");
		log("sdc.json), clock targets are mapped onto the domain names and crossings\n");
		log("between clocks declared asynchronous (set_clock_groups -asynchronous) or\n");
		log("connected by set_false_path are paired with the declaration: unsynchronized\n");
		log("ERRORs downgrade to WARNING async-declared.  Synchronized crossings,\n");
		log("reset findings, and clock-network findings are unaffected.\n");
		log("\n");
		log("Options:\n");
		log("\n");
		log("    -json <file>\n");
		log("        write all findings to the given file as a JSON array of objects\n");
		log("        with the fields 'severity', 'class', 'src', 'dst', 'bit',\n");
		log("        'cell' and 'module'.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		std::string json_file;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-json" && argidx + 1 < args.size()) {
				json_file = args[++argidx];
				continue;
			}
			break;
		}
		extra_args(args, argidx, design);

		log_header(design, "Executing CDC pass (structural clock-domain crossing analysis).\n");

		std::vector<CdcFinding> all_findings;
		int total = 0, num_errors = 0, num_warnings = 0, num_ok = 0, num_nets = 0;

		for (auto module : design->selected_whole_modules_warn())
		{
			if (!module->processes.empty())
				log_error("Module %s still contains processes. Run 'proc' before 'cdc'.\n", module);

			CdcAnalyzer analyzer(module);
			analyzer.run(design);

			num_nets += analyzer.num_crossing_nets;

			log("Analyzing module %s:", module);
			if (analyzer.domain_names.empty()) {
				log(" no clock domains found");
			} else {
				log(" clock domains:");
				for (auto &name : analyzer.domain_names)
					log(" %s", name);
			}
			log("\n");

			for (auto &f : analyzer.findings) {
				log("  %-9s %-22s %s -> %s, bit %s (ff %s)\n",
						(f.severity + ":").c_str(), f.cls.c_str(), f.src.c_str(), f.dst.c_str(),
						f.bit.c_str(), f.cell.c_str());
				f.module = log_id(module);
				all_findings.push_back(f);
				total++;
				if (f.severity == "ERROR")
					num_errors++;
				else if (f.severity == "WARNING")
					num_warnings++;
				else
					num_ok++;
			}
		}

		log("%d crossings analyzed, %d errors, %d warnings, %d ok.\n", total, num_errors, num_warnings, num_ok);
		if (num_nets)
			log_debug("cdc: %d multi-domain crossing nets recorded.\n", num_nets);

		if (!json_file.empty()) {
			json11::Json::array arr;
			for (auto &f : all_findings)
				arr.push_back(json11::Json::object{
					{"severity", f.severity},
					{"class", f.cls},
					{"src", f.src},
					{"dst", f.dst},
					{"bit", f.bit},
					{"cell", f.cell},
					{"module", f.module},
				});
			std::string out = json11::Json(arr).dump();
			FILE *fp = fopen(json_file.c_str(), "wt");
			if (fp == nullptr)
				log_error("Can't open JSON output file `%s'.\n", json_file);
			fwrite(out.data(), 1, out.size(), fp);
			fclose(fp);
			log("Wrote %d findings to `%s'.\n", GetSize(arr), json_file);
		}
	}
} CdcPass;

PRIVATE_NAMESPACE_END
