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

// read_sdc -- read a Synopsys Design Constraints (SDC) subset
//
// Parses the flat constraint subset commonly emitted by FPGA/ASIC flows
// (create_clock, create_generated_clock, set_clock_groups, set_false_path,
// set_multicycle_path, set_input_delay, set_output_delay, set_max_delay)
// and stores the result on the design scratchpad as JSON ("sdc.json") for
// other passes to consume -- the `cdc` pass pairs clock-group and
// false-path declarations with its crossing findings.
//
// This is intentionally NOT a Tcl interpreter: SDC files are Tcl scripts,
// and upstream yosys has an experimental Tcl-based `sdc` pass (built only
// with YOSYS_ENABLE_TCL) that tracks referenced design objects.  This pass
// covers the regular, non-programmatic constraint style; Tcl control flow,
// variables, and unknown commands are skipped with a warning count.

#include "kernel/yosys.h"
#include "kernel/register.h"
#include "kernel/rtlil.h"
#include "kernel/log.h"
#include "kernel/log_help.h"
#include "libs/json11/json11.hpp"
#include "passes/cmds/sdc_constraints.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

// ---------------- tokenizing ------------------------------------------
// One SDC command per line; # comments; {} and "" group words; [...]
// getter calls are captured whole wherever they appear.

static std::vector<std::string> tokenize(const std::string &line, std::string &err)
{
	std::vector<std::string> out;
	size_t i = 0, n = line.size();
	while (i < n) {
		char ch = line[i];
		if (isspace((unsigned char)ch)) {
			i++;
			continue;
		}
		if (ch == '#')
			break;
		if (ch == '{') {
			size_t j = line.find('}', i + 1);
			if (j == std::string::npos) {
				err = "unmatched '{'";
				return out;
			}
			std::string inner = line.substr(i + 1, j - i - 1);
			// a braced list becomes one token per element
			size_t k = 0;
			while (k < inner.size()) {
				while (k < inner.size() && isspace((unsigned char)inner[k]))
					k++;
				if (k >= inner.size())
					break;
				size_t e = k;
				while (e < inner.size() && !isspace((unsigned char)inner[e]))
					e++;
				out.push_back(inner.substr(k, e - k));
				k = e;
			}
			i = j + 1;
			continue;
		}
		if (ch == '"') {
			size_t j = i + 1;
			std::string tok;
			while (j < n && line[j] != '"') {
				if (line[j] == '\\' && j + 1 < n)
					j++;
				tok += line[j++];
			}
			if (j >= n) {
				err = "unmatched '\"'";
				return out;
			}
			out.push_back(tok);
			i = j + 1;
			continue;
		}
		if (ch == '[') {
			// capture the whole bracketed getter call as one token
			int depth = 1;
			size_t j = i + 1;
			while (j < n && depth) {
				if (line[j] == '[')
					depth++;
				else if (line[j] == ']')
					depth--;
				j++;
			}
			if (depth) {
				err = "unmatched '['";
				return out;
			}
			out.push_back(line.substr(i, j - i));
			i = j;
			continue;
		}
		size_t j = i;
		while (j < n && !isspace((unsigned char)line[j]) &&
				!strchr("#{}\"[", line[j]))
			j++;
		if (j == i) {
			err = stringf("unexpected character '%c'", ch);
			return out;
		}
		out.push_back(line.substr(i, j - i));
		i = j;
	}
	return out;
}

// [get_ports pat] / [get_pins pat] / [get_clocks pat] / [all_clocks] ...
// -> typed target; a bare name in a target position is a port.
static SdcTarget getter_target(const std::string &tok, bool &ok)
{
	SdcTarget t;
	ok = true;
	if (tok.size() < 2 || tok.front() != '[' || tok.back() != ']') {
		t.kind = "port";
		t.pattern = tok;
		t.wildcard = t.pattern.find('*') != std::string::npos ||
				t.pattern.find('?') != std::string::npos;
		return t;
	}
	std::string inner = tok.substr(1, tok.size() - 2);
	std::istringstream iss(inner);
	std::string cmd, pat;
	iss >> cmd >> pat;
	if (cmd == "get_ports" || cmd == "get_pins" || cmd == "get_clocks") {
		t.kind = cmd.substr(4, cmd.size() - 5);	// port/pin/clock
		t.pattern = pat;
	} else if (cmd == "all_clocks") {
		t.kind = "clock";
		t.pattern = "*";
	} else if (cmd == "all_inputs" || cmd == "all_outputs") {
		t.kind = "port";
		t.pattern = "*";
	} else {
		// e.g. [get_nets ...] or an expression: kept raw; nothing in
		// the v1 consumers matches these
		t.kind = "expr";
		t.pattern = tok;
		ok = false;
	}
	t.wildcard = t.pattern.find('*') != std::string::npos ||
			t.pattern.find('?') != std::string::npos;
	return t;
}

