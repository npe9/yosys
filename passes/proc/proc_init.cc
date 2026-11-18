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

#include "kernel/register.h"
#include "kernel/sigtools.h"
#include "kernel/log.h"
#include "kernel/celltypes.h"
#include <stdlib.h>
#include <stdio.h>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

void proc_init(RTLIL::Module *mod, SigMap &sigmap, RTLIL::Process *proc)
{
	for (auto &sync : proc->syncs)
		if (sync->type == RTLIL::SyncType::STi)
		{
			log("Found init rule in `%s.%s'.\n", mod->name, proc->name);

			for (auto &action : sync->actions)
			{
				RTLIL::SigSpec lhs = action.first;
				RTLIL::SigSpec rhs = sigmap(action.second);

				if (!rhs.is_fully_const()) {
					// Non-constant initial values arise from simulation-only
					// code (e.g. `int i = 0; initial i++;` collapses to an
					// init rule of i+1). The init attribute cannot represent
					// them; skip instead of aborting so such code elaborates.
					// fe218: the variable then starts from its init
					// attribute (or a free/zero init), not the value
					// the initial block computed: a degradation
					log_warning("[sv_degraded] Skipping non-constant init value for %s: %s\n", log_signal(lhs), log_signal(rhs));
					std::string cur = mod->get_string_attribute(ID(sv_degraded));
					std::string entry = stringf("non-constant init value for %s @%s", log_signal(lhs), proc->get_src_attribute().c_str());
					mod->set_string_attribute(ID(sv_degraded), cur.empty() ? entry : cur + "; " + entry);
					continue;
				}

				int offset = 0;
				for (auto &lhs_c : lhs.chunks())
				{
					if (lhs_c.wire != nullptr)
					{
						SigSpec valuesig = rhs.extract(offset, lhs_c.width);
						if (!valuesig.is_fully_const())
							log_cmd_error("Non-const initialization value: %s = %s\n", log_signal(lhs_c), log_signal(valuesig));

						Const value = valuesig.as_const();
						Const &wireinit = lhs_c.wire->attributes[ID::init];
						if (GetSize(wireinit) < lhs_c.wire->width)
							wireinit.resize(lhs_c.wire->width, State::Sx);
						bool warned_conflict = false;
						for (int i = 0; i < lhs_c.width; i++) {
							int index = i + lhs_c.offset;
							State initbit = wireinit[index];
							if (initbit != State::Sx && initbit != value[i]) {
								// Multiple time-zero sources may disagree,
								// e.g. a declaration initializer
								// (`bit a = 0;`) and a later explicit
								// `initial a = 1;`. IEEE executes the
								// declaration assignment first, so the
								// later source wins; represent that
								// instead of aborting.
								if (!warned_conflict) {
									log_warning("Overriding conflicting initialization value for %s (%s -> %s).\n",
											log_signal(lhs_c), log_signal(wireinit), log_signal(valuesig));
									warned_conflict = true;
								}
							}
							wireinit.set(index, value[i]);
						}
						log("  Set init value: %s = %s\n", log_signal(lhs_c.wire), log_signal(wireinit));
					}
					offset += lhs_c.width;
				}
			}
			sync->actions.clear();
		}
}

struct ProcInitPass : public Pass {
	ProcInitPass() : Pass("proc_init", "convert initial block to init attributes") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    proc_init [selection]\n");
		log("\n");
		log("This pass extracts the 'init' actions from processes (generated from Verilog\n");
		log("'initial' blocks) and sets the initial value to the 'init' attribute on the\n");
		log("respective wire.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing PROC_INIT pass (extract init attributes).\n");

		extra_args(args, 1, design);

		for (auto mod : design->all_selected_modules()) {
			SigMap sigmap(mod);
			for (auto proc : mod->selected_processes())
				proc_init(mod, sigmap, proc);
		}
	}
} ProcInitPass;

