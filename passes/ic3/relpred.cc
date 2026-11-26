/*
 *  relpred -- cross-copy mismatch predicates for relational proofs
 *
 *  SecIC3-style equivalence predicates (Tan et al., DATE 2026, arXiv
 *  2601.21353): for a register word X present in two copies of a design (a
 *  self-composition, or the gold/gate halves of an equivalence miter), add a
 *  1-bit register neq_X that tracks X1 != X2.  `ic3 -rel` then reasons with
 *  one literal per word instead of per bit.  The predicates only observe: no
 *  existing cell, init value or property changes, so every verdict on the
 *  augmented module is a verdict on the original one.
 */

#include "kernel/yosys.h"
#include "kernel/sigtools.h"
#include "kernel/ff.h"
#include "kernel/ffinit.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct RelpredPass : public Pass {
	RelpredPass() : Pass("relpred", "add cross-copy mismatch predicates for ic3 -rel") { }

	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    relpred [options] [selection]\n");
		log("\n");
		log("Pair register words of two copies of a design and add, for each pair\n");
		log("(X1, X2), a register neq_X with next state (X1' != X2') and initial value\n");
		log("(init(X1) != init(X2)), so that neq_X == (X1 != X2) on every reachable state.\n");
		log("The new wire carries the attribute relpred_pair = \"<X1> <X2>\", which\n");
		log("`ic3 -rel` reads.  Nothing else in the module changes.\n");
		log("\n");
		log("A word pairs when both wires are driven entirely by register Q bits of the\n");
		log("same kind (both $ff, or $dff on the same clock and polarity), with equal\n");
		log("width.  Registers with enable, reset or set inputs are refused with an\n");
		log("error: run `dffunmap` first.\n");
		log("\n");
		log("    -p1 <prefix>, -p2 <prefix>\n");
		log("        pair wire <p1>NAME with wire <p2>NAME (default: copy1. and copy2.,\n");
		log("        the self-composition naming; after flatten, gold./gate. for a\n");
		log("        miter -flatten miter).\n");
		log("\n");
		log("    -pairs <file>\n");
		log("        explicit pairs, one \"<wire1> <wire2>\" per line, instead of prefixes\n");
		log("        (e.g. from a name or signature matcher for designs that differ).\n");
		log("\n");
		log("An x init bit counts as 0 (the ic3 -init-zero model); without -init-zero\n");
		log("the seeded clauses of such a word fail ic3's own init check and are dropped.\n");
		log("\n");
	}

	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		std::string p1 = "copy1.", p2 = "copy2.", pairs_file;
		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-p1" && argidx+1 < args.size()) { p1 = args[++argidx]; continue; }
			if (args[argidx] == "-p2" && argidx+1 < args.size()) { p2 = args[++argidx]; continue; }
			if (args[argidx] == "-pairs" && argidx+1 < args.size()) { pairs_file = args[++argidx]; continue; }
			break;
		}
		extra_args(args, argidx, design);
		log_header(design, "Executing RELPRED pass.\n");

		for (auto module : design->selected_modules()) {
			SigMap sigmap(module);
			FfInitVals initvals(&sigmap, module);
			// Q bit -> (FF cell, bit offset)
			dict<SigBit, std::pair<Cell*, int>> qbit;
			for (auto cell : module->cells()) {
				if (!cell->is_builtin_ff())
					continue;
				SigSpec q = sigmap(cell->getPort(ID::Q));
				for (int i = 0; i < GetSize(q); i++)
					if (q[i].wire)
						qbit[q[i]] = {cell, i};
			}

			std::vector<std::pair<Wire*, Wire*>> cand;
			if (!pairs_file.empty()) {
				std::ifstream f(pairs_file);
				if (!f)
					log_cmd_error("relpred: cannot read %s.\n", pairs_file.c_str());
				std::string a, b;
				while (f >> a >> b) {
					Wire *w1 = module->wire(RTLIL::escape_id(a)), *w2 = module->wire(RTLIL::escape_id(b));
					if (!w1 || !w2)
						log_cmd_error("relpred: %s: no wire %s in %s.\n", pairs_file.c_str(),
								(w1 ? b : a).c_str(), log_id(module));
					cand.push_back({w1, w2});
				}
			} else {
				for (auto w1 : module->wires()) {
					if (!w1->name.isPublic())
						continue;
					std::string n = w1->name.str().substr(1);
					if (n.compare(0, p1.size(), p1) != 0)
						continue;
					Wire *w2 = module->wire(RTLIL::escape_id(p2 + n.substr(p1.size())));
					if (w2)
						cand.push_back({w1, w2});
				}
				std::sort(cand.begin(), cand.end(), [](const std::pair<Wire*, Wire*> &x, const std::pair<Wire*, Wire*> &y) {
					return x.first->name.str() < y.first->name.str();
				});
			}

			int n_added = 0, n_skipped = 0;
			pool<std::pair<SigSpec, SigSpec>> done;
			for (auto &pr : cand) {
				Wire *w1 = pr.first, *w2 = pr.second;
				SigSpec q1 = sigmap(w1), q2 = sigmap(w2);
				if (GetSize(q1) != GetSize(q2) || q1 == q2 || done.count({q1, q2})) {
					n_skipped++;
					continue;
				}
				// every bit a register Q bit of one clock domain
				SigSpec d1, d2;
				Cell *proto = nullptr;
				FfData proto_ff;
				bool ok = true;
				for (int i = 0; i < GetSize(q1) && ok; i++)
					for (int side = 0; side < 2 && ok; side++) {
						SigBit b = side ? q2[i] : q1[i];
						auto it = qbit.find(b);
						if (it == qbit.end()) { ok = false; break; }
						FfData ff(&initvals, it->second.first);
						if (ff.has_ce || ff.has_srst || ff.has_arst || ff.has_sr || ff.has_aload)
							log_cmd_error("relpred: register %s.%s (word %s) has an enable/reset/set input; run `dffunmap` first.\n",
									log_id(module), log_id(it->second.first), log_id(side ? w2 : w1));
						if (!proto) {
							proto = it->second.first;
							proto_ff = ff;
						} else if (ff.has_clk != proto_ff.has_clk || ff.is_fine != proto_ff.is_fine ||
								(ff.has_clk && (sigmap(ff.sig_clk) != sigmap(proto_ff.sig_clk) || ff.pol_clk != proto_ff.pol_clk))) {
							ok = false;
							break;
						}
						(side ? d2 : d1).append(ff.sig_d[it->second.second]);
					}
				if (!ok) {
					n_skipped++;
					continue;
				}
				done.insert({q1, q2});
				bool init_ne = false;
				for (int i = 0; i < GetSize(q1); i++) {
					State a = initvals(q1[i]), b = initvals(q2[i]);
					if (a != State::S1) a = State::S0;
					if (b != State::S1) b = State::S0;
					if (a != b)
						init_ne = true;
				}
				std::string base = w1->name.str().substr(1);
				if (pairs_file.empty())
					base = base.substr(p1.size());
				IdString nname = module->uniquify(RTLIL::escape_id("relpred_neq." + base));
				Wire *neq = module->addWire(nname);
				SigSpec dn = module->Ne(NEW_ID, d1, d2);
				if (proto_ff.has_clk)
					module->addDff(NEW_ID, proto_ff.sig_clk, dn, neq, proto_ff.pol_clk);
				else
					module->addFf(NEW_ID, dn, neq);
				neq->attributes[ID::init] = init_ne ? State::S1 : State::S0;
				neq->set_string_attribute(ID(relpred_pair), w1->name.str() + " " + w2->name.str());
				n_added++;
			}
			log("relpred: %s: %d mismatch predicates added, %d candidate pairs skipped.\n",
					log_id(module), n_added, n_skipped);
		}
	}
} RelpredPass;

PRIVATE_NAMESPACE_END
