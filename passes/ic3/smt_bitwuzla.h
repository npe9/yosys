/*
 *  ic3 -engine bitwuzla: in-process word-level BMC / k-induction on
 *  Bitwuzla's C API (register E8); see smt_bitwuzla.cc.
 */

#ifndef IC3_SMT_BITWUZLA_H
#define IC3_SMT_BITWUZLA_H

#include "kernel/yosys.h"

YOSYS_NAMESPACE_BEGIN

struct SmtBitwuzlaOptions {
	bool init_zero = false;
	int depth_max = 20;		// last step unrolled (k-induction depth bound)
	double timeout_s = 0;	// per check-sat (0 = none)
	double total_s = 0;		// whole run (0 = none)
};

struct SmtBitwuzlaResult {
	// 0 proved (k-induction at depth), 1 disproved (cex at step depth),
	// 2 solver budget exhausted at depth, 3 open after depth_max, -1 not run
	int verdict = -1;
	int depth = 0;
};

// checks: asserts and covers (a cover is solved as the assert of !A: a
// disproof is a reach); returns "" or why the engine could not run
std::string smt_bitwuzla_prove(RTLIL::Design *design, RTLIL::Module *module,
		const std::vector<RTLIL::Cell*> &checks, const pool<RTLIL::Cell*> &covers,
		const SmtBitwuzlaOptions &opt, std::vector<SmtBitwuzlaResult> &results, std::string &stats);

YOSYS_NAMESPACE_END

#endif
