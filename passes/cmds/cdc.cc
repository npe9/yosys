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
// two-flop synchronizers (OK), bare bits and buses (ERROR), synchronized
// buses (WARNING), and asynchronous resets from foreign domains (WARNING).
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

	// Follow the synchronizer chain forward from the crossing FF: count
	// consecutive flip-flops of domain d_ff, each Q bit feeding the next D
	// bit. A chain of 2 or more stages means the crossing is synchronized.
	bool is_synchronized(Cell *cell, int qidx, int d_ff)
	{
		std::set<std::pair<Cell *, int>> visited;
		visited.insert(std::make_pair(cell, qidx));
		int stages = 1;
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
			cur_cell = cands.front().first;
			cur_idx = cands.front().second;
			stages++;
		}
		return stages >= 2;
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

	void find_crossings()
	{
		struct Crossing {
			int dst;
			std::set<int> srcs;
			std::string bit;
			std::string cell;
			bool synced;
			Cell *ffcell;
			int qidx;
		};
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
					crossings.push_back({d_ff, foreign, bit_name(dsig[i]), cname,
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
				crossings.push_back({d_ff, foreign, bit_name(bit) + tag, cname, false});
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
				// synced-bus WARNING (Gray/decoder recognition would
				// further upgrade a true Gray bus; not done yet).
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
				std::map<int, int> comp_size;
				for (int a = 0; a < n; a++)
					comp_size[find(a)]++;
				for (int a = 0; a < n; a++) {
					Crossing &c = crossings[g.second[a]];
					bool lone = comp_size[find(a)] == 1;
					findings.push_back({lone ? "OK" : "WARNING",
							lone ? "synchronizer-2ff" : "synced-bus",
							join_domains(c.srcs), domain_names[c.dst], c.bit, c.cell});
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
