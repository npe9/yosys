/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2026  the sv-yosys project contributors
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

// Shared contract between the read_sdc pass (producer) and consumers
// such as the cdc pass: the parsed constraint subset is serialized to
// the design scratchpad under "sdc.json"; sdc_load_constraints parses
// it back.  See passes/cmds/read_sdc.cc.

#ifndef SDC_CONSTRAINTS_H
#define SDC_CONSTRAINTS_H

#include "kernel/rtlil.h"
#include <string>
#include <vector>

YOSYS_NAMESPACE_BEGIN

struct SdcTarget {
	std::string kind;	// "port" | "pin" | "clock"
	std::string pattern;
	bool wildcard = false;
};

struct SdcClock {
	std::string name;
	double period = 0.0;
	bool period_set = false;
	std::vector<double> waveform;
	std::vector<SdcTarget> targets;
	bool generated = false;
	std::string gen_source;
	int divide_by = 1, multiply_by = 1;
	bool invert = false;
};

struct SdcClockGroup {
	std::vector<std::vector<SdcTarget>> groups;
	bool asynchronous = false, logically_exclusive = false,
			physically_exclusive = false;
};

struct SdcPathExc {
	std::string kind;	// "false_path" | "multicycle_path"
	std::vector<SdcTarget> from, to, through;
	int cycles = 0;
	bool setup = false, hold = false;
};

struct SdcIoDelay {
	bool output = false;	// set_output_delay vs set_input_delay
	std::vector<SdcTarget> ports;
	std::string clock;
	bool clock_fall = false, add_delay = false;
	double max = 0.0, min = 0.0;
	bool has_max = false, has_min = false;
};

struct SdcMaxDelay {
	std::vector<SdcTarget> from, to;
	double delay = 0.0;
};

struct SdcConstraints {
	std::vector<SdcClock> clocks;
	std::vector<SdcClockGroup> clock_groups;
	std::vector<SdcPathExc> path_exceptions;
	std::vector<SdcIoDelay> io_delays;
	std::vector<SdcMaxDelay> max_delays;
	int ignored = 0;
};

bool sdc_load_constraints(Design *design, SdcConstraints &out);

YOSYS_NAMESPACE_END

#endif
