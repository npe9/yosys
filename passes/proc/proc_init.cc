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
					log_warning("Skipping non-constant init value for %s: %s\n", log_signal(lhs), log_signal(rhs));
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

PRIVATE_NAMESPACE_END