// ---------------- command interpretation -------------------------------

struct SdcParser {
	SdcConstraints &c;

	bool is_flag(const std::string &s) { return !s.empty() && s[0] == '-'; }

	double to_double(const std::string &s, bool &ok) {
		char *end = nullptr;
		double v = strtod(s.c_str(), &end);
		ok = end && *end == '\0' && end != s.c_str();
		return v;
	}
	int to_int(const std::string &s, bool &ok) {
		char *end = nullptr;
		long v = strtol(s.c_str(), &end, 10);
		ok = end && *end == '\0' && end != s.c_str();
		return (int)v;
	}

	void cmd_create_clock(std::vector<std::string> args, bool generated)
	{
		SdcClock clk;
		clk.generated = generated;
		std::vector<std::string> positional;
		for (size_t i = 0; i < args.size(); i++) {
			std::string &a = args[i];
			auto next = [&](const char *flag) -> std::string {
				if (i + 1 >= args.size()) {
					log_warning("read_sdc: %s missing value for %s\n",
							generated ? "create_generated_clock" : "create_clock", flag);
					return "";
				}
				return args[++i];
			};
			bool ok;
			if (a == "-period") {
				clk.period = to_double(next("-period"), ok);
				clk.period_set = ok;
			} else if (a == "-name") {
				clk.name = next("-name");
			} else if (a == "-waveform") {
				// two following values {rise fall}
				if (i + 2 < args.size()) {
					clk.waveform.push_back(to_double(args[i + 1], ok));
					clk.waveform.push_back(to_double(args[i + 2], ok));
					i += 2;
				} else
					log_warning("read_sdc: -waveform needs two values\n");
			} else if (a == "-divide_by") {
				clk.divide_by = to_int(next("-divide_by"), ok);
			} else if (a == "-multiply_by") {
				clk.multiply_by = to_int(next("-multiply_by"), ok);
			} else if (a == "-invert") {
				clk.invert = true;
			} else if (a == "-source") {
				bool gok;
				clk.gen_source = getter_target(next("-source"), gok).pattern;
			} else if (a == "-add") {
				// merging clocks on one target: recorded as-is; the
				// consumer sees both clock objects
			} else if (a == "-comment") {
				next("-comment");
			} else if (is_flag(a)) {
				log_warning("read_sdc: %s ignoring unknown option %s\n",
						generated ? "create_generated_clock" : "create_clock", a.c_str());
			} else
				positional.push_back(a);
		}
		if (!generated && !clk.period_set)
			log_warning("read_sdc: create_clock without -period ignored\n");
		else if (generated && clk.gen_source.empty())
			log_warning("read_sdc: create_generated_clock without -source ignored\n");
		else {
			for (auto &p : positional) {
				bool ok;
				SdcTarget t = getter_target(p, ok);
				if (!ok)
					log_warning("read_sdc: unsupported create_clock target %s\n", p.c_str());
				else
					clk.targets.push_back(t);
			}
			if (clk.name.empty() && !clk.targets.empty())
				clk.name = clk.targets.front().pattern;
			c.clocks.push_back(clk);
		}
	}

	void cmd_clock_groups(std::vector<std::string> args)
	{
		SdcClockGroup g;
		for (size_t i = 0; i < args.size(); i++) {
			std::string &a = args[i];
			if (a == "-group") {
				std::vector<SdcTarget> grp;
				while (i + 1 < args.size() && !is_flag(args[i + 1])) {
					bool ok;
					SdcTarget t = getter_target(args[++i], ok);
					if (!ok)
						log_warning("read_sdc: unsupported -group member %s\n", args[i].c_str());
					else
						grp.push_back(t);
				}
				g.groups.push_back(grp);
			} else if (a == "-asynchronous")
				g.asynchronous = true;
			else if (a == "-logically_exclusive")
				g.logically_exclusive = true;
			else if (a == "-physically_exclusive")
				g.physically_exclusive = true;
			else if (a == "-name" || a == "-comment") {
				if (i + 1 < args.size())
					i++;
			} else if (a == "-allow_paths") {
				// boolean: nothing to consume
			} else
				log_warning("read_sdc: set_clock_groups ignoring unknown option %s\n", a.c_str());
		}
		if (g.groups.size() < 2)
			log_warning("read_sdc: set_clock_groups with fewer than 2 groups ignored\n");
		else
			c.clock_groups.push_back(g);
	}

