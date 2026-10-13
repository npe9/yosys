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
// foreign domains (WARNING).
// The design must be process-free: run 'proc' before this pass.

#include "kernel/yosys.h"
#include "kernel/sigtools.h"
#include "kernel/ff.h"
#include "kernel/log_help.h"
#include "libs/json11/json11.hpp"

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
	// ends the walk).  Two synced crossings of one group whose cones share
	// a comb cell or a sink FF reconverge in the destination domain; a
	// crossing whose cone shares nothing with its group siblings is an
	// independent synchronizer even when the group carries a bus.
	void q_cone(Cell *ff, int qidx, pool<Cell *> &comb, pool<Cell *> &sinks)
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
					sinks.insert(c);
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

	// Source side of a Gray-coded bus: every crossing bit must be produced
	// by a $xor encoder of the shape gray[i] = w[i] ^ w[i+1] over one
	// common source vector w (matching bin -> bin ^ (bin >> 1)), with the
	// component bits spanning a contiguous, monotonic run of w and only
	// the last bit's neighbor constant (gray[MSB] = w[MSB] ^ 0).  The
	// encoder is located on the crossing D bit itself; a registered
	// encoder output is hopped across before checking.  Component bits
	// arrive ordered by (ffcell, qidx).
	bool gray_encoder(const std::vector<Crossing> &comp)
	{
		int n = GetSize(comp);
		std::vector<std::vector<std::pair<SigBit, SigBit>>> opts(n);
		for (int k = 0; k < n; k++) {
			SigBit dbit = comp[k].dbit;
			Cell *x = nullptr;
			for (int hop = 0; hop < 4; hop++) {
				auto dit = drivers.find(dbit);
				if (dit == drivers.end())
					return false;
				x = dit->second.first;
				if (!x->is_builtin_ff())
					break;
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
		std::function<bool(int, Wire *, int, int, SigBit)> dfs = [&](int k, Wire *w, int prev_off, int step, SigBit prev_nxt) {
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
				if (dfs(k + 1, w != nullptr ? w : cur.wire, cur.offset, step, nxt))
					return true;
			}
			return false;
		};
		return dfs(0, nullptr, 0, 0, SigBit());
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
			pool<Cell *> comb, sinks;
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
			if (++nodes > 100000)
				return false;
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
					std::set<int> foreign;
					for (int d : bit_domains(dsig[i]))
						if (d != d_ff)
							foreign.insert(d);
					if (foreign.empty())
						continue;
					crossings.push_back({d_ff, foreign, bit_name(dsig[i]), dsig[i], cname,
							is_synchronized(cell, i, d_ff), cell, i});
				}
			}

			auto check_control = [&](SigSpec sig, const char *tag) {
				if (GetSize(sig) != 1)
					return;
				SigBit bit = sigmap(sig[0]);
				std::set<int> foreign;
				for (int d : bit_domains(bit))
					if (d != d_ff)
						foreign.insert(d);
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
					if (d != d_ff)
						foreign.insert(d);
				if (!foreign.empty()) {
					findings.push_back({"WARNING", "reset-cdc", join_domains(foreign),
							domain_names[d_ff], bit_name(bit), cname});
				} else if (bit.wire && bit.wire->port_input && domain_clks[d_ff] != bit) {
					findings.push_back({"WARNING", "reset-cdc", "(external)",
							domain_names[d_ff], bit_name(bit), cname});
				}
			}
		}

		std::map<std::pair<int, std::set<int>>, std::vector<int>> groups;
		for (int i = 0; i < GetSize(crossings); i++)
			groups[std::make_pair(crossings[i].dst, crossings[i].srcs)].push_back(i);

		for (auto &g : groups) {
			bool all_synced = true;
			for (int i : g.second)
				if (!crossings[i].synced) {
					all_synced = false;
					break;
				}
			if (g.second.size() > 1 && all_synced) {
				// Reconvergence split: a synced crossing whose forward cone
				// shares no comb cell and no sink FF with its group
				// siblings is an independent synchronizer, not part of a
				// bus -- promote it to OK.  Bits whose cones merge stay a
				// synced-bus WARNING unless the component structurally is
				// a Gray-coded bus, which upgrades to OK gray-bus.
				int n = GetSize(g.second);
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
				std::vector<pool<Cell *>> comb(n), sinks(n);
				for (int a = 0; a < n; a++)
					q_cone(crossings[g.second[a]].ffcell, crossings[g.second[a]].qidx, comb[a], sinks[a]);
				auto share = [&](int a, int b) {
					for (Cell *c : comb[a])
						if (comb[b].count(c))
							return true;
					for (Cell *c : sinks[a])
						if (sinks[b].count(c))
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
				// the source side and the prefix-XOR decoder chain on the
				// destination side.  EQ-compare Gray recognition
				// (local-Gray vs synced-Gray $eq cross-check, as in FIFO
				// full/empty pointers) is the next increment.
				std::map<int, bool> comp_gray;
				for (auto &it : comps) {
					if (GetSize(it.second) < 2)
						continue;
					std::vector<Crossing> comp;
					for (int a : it.second)
						comp.push_back(crossings[g.second[a]]);
					std::sort(comp.begin(), comp.end(), [](const Crossing &x, const Crossing &y) {
						if (x.ffcell->name != y.ffcell->name)
							return x.ffcell->name < y.ffcell->name;
						return x.qidx < y.qidx;
					});
					comp_gray[it.first] = gray_encoder(comp) && gray_decoder(comp);
				}
				for (int a = 0; a < n; a++) {
					Crossing &c = crossings[g.second[a]];
					bool lone = GetSize(comps[find(a)]) == 1;
					std::string sev = "WARNING", cls = "synced-bus";
					if (lone) {
						sev = "OK";
						cls = "synchronizer-2ff";
					} else if (comp_gray[find(a)]) {
						sev = "OK";
						cls = "gray-bus";
					}
					findings.push_back({sev, cls, join_domains(c.srcs), domain_names[c.dst], c.bit, c.cell});
				}
				continue;
			}
			for (int i : g.second) {
				Crossing &c = crossings[i];
				std::string sev, cls;
				if (g.second.size() > 1) {
					sev = "ERROR";
					cls = "unsynchronized-bus";
				} else {
					if (c.synced) {
						sev = "OK";
						cls = "synchronizer-2ff";
					} else {
						sev = "ERROR";
						cls = "unsynchronized-bit";
					}
				}
				findings.push_back({sev, cls, join_domains(c.srcs), domain_names[c.dst], c.bit, c.cell});
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

	void run()
	{
		build_maps();
		infer_domains();
		propagate();

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
		log("contains processes is a fatal error.\n");
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
		log("                                 same-domain flip-flops (Q feeding next D)\n");
		log("  - gray-bus           (OK)      synced bus whose bits are Gray-coded:\n");
		log("                                 bin->gray $xor encoder in the source\n");
		log("                                 domain, prefix-XOR decoder chain over all\n");
		log("                                 synced bits in the destination domain\n");
		log("  - unsynchronized-bit (ERROR)   single-bit crossing without a synchronizer\n");
		log("  - synced-bus         (WARNING) all bits of a multi-bit crossing use 2+\n");
		log("                                 stage synchronizers; check Gray coding or\n");
		log("                                 handshake coherence between the domains\n");
		log("  - unsynchronized-bus (ERROR)   a multi-bit crossing with unsynchronized bits\n");
		log("  - reset-cdc          (WARNING) asynchronous reset input driven from another\n");
		log("                                 clock domain or from an external input port\n");
		log("\n");
		log("Clock enable and synchronous reset inputs are checked like data inputs.\n");
		log("Findings are printed as a table and counted in a final summary; the pass\n");
		log("never aborts on findings.\n");
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
			analyzer.run();

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