// fe218: the wires an initial block drives through an `always' sync rule
// (sv_init_drv, a per-bit mask set by the frontend) are continuous drivers after proc.
// That is exact for a value constant at time 0 only: `initial v = w;'
// would make v follow w, and a second time-0 source (a declaration
// initializer, another initial block) is a second driver that folding
// resolves to either one.  After folding, each driver of a marked bit is
// followed through connections: a single driver must be a constant or a
// free time-0 constant ($anyconst/$allconst); several drivers must all be
// constants.  Anything else is a degradation.
struct ProcInitCheckPass : public Pass {
	ProcInitCheckPass() : Pass("proc_initcheck", "check initial-block drivers") { }
	void help() override
	{
		log("\n");
		log("    proc_initcheck [selection]\n");
		log("\n");
		log("This pass checks the wires an initial block drives continuously (attribute\n");
		log("sv_init_drv): each must be driven by a time-0 constant ($anyconst counts when\n");
		log("it is the only driver).  A wire that is not is marked (* sv_degraded *).\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing PROC_INITCHECK pass (check initial-block drivers).\n");
		extra_args(args, 1, design);
		for (auto mod : design->selected_modules()) {
			std::vector<RTLIL::Wire*> marked;
			for (auto w : mod->wires())
				if (w->attributes.count(ID(sv_init_drv)))
					marked.push_back(w);
			if (marked.empty())
				continue;
			dict<RTLIL::SigBit, std::vector<RTLIL::SigBit>> conn_drv;
			for (auto &conn : mod->connections())
				for (int i = 0; i < GetSize(conn.first); i++)
					if (conn.first[i].wire)
						conn_drv[conn.first[i]].push_back(conn.second[i]);
			dict<RTLIL::SigBit, RTLIL::Cell*> cell_drv;
			for (auto c : mod->cells())
				for (auto &p : c->connections())
					if (c->output(p.first))
						for (auto b : p.second)
							if (b.wire)
								cell_drv[b] = c;
			// CONST: constants only; FREE: a function of constants and
			// $anyconst/$allconst outputs through combinational cells (a
			// time-0 constant, e.g. $urandom_range in an initial block);
			// OTHER: anything else (an input, an undriven wire, a
			// register, $anyseq, several drivers)
			enum Kind { CONST = 0, FREE = 1, OTHER = 2 };
			CellTypes ct;
			ct.setup_internals_eval();
			ct.setup_stdcells_eval();
			dict<RTLIL::SigBit, Kind> memo;
			std::function<Kind(RTLIL::SigBit, int)> follow = [&](RTLIL::SigBit b, int depth) -> Kind {
				if (!b.wire)
					return CONST;
				if (depth > 2000)
					return OTHER;
				auto mi = memo.find(b);
				if (mi != memo.end())
					return mi->second;
				memo[b] = OTHER;   // a loop is OTHER
				Kind k = OTHER;
				auto ci = cell_drv.find(b);
				auto it = conn_drv.find(b);
				if (ci != cell_drv.end()) {
					RTLIL::Cell *c = ci->second;
					if (c->type.in(ID($anyconst), ID($allconst)))
						k = FREE;
					else if (ct.cell_evaluable(c->type) && (it == conn_drv.end())) {
						k = CONST;
						for (auto &p : c->connections()) {
							if (!c->input(p.first))
								continue;
							for (auto ib : p.second)
								k = std::max(k, follow(ib, depth + 1));
							if (k == OTHER)
								break;
						}
					}
				} else if (it != conn_drv.end() && GetSize(it->second) == 1)
					k = follow(it->second[0], depth + 1);
				memo[b] = k;
				return k;
			};
			for (auto w : marked) {
				std::string why;
				const RTLIL::Const &mask = w->attributes.at(ID(sv_init_drv));
				for (int k = 0; k < w->width && why.empty(); k++) {
					if (k < GetSize(mask) && mask[k] != RTLIL::State::S1)
						continue;
					RTLIL::SigBit b(w, k);
					std::vector<RTLIL::SigBit> drv;
					auto it = conn_drv.find(b);
					if (it != conn_drv.end())
						drv = it->second;
					if (cell_drv.count(b))
						drv.push_back(b);   // follow(b) reads the cell
					if (drv.empty())
						continue;
					if (GetSize(drv) > 1)
						memo.erase(b);
					int nconst = 0, nfree = 0;
					for (auto d : drv) {
						Kind kd = follow(d, 0);
						if (kd == CONST)
							nconst++;
						else if (kd == FREE)
							nfree++;
					}
					int n = GetSize(drv);
					if (n == 1 && nconst + nfree == 1)
						continue;
					if (n > 1 && nconst == n)
						continue;
					why = n > 1 ? stringf("%d time-0 sources (a declaration initializer or initial blocks), not all constant", n)
							: std::string("a value that is not constant at time 0");
				}
				w->attributes.erase(ID(sv_init_drv));
				if (why.empty())
					continue;
				log_warning("[sv_degraded] Initial block drives `%s.%s' with %s; the model would drive it continuously.\n",
						log_id(mod), log_id(w), why.c_str());
				std::string cur = mod->get_string_attribute(ID(sv_degraded));
				std::string entry = stringf("initial-block value of %s (%s) @%s", log_id(w), why.c_str(), w->get_src_attribute().c_str());
				mod->set_string_attribute(ID(sv_degraded), cur.empty() ? entry : cur + "; " + entry);
			}
		}
	}
} ProcInitCheckPass;

PRIVATE_NAMESPACE_END