	void cmd_path_exception(std::vector<std::string> args, const std::string &kind)
	{
		SdcPathExc p;
		p.kind = kind;
		for (size_t i = 0; i < args.size(); i++) {
			std::string &a = args[i];
			auto take = [&](std::vector<SdcTarget> &dst) {
				if (i + 1 >= args.size()) {
					log_warning("read_sdc: %s missing value after %s\n", kind.c_str(), a.c_str());
					return;
				}
				bool ok;
				SdcTarget t = getter_target(args[++i], ok);
				if (ok)
					dst.push_back(t);
			};
			// v1 treats rise/fall-qualified endpoints the same
			if (a == "-from" || a == "-fall_from" || a == "-rise_from")
				take(p.from);
			else if (a == "-to" || a == "-fall_to" || a == "-rise_to")
				take(p.to);
			else if (a == "-through" || a == "-fall_through" || a == "-rise_through")
				take(p.through);
			else if (a == "-setup")
				p.setup = true;
			else if (a == "-hold")
				p.hold = true;
			else if (a == "-reset_path" || a == "-comment") {
				if (a == "-comment" && i + 1 < args.size())
					i++;
			} else if (is_flag(a)) {
				log_warning("read_sdc: %s ignoring unknown option %s\n", kind.c_str(), a.c_str());
			} else if (kind == "multicycle_path" && !p.cycles) {
				bool ok;
				p.cycles = to_int(a, ok);
				if (!ok)
					p.cycles = 0;
			}
		}
		c.path_exceptions.push_back(p);
	}

	void cmd_io_delay(std::vector<std::string> args, bool output)
	{
		SdcIoDelay d;
		d.output = output;
		for (size_t i = 0; i < args.size(); i++) {
			std::string &a = args[i];
			bool ok;
			if (a == "-clock") {
				if (i + 1 < args.size())
					d.clock = getter_target(args[++i], ok).pattern;
			} else if (a == "-clock_fall") {
				d.clock_fall = true;
			} else if (a == "-add_delay") {
				d.add_delay = true;
			} else if (a == "-reference_pin" || a == "-comment") {
				if (i + 1 < args.size())
					i++;
			} else if (a == "-max" || a == "-min") {
				if (i + 1 < args.size()) {
					double v = to_double(args[i + 1], ok);
					if (ok) {
						i++;
						if (a == "-max") {
							d.max = v;
							d.has_max = true;
						} else {
							d.min = v;
							d.has_min = true;
						}
					}
				}
			} else if (a == "-network_latency_included" || a == "-source_latency_included") {
				// booleans: nothing to consume
			} else if (is_flag(a)) {
				log_warning("read_sdc: %s ignoring unknown option %s\n",
						output ? "set_output_delay" : "set_input_delay", a.c_str());
			} else {
				double v = to_double(a, ok);
				if (ok && !d.has_max && !d.has_min) {
					d.max = v;
					d.has_max = true;
				} else {
					SdcTarget t = getter_target(a, ok);
					if (ok)
						d.ports.push_back(t);
				}
			}
		}
		c.io_delays.push_back(d);
	}

	void cmd_max_delay(std::vector<std::string> args)
	{
		SdcMaxDelay m;
		for (size_t i = 0; i < args.size(); i++) {
			std::string &a = args[i];
			bool ok;
			if ((a == "-from" || a == "-fall_from" || a == "-rise_from") && i + 1 < args.size()) {
				SdcTarget t = getter_target(args[++i], ok);
				if (ok)
					m.from.push_back(t);
			} else if ((a == "-to" || a == "-fall_to" || a == "-rise_to") && i + 1 < args.size()) {
				SdcTarget t = getter_target(args[++i], ok);
				if (ok)
					m.to.push_back(t);
			} else if (a == "-through" && i + 1 < args.size()) {
				i++;	// v1 ignores -through on max_delay
			} else if (!is_flag(a)) {
				double v = to_double(a, ok);
				if (ok && m.delay == 0.0)
					m.delay = v;
			}
		}
		c.max_delays.push_back(m);
	}

	void parse_line(const std::vector<std::string> &toks)
	{
		if (toks.empty())
			return;
		const std::string &cmd = toks[0];
		std::vector<std::string> args(toks.begin() + 1, toks.end());
		if (cmd == "create_clock")
			cmd_create_clock(args, false);
		else if (cmd == "create_generated_clock")
			cmd_create_clock(args, true);
		else if (cmd == "set_clock_groups")
			cmd_clock_groups(args);
		else if (cmd == "set_false_path")
			cmd_path_exception(args, "false_path");
		else if (cmd == "set_multicycle_path")
			cmd_path_exception(args, "multicycle_path");
		else if (cmd == "set_input_delay")
			cmd_io_delay(args, false);
		else if (cmd == "set_output_delay")
			cmd_io_delay(args, true);
		else if (cmd == "set_max_delay")
			cmd_max_delay(args);
		else {
			// set_clock_latency, set_clock_uncertainty,
			// set_disable_timing, current_instance,
			// set_case_analysis, ...: out of the v1 subset
			c.ignored++;
			log_warning("read_sdc: ignoring unsupported command '%s'\n", cmd.c_str());
		}
	}
};

// ---------------- scratchpad serialization ------------------------------

static json11::Json target_json(const SdcTarget &t)
{
	return json11::Json::object{{"kind", t.kind}, {"pattern", t.pattern},
			{"wildcard", t.wildcard}};
}

static json11::Json targets_json(const std::vector<SdcTarget> &v)
{
	json11::Json::array a;
	for (auto &t : v)
		a.push_back(target_json(t));
	return a;
}

static std::string constraints_to_json(const SdcConstraints &c)
{
	json11::Json::array clocks;
	for (auto &k : c.clocks) {
		json11::Json::array wf;
		for (double x : k.waveform)
			wf.push_back(x);
		clocks.push_back(json11::Json::object{
				{"name", k.name}, {"period", k.period},
				{"period_set", k.period_set}, {"waveform", wf},
				{"targets", targets_json(k.targets)},
				{"generated", k.generated},
				{"source", k.gen_source}, {"divide_by", k.divide_by},
				{"multiply_by", k.multiply_by}, {"invert", k.invert}});
	}
	json11::Json::array groups;
	for (auto &g : c.clock_groups) {
		json11::Json::array grps;
		for (auto &grp : g.groups)
			grps.push_back(targets_json(grp));
		groups.push_back(json11::Json::object{
				{"groups", grps}, {"asynchronous", g.asynchronous},
				{"logically_exclusive", g.logically_exclusive},
				{"physically_exclusive", g.physically_exclusive}});
	}
	json11::Json::array excs;
	for (auto &p : c.path_exceptions)
		excs.push_back(json11::Json::object{
				{"kind", p.kind}, {"from", targets_json(p.from)},
				{"to", targets_json(p.to)}, {"through", targets_json(p.through)},
				{"cycles", p.cycles}, {"setup", p.setup}, {"hold", p.hold}});
	json11::Json::array ios;
	for (auto &d : c.io_delays)
		ios.push_back(json11::Json::object{
				{"output", d.output}, {"ports", targets_json(d.ports)},
				{"clock", d.clock}, {"clock_fall", d.clock_fall},
				{"add_delay", d.add_delay}, {"max", d.max}, {"min", d.min},
				{"has_max", d.has_max}, {"has_min", d.has_min}});
	json11::Json::array maxds;
	for (auto &m : c.max_delays)
		maxds.push_back(json11::Json::object{
				{"from", targets_json(m.from)}, {"to", targets_json(m.to)},
				{"delay", m.delay}});
	return json11::Json(json11::Json::object{
			{"clocks", clocks}, {"clock_groups", groups},
			{"path_exceptions", excs}, {"io_delays", ios},
			{"max_delays", maxds}, {"ignored", c.ignored}}).dump();
}

static void dump_constraints(const SdcConstraints &c)
{
	for (auto &k : c.clocks)
		log("clock %s period %.3f%s\n", k.name.c_str(), k.period,
				k.generated ? " generated" : "");
	for (auto &g : c.clock_groups) {
		std::string grps;
		for (size_t i = 0; i < g.groups.size(); i++) {
			if (i)
				grps += " vs ";
			for (size_t j = 0; j < g.groups[i].size(); j++) {
				if (j)
					grps += ",";
				grps += g.groups[i][j].pattern;
			}
		}
		log("clock-groups %s: %s\n", g.asynchronous ? "async" : "exclusive", grps.c_str());
	}
	for (auto &p : c.path_exceptions)
		log("%s%s %s -> %s%s\n", p.kind.c_str(),
				p.cycles ? stringf(" %d", p.cycles).c_str() : "",
				p.from.empty() ? "*" : p.from.front().pattern.c_str(),
				p.to.empty() ? "*" : p.to.front().pattern.c_str(),
				(p.setup || p.hold) ? (p.setup ? " -setup" : " -hold") : "");
	for (auto &d : c.io_delays)
		log("io-delay %s port:%s clock %s max %.3f%s\n", d.output ? "output" : "input",
				d.ports.empty() ? "?" : d.ports.front().pattern.c_str(),
				d.clock.c_str(), d.max,
				d.has_min ? stringf(" min %.3f", d.min).c_str() : "");
	for (auto &m : c.max_delays)
		log("max-delay %.3f %s -> %s\n", m.delay,
				m.from.empty() ? "*" : m.from.front().pattern.c_str(),
				m.to.empty() ? "*" : m.to.front().pattern.c_str());
	log("ignored commands: %d\n", c.ignored);
}

struct ReadSdcPass : public Pass {
	ReadSdcPass() : Pass("read_sdc", "read a Synopsys Design Constraints (SDC) subset") { }
	bool formatted_help() override {
		auto *help = PrettyHelp::get_current();
		help->set_group("passes/io");
		return false;
	}
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    read_sdc [options] <file>\n");
		log("\n");
		log("Read a Synopsys Design Constraints file and store the parsed constraints on\n");
		log("the design scratchpad (key \"sdc.json\") for other passes to consume.  This\n");
		log("is a flat-subset parser, not a Tcl interpreter: it covers the regular,\n");
		log("non-programmatic constraint style and skips Tcl control flow, variables,\n");
		log("and unsupported commands (counted in the summary).  When built with Tcl\n");
		log("support, the experimental `sdc` pass provides full Tcl evaluation with\n");
		log("design-object tracking instead.\n");
		log("\n");
		log("Supported commands:\n");
		log("  create_clock, create_generated_clock, set_clock_groups,\n");
		log("  set_false_path, set_multicycle_path, set_input_delay,\n");
		log("  set_output_delay, set_max_delay\n");
		log("\n");
		log("Getter syntax [get_ports a], [get_pins u_ff/Q], [get_clocks clk*],\n");
		log("[all_clocks], [all_inputs], [all_outputs] is recognized in target\n");
		log("positions.\n");
		log("\n");
		log("The `cdc` pass consumes the stored constraints: crossings between clocks\n");
		log("declared asynchronous (set_clock_groups -asynchronous) or false-pathed\n");
		log("are paired with the declaration (see the cdc pass help).\n");
		log("\n");
		log("    -dump\n");
		log("        print the parsed constraints and do not store them\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing READ_SDC pass (SDC subset parser).\n");
		bool dump = false;
		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-dump") {
				dump = true;
				continue;
			}
			break;
		}
		if (argidx >= args.size())
			log_cmd_error("Missing SDC file.\n");
		std::string path = args[argidx++];
		extra_args(args, argidx, design);

		std::ifstream in(path);
		if (!in)
			log_cmd_error("Can't open SDC file `%s'.\n", path.c_str());

		SdcConstraints c;
		if (!dump)
			sdc_load_constraints(design, c);	// merge with earlier reads
		std::string line, logical;
		int lineno = 0, first_lineno = 0;
		while (std::getline(in, line)) {
			lineno++;
			// Tcl continuation: a line ending in backslash joins the
			// next one (common in generated SDC)
			if (logical.empty())
				first_lineno = lineno;
			std::string cur = line;
			while (!cur.empty() && (cur.back() == '\r' || cur.back() == '\n'))
				cur.pop_back();
			bool cont = !cur.empty() && cur.back() == '\\';
			if (cont)
				cur.pop_back();
			logical += cur;
			if (cont)
				continue;
			std::string err;
			std::vector<std::string> toks = tokenize(logical, err);
			if (!err.empty())
				log_warning("read_sdc: %s:%d: %s\n", path.c_str(), first_lineno, err.c_str());
			SdcParser parser{c};
			parser.parse_line(toks);
			logical.clear();
		}
		if (!logical.empty()) {
			std::string err;
			std::vector<std::string> toks = tokenize(logical, err);
			if (!err.empty())
				log_warning("read_sdc: %s:%d: %s\n", path.c_str(), first_lineno, err.c_str());
			SdcParser parser{c};
			parser.parse_line(toks);
		}
		if (dump) {
			dump_constraints(c);
		} else {
			design->scratchpad["sdc.json"] = constraints_to_json(c);
			log("Stored %d clocks, %d clock groups, %d path exceptions, "
					"%d io delays, %d max delays (%d commands ignored) from `%s'.\n",
					GetSize(c.clocks), GetSize(c.clock_groups),
					GetSize(c.path_exceptions), GetSize(c.io_delays),
					GetSize(c.max_delays), c.ignored, path.c_str());
		}
	}
} ReadSdcPass;

PRIVATE_NAMESPACE_END

// scratchpad loader shared with consumers (declared in sdc_constraints.h)
YOSYS_NAMESPACE_BEGIN
bool sdc_load_constraints(Design *design, SdcConstraints &out)
{
	auto it = design->scratchpad.find("sdc.json");
	if (it == design->scratchpad.end())
		return false;
	std::string err;
	json11::Json doc = json11::Json::parse(it->second, err);
	if (!err.empty()) {
		log_warning("sdc: could not parse sdc.json scratchpad entry: %s\n", err.c_str());
		return false;
	}
	out.ignored = doc["ignored"].int_value();
	auto targets_from_json = [](const json11::Json &j) {
		std::vector<SdcTarget> v;
		for (auto &e : j.array_items()) {
			SdcTarget t;
			t.kind = e["kind"].string_value();
			t.pattern = e["pattern"].string_value();
			t.wildcard = e["wildcard"].bool_value();
			v.push_back(t);
		}
		return v;
	};
	for (auto &k : doc["clocks"].array_items()) {
		SdcClock clk;
		clk.name = k["name"].string_value();
		clk.period = k["period"].number_value();
		clk.period_set = k["period_set"].bool_value();
		for (auto &x : k["waveform"].array_items())
			clk.waveform.push_back(x.number_value());
		clk.targets = targets_from_json(k["targets"]);
		clk.generated = k["generated"].bool_value();
		clk.gen_source = k["source"].string_value();
		clk.divide_by = k["divide_by"].int_value();
		clk.multiply_by = k["multiply_by"].int_value();
		clk.invert = k["invert"].bool_value();
		out.clocks.push_back(clk);
	}
	for (auto &g : doc["clock_groups"].array_items()) {
		SdcClockGroup grp;
		for (auto &gj : g["groups"].array_items())
			grp.groups.push_back(targets_from_json(gj));
		grp.asynchronous = g["asynchronous"].bool_value();
		grp.logically_exclusive = g["logically_exclusive"].bool_value();
		grp.physically_exclusive = g["physically_exclusive"].bool_value();
		out.clock_groups.push_back(grp);
	}
	for (auto &p : doc["path_exceptions"].array_items()) {
		SdcPathExc e;
		e.kind = p["kind"].string_value();
		e.from = targets_from_json(p["from"]);
		e.to = targets_from_json(p["to"]);
		e.through = targets_from_json(p["through"]);
		e.cycles = p["cycles"].int_value();
		e.setup = p["setup"].bool_value();
		e.hold = p["hold"].bool_value();
		out.path_exceptions.push_back(e);
	}
	for (auto &d : doc["io_delays"].array_items()) {
		SdcIoDelay io;
		io.output = d["output"].bool_value();
		io.ports = targets_from_json(d["ports"]);
		io.clock = d["clock"].string_value();
		io.clock_fall = d["clock_fall"].bool_value();
		io.add_delay = d["add_delay"].bool_value();
		io.max = d["max"].number_value();
		io.min = d["min"].number_value();
		io.has_max = d["has_max"].bool_value();
		io.has_min = d["has_min"].bool_value();
		out.io_delays.push_back(io);
	}
	for (auto &m : doc["max_delays"].array_items()) {
		SdcMaxDelay md;
		md.from = targets_from_json(m["from"]);
		md.to = targets_from_json(m["to"]);
		md.delay = m["delay"].number_value();
		out.max_delays.push_back(md);
	}
	return true;
}
YOSYS_NAMESPACE_END
