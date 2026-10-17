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
 *  ---
 *
 *  This is the AST frontend library.
 *
 *  The AST frontend library is not a frontend on it's own but provides a
 *  generic abstract syntax tree (AST) abstraction for HDL code and can be
 *  used by HDL frontends. See "ast.h" for an overview of the API and the
 *  Verilog frontend for an usage example.
 *
 */

#include "kernel/log.h"
#include "kernel/utils.h"
#include "libs/sha1/sha1.h"
#include "ast.h"

#include <sstream>
#include <stdarg.h>
#include <algorithm>

YOSYS_NAMESPACE_BEGIN

using namespace AST;
using namespace AST_INTERNAL;

// helper function for creating RTLIL code for unary operations
// `foo.triggered` / `foo.matched` parse as a hierarchical identifier;
// when the stem names a sequence declaration (checked in simplify) the
// method wire is lowered by the sva2check pass, so the autowire path
// stays quiet for it.
// Register v3 D11: a plain operator (`&&`, `||`, `!`, `?:`, arithmetic,
// comparison, bitwise) over a sequence/property expression is not
// SystemVerilog (IEEE 1800-2017 16.12 gives properties their own
// and/or/not).  The frontend used to lower the property operand to a
// 1-bit placeholder and the engines produced a verdict on it.
// Boolean-valued SVA leaves (`inside`, `weq`, `wne`, `meq`, `true`, match
// items) stay allowed.
// An $sva node is Boolean-valued when it is a Boolean leaf, or the
// Boolean `<->` / `->` operators (IEEE 1800-2012 11.4.7, which the parser
// shares with the property `iff` / `implies`) over Boolean-valued operands.
static bool sva_is_boolean(const AstNode *n)
{
	static const std::set<std::string> boolean_leaves = {"inside", "weq", "wne", "meq", "true", "assign", "set"};
	if (n->type != AST_SVA)
		return true;
	if (boolean_leaves.count(n->str))
		return true;
	if (n->str == "iff" || n->str == "implies") {
		for (auto &c : n->children)
			if (c && !sva_is_boolean(c.get()))
				return false;
		return true;
	}
	return false;
}

static void refuse_sva_operand(const AstNode *op)
{
	for (auto &c : op->children)
		if (c && c->type == AST_SVA && !sva_is_boolean(c.get()))
			op->input_error("Boolean operator over a sequence/property expression (`%s`): use the property "
					"operators and/or/not, or parenthesize a Boolean.\n", c->str.c_str());
}

// SVA local variables assigned in match items, per module (see AST_SVA)
static dict<RTLIL::Module *, pool<std::string>> sva_local_names;

static bool is_sequence_method_name(const std::string &str)
{
	for (const char *suf : {".triggered", ".matched"}) {
		size_t sl = strlen(suf);
		if (str.size() > sl && str.compare(str.size() - sl, sl, suf) == 0)
			return true;
	}
	return false;
}

static RTLIL::SigSpec uniop2rtlil(AstNode *that, IdString type, int result_width, const RTLIL::SigSpec &arg, bool gen_attributes = true)
{
	IdString name = stringf("%s$%s:%d$%d", type, RTLIL::encode_filename(*that->location.begin.filename), that->location.begin.line, autoidx++);
	RTLIL::Cell *cell = current_module->addCell(name, type);
	set_src_attr(cell, that);

	RTLIL::Wire *wire = current_module->addWire(cell->name.str() + "_Y", result_width);
	set_src_attr(wire, that);
	wire->is_signed = that->is_signed;

	if (gen_attributes)
		for (auto &attr : that->attributes) {
			if (attr.second->type != AST_CONSTANT)
				that->input_error("Attribute `%s' with non-constant value!\n", attr.first);
			cell->attributes[attr.first] = attr.second->asAttrConst();
		}

	cell->parameters[ID::A_SIGNED] = RTLIL::Const(that->children[0]->is_signed);
	cell->parameters[ID::A_WIDTH] = RTLIL::Const(arg.size());
	cell->setPort(ID::A, arg);

	cell->parameters[ID::Y_WIDTH] = result_width;
	cell->setPort(ID::Y, wire);
	return wire;
}

// helper function for extending bit width (preferred over SigSpec::extend() because of correct undef propagation in ConstEval)
static void widthExtend(AstNode *that, RTLIL::SigSpec &sig, int width, bool is_signed)
{
	if (width <= sig.size()) {
		sig.extend_u0(width, is_signed);
		return;
	}

	IdString name = stringf("$extend$%s:%d$%d", RTLIL::encode_filename(*that->location.begin.filename), that->location.begin.line, autoidx++);
	RTLIL::Cell *cell = current_module->addCell(name, ID($pos));
	set_src_attr(cell, that);

	RTLIL::Wire *wire = current_module->addWire(cell->name.str() + "_Y", width);
	set_src_attr(wire, that);
	wire->is_signed = that->is_signed;

	if (that != nullptr)
		for (auto &attr : that->attributes) {
			if (attr.second->type != AST_CONSTANT)
				that->input_error("Attribute `%s' with non-constant value!\n", attr.first);
			cell->attributes[attr.first] = attr.second->asAttrConst();
		}

	cell->parameters[ID::A_SIGNED] = RTLIL::Const(is_signed);
	cell->parameters[ID::A_WIDTH] = RTLIL::Const(sig.size());
	cell->setPort(ID::A, sig);

	cell->parameters[ID::Y_WIDTH] = width;
	cell->setPort(ID::Y, wire);
	sig = wire;
}

// helper function for creating RTLIL code for binary operations
static RTLIL::SigSpec binop2rtlil(AstNode *that, IdString type, int result_width, const RTLIL::SigSpec &left, const RTLIL::SigSpec &right)
{
	IdString name = stringf("%s$%s:%d$%d", type, RTLIL::encode_filename(*that->location.begin.filename), that->location.begin.line, autoidx++);
	RTLIL::Cell *cell = current_module->addCell(name, type);
	set_src_attr(cell, that);

	RTLIL::Wire *wire = current_module->addWire(cell->name.str() + "_Y", result_width);
	set_src_attr(wire, that);
	wire->is_signed = that->is_signed;

	for (auto &attr : that->attributes) {
		if (attr.second->type != AST_CONSTANT)
			that->input_error("Attribute `%s' with non-constant value!\n", attr.first);
		cell->attributes[attr.first] = attr.second->asAttrConst();
	}

	// Operand signedness.  A fork revision derived ONE shared sign for
	// arithmetic/comparison cells (matched_sign) and forced both shift
	// operands unsigned.  The shift half was wrong in two ways: $shift/
	// $shiftx take a SIGNED offset (negative = shift left), so forcing B
	// unsigned turned the negated offset of every dynamic part-select
	// write into a huge count and the write vanished (tests/simple
	// dynslice/partsel/sign_part_assign/memory, sat/counters-repeat,
	// various/dynamic_part_select); and a logical shift must still
	// sign-extend a signed A to the result width before shifting
	// (s1 >> s2 with s1 = -2 gave 3, not 63; tests/simple/operators.v).
	// Shifts therefore follow upstream (per-operand).  The matched_sign
	// rule for the remaining cells is kept for now: switching those to
	// per-operand signs as upstream does crashes the frontend (stack
	// overflow) on axi4-fvip/AXI4_STREAM and caliptra fv_montmultiplier
	// -- see PLAN.md 2026-10-16, open item.
	bool a_signed, b_signed;
	if (type == ID($shl) || type == ID($shr) || type == ID($sshl) || type == ID($sshr)) {
		a_signed = that->children[0]->is_signed;
		b_signed = false;
	} else if (type == ID($shift) || type == ID($shiftx)) {
		a_signed = that->children[0]->is_signed;
		b_signed = that->children[1]->is_signed;
	} else {
		bool matched_sign = that->children[0]->is_signed && that->children[1]->is_signed;
		a_signed = b_signed = matched_sign;
	}
	cell->parameters[ID::A_SIGNED] = RTLIL::Const(a_signed);
	cell->parameters[ID::B_SIGNED] = RTLIL::Const(b_signed);

	cell->parameters[ID::A_WIDTH] = RTLIL::Const(left.size());
	cell->parameters[ID::B_WIDTH] = RTLIL::Const(right.size());

	cell->setPort(ID::A, left);
	cell->setPort(ID::B, right);

	cell->parameters[ID::Y_WIDTH] = result_width;
	cell->setPort(ID::Y, wire);
	return wire;
}

// helper function for creating RTLIL code for multiplexers
static RTLIL::SigSpec mux2rtlil(AstNode *that, const RTLIL::SigSpec &cond, const RTLIL::SigSpec &left, const RTLIL::SigSpec &right)
{
	log_assert(cond.size() == 1);

	std::stringstream sstr;
	sstr << "$ternary$" << RTLIL::encode_filename(*that->location.begin.filename) << ":" << that->location.begin.line << "$" << (autoidx++);

	RTLIL::Cell *cell = current_module->addCell(sstr.str(), ID($mux));
	set_src_attr(cell, that);

	RTLIL::Wire *wire = current_module->addWire(cell->name.str() + "_Y", left.size());
	set_src_attr(wire, that);
	wire->is_signed = that->is_signed;

	for (auto &attr : that->attributes) {
		if (attr.second->type != AST_CONSTANT)
			that->input_error("Attribute `%s' with non-constant value!\n", attr.first);
		cell->attributes[attr.first] = attr.second->asAttrConst();
	}

	cell->parameters[ID::WIDTH] = RTLIL::Const(left.size());

	cell->setPort(ID::A, right);
	cell->setPort(ID::B, left);
	cell->setPort(ID::S, cond);
	cell->setPort(ID::Y, wire);

	return wire;
}

static void check_unique_id(RTLIL::Module *module, RTLIL::IdString &id,
		const AstNode *node, const char *to_add_kind)
{
	bool clash = module->wire(id) || module->cell(id) || module->processes.count(id) || module->memories.count(id);
	if (!clash)
		return;
	if (node->attributes.count(IdString("\\checker_instance")))
		log("Checker instance copy of %s `%s' renamed.\n", to_add_kind, id.c_str());
	else if (!AST::flag_permissive)
		// upstream's wording (tests/verilog/conflict_*.ys)
		log_file_error(node->location.to_loc(), "Cannot add %s `%s' because a %s with the same name was already created.\n", to_add_kind, id.c_str(),
				module->memories.count(id) ? "memory" : module->cell(id) ? "cell" : "signal");
	else
		degrade(node->location.to_loc(), "Duplicate %s `%s'; renamed.\n", to_add_kind, id.c_str());
	id = stringf("%s$%d", id.c_str(), autoidx++);
}

// helper class for rewriting simple lookahead references in AST always blocks
struct AST_INTERNAL::LookaheadRewriter
{
	dict<IdString, pair<AstNode*, AstNode*>> lookaheadids;

	void collect_lookaheadids(AstNode *node)
	{
		if (node->lookahead) {
			log_assert(node->type == AST_IDENTIFIER);
			if (!lookaheadids.count(node->str)) {
				auto wire = std::make_unique<AstNode>(node->location, AST_WIRE);
				for (auto& c : node->id2ast->children)
					wire->children.push_back(c->clone());
				wire->fixup_hierarchy_flags();
				wire->str = stringf("$lookahead%s$%d", node->str, autoidx++);
				wire->set_attribute(ID::nosync, AstNode::mkconst_int(node->location, 1, false));
				wire->is_logic = true;
				while (wire->simplify(true, 1, -1, false)) { }
				lookaheadids[node->str] = make_pair(node->id2ast, wire.get());
				wire->genRTLIL();
				current_ast_mod->children.push_back(std::move(wire));
			}
		}

		for (auto& child : node->children)
			collect_lookaheadids(child.get());
	}

	bool has_lookaheadids(AstNode *node)
	{
		if (node->type == AST_IDENTIFIER && lookaheadids.count(node->str) != 0)
			return true;

		for (auto& child : node->children)
			if (has_lookaheadids(child.get()))
				return true;

		return false;
	}

	bool has_nonlookaheadids(AstNode *node)
	{
		if (node->type == AST_IDENTIFIER && lookaheadids.count(node->str) == 0)
			return true;

		for (auto& child : node->children)
			if (has_nonlookaheadids(child.get()))
				return true;

		return false;
	}

	void rewrite_lookaheadids(AstNode *node, bool lhs = false)
	{
		if (node->type == AST_ASSIGN_LE)
		{
			if (has_lookaheadids(node->children[0].get()))
			{
				if (has_nonlookaheadids(node->children[0].get()))
					log_error("incompatible mix of lookahead and non-lookahead IDs in LHS expression.\n");

				rewrite_lookaheadids(node->children[0].get(), true);
				node->type = AST_ASSIGN_EQ;
			}

			rewrite_lookaheadids(node->children[1].get(), lhs);
			return;
		}

		if (node->type == AST_IDENTIFIER && (node->lookahead || lhs)) {
			AstNode *newwire = lookaheadids.at(node->str).second;
			node->str = newwire->str;
			node->id2ast = newwire;
			lhs = false;
		}

		for (auto& child : node->children)
			rewrite_lookaheadids(child.get(), lhs);
	}

	LookaheadRewriter(AstNode *top)
	{
		// top->dumpAst(nullptr, "REWRITE-BEFORE> ");
		// top->dumpVlog(nullptr, "REWRITE-BEFORE> ");

		AstNode *block = nullptr;
		auto loc = top->location;

		for (auto& c : top->children)
			if (c->type == AST_BLOCK) {
				log_assert(block == nullptr);
				block = c.get();
			}
		log_assert(block != nullptr);

		collect_lookaheadids(block);
		rewrite_lookaheadids(block);

		for (auto it : lookaheadids)
		{
			auto ref_orig = std::make_unique<AstNode>(loc, AST_IDENTIFIER);
			ref_orig->str = it.second.first->str;
			ref_orig->id2ast = it.second.first;
			ref_orig->was_checked = true;

			auto ref_temp = std::make_unique<AstNode>(loc, AST_IDENTIFIER);
			ref_temp->str = it.second.second->str;
			ref_temp->id2ast = it.second.second;
			ref_temp->was_checked = true;

			auto init_assign = std::make_unique<AstNode>(loc, AST_ASSIGN_EQ, ref_temp->clone(), ref_orig->clone());
			auto final_assign = std::make_unique<AstNode>(loc, AST_ASSIGN_LE, std::move(ref_orig), std::move(ref_temp));

			block->children.insert(block->children.begin(), std::move(init_assign));
			block->children.push_back(std::move(final_assign));
		}

		// top->dumpAst(nullptr, "REWRITE-AFTER> ");
		// top->dumpVlog(nullptr, "REWRITE-AFTER> ");
	}
};

// helper class for converting AST always nodes to RTLIL processes
struct AST_INTERNAL::ProcessGenerator
{
	// input and output structures
	std::unique_ptr<AstNode> always;
	RTLIL::SigSpec initSyncSignals;
	RTLIL::Process *proc;
	RTLIL::SigSpec outputSignals;

	// This always points to the RTLIL::CaseRule being filled at the moment
	RTLIL::CaseRule *current_case;

	// This map contains the replacement pattern to be used in the right hand side
	// of an assignment. E.g. in the code "foo = bar; foo = func(foo);" the foo in the right
	// hand side of the 2nd assignment needs to be replace with the temporary signal holding
	// the value assigned in the first assignment. So when the first assignment is processed
	// the according information is appended to subst_rvalue_from and subst_rvalue_to.
	stackmap<RTLIL::SigBit, RTLIL::SigBit> subst_rvalue_map;

	// This map contains the replacement pattern to be used in the left hand side
	// of an assignment. E.g. in the code "always @(posedge clk) foo <= bar" the signal bar
	// should not be connected to the signal foo. Instead it must be connected to the temporary
	// signal that is used as input for the register that drives the signal foo.
	stackmap<RTLIL::SigBit, RTLIL::SigBit> subst_lvalue_map;

	// The code here generates a number of temporary signal for each output register. This
	// map helps generating nice numbered names for all this temporary signals.
	std::map<RTLIL::Wire*, int> new_temp_count;

	// Buffer for generating the init action
	RTLIL::SigSpec init_lvalue, init_rvalue;

	// The most recently assigned $print or $check cell \PRIORITY.
	int last_effect_priority;

	// Track which signals have been assigned in current_case to avoid unnecessary removeSignalFromCaseTree calls
	pool<RTLIL::SigBit> current_case_assigned_bits;

	ProcessGenerator(std::unique_ptr<AstNode> a, RTLIL::SigSpec initSyncSignalsArg = RTLIL::SigSpec()) : always(std::move(a)), initSyncSignals(initSyncSignalsArg), last_effect_priority(0)
	{
		// rewrite lookahead references
		LookaheadRewriter la_rewriter(always.get());

		// generate process and simple root case
		proc = current_module->addProcess(stringf("$proc$%s:%d$%d", RTLIL::encode_filename(*always->location.begin.filename), always->location.begin.line, autoidx++));
		set_src_attr(proc, always.get());
		for (auto &attr : always->attributes) {
			if (attr.second->type != AST_CONSTANT)
				always->input_error("Attribute `%s' with non-constant value!\n", attr.first);
			proc->attributes[attr.first] = attr.second->asAttrConst();
		}
		current_case = &proc->root_case;

		// create initial temporary signal for all output registers
		RTLIL::SigSpec subst_lvalue_from, subst_lvalue_to;
		collect_lvalues(subst_lvalue_from, always.get(), true, true);
		subst_lvalue_to = new_temp_signal(subst_lvalue_from);
		subst_lvalue_map = subst_lvalue_from.to_sigbit_map(subst_lvalue_to);

		bool found_global_syncs = false;
		bool found_anyedge_syncs = false;
		for (auto& child : always->children)
		{
			if ((child->type == AST_POSEDGE || child->type == AST_NEGEDGE) && GetSize(child->children) == 1 && child->children.at(0)->type == AST_IDENTIFIER &&
					child->children.at(0)->id2ast && child->children.at(0)->id2ast->type == AST_WIRE && child->children.at(0)->id2ast->get_bool_attribute(ID::gclk)) {
				found_global_syncs = true;
			}
			if (child->type == AST_EDGE) {
				if (GetSize(child->children) == 1 && child->children.at(0)->type == AST_IDENTIFIER && child->children.at(0)->str == "\\$global_clock")
					found_global_syncs = true;
				else
					found_anyedge_syncs = true;
			}
		}

		if (found_anyedge_syncs) {
			if (found_global_syncs)
				always->input_error("Found non-synthesizable event list!\n");
			log("Note: Assuming pure combinatorial block at %s in\n", always->loc_string());
			log("compliance with IEC 62142(E):2005 / IEEE Std. 1364.1(E):2002. Recommending\n");
			log("use of @* instead of @(...) for better match of synthesis and simulation.\n");
		}

		// create syncs for the process
		bool found_clocked_sync = false;
		for (auto& child : always->children)
			if (child->type == AST_POSEDGE || child->type == AST_NEGEDGE) {
				if (GetSize(child->children) == 1 && child->children.at(0)->type == AST_IDENTIFIER && child->children.at(0)->id2ast &&
						child->children.at(0)->id2ast->type == AST_WIRE && child->children.at(0)->id2ast->get_bool_attribute(ID::gclk))
					continue;
				found_clocked_sync = true;
				if (found_global_syncs || found_anyedge_syncs)
					always->input_error("Found non-synthesizable event list!\n");
				RTLIL::SyncRule *syncrule = new RTLIL::SyncRule;
				syncrule->type = child->type == AST_POSEDGE ? RTLIL::STp : RTLIL::STn;
				syncrule->signal = child->children[0]->genRTLIL();
				if (GetSize(syncrule->signal) != 1) {
					// Edge event on a real-typed signal is illegal in SV
					// (IEEE 6.12) but occurs in simulation-only code;
					// degrade to the low bit so elaboration can proceed.
					AstNode *edge_id = child->children.at(0).get();
					bool is_real_sig = edge_id->type == AST_IDENTIFIER && edge_id->id2ast &&
							edge_id->id2ast->type == AST_WIRE && !edge_id->id2ast->children.empty() &&
							edge_id->id2ast->children[0] && edge_id->id2ast->children[0]->type == AST_REALVALUE;
					if (!is_real_sig)
						always->input_error("Found posedge/negedge event on a signal that is not 1 bit wide!\n");
					syncrule->signal = syncrule->signal.extract(0, 1);
				}
				addChunkActions(syncrule->actions, subst_lvalue_from, subst_lvalue_to, true);
				proc->syncs.push_back(syncrule);
			}
		if (proc->syncs.empty()) {
			RTLIL::SyncRule *syncrule = new RTLIL::SyncRule;
			syncrule->type = found_global_syncs ? RTLIL::STg : RTLIL::STa;
			syncrule->signal = RTLIL::SigSpec();
			addChunkActions(syncrule->actions, subst_lvalue_from, subst_lvalue_to, true);
			proc->syncs.push_back(syncrule);
		}

		// create initial assignments for the temporary signals
		if ((flag_nolatches || always->get_bool_attribute(ID::nolatches) || current_module->get_bool_attribute(ID::nolatches)) && !found_clocked_sync) {
			subst_rvalue_map = subst_lvalue_from.to_sigbit_dict(RTLIL::SigSpec(RTLIL::State::Sx, GetSize(subst_lvalue_from)));
		} else {
			addChunkActions(current_case->actions, subst_lvalue_to, subst_lvalue_from);
			// Track initial assignments
			for (auto &bit : subst_lvalue_to)
				if (bit.wire != NULL)
					current_case_assigned_bits.insert(bit);
		}

		// process the AST
		for (auto& child : always->children)
			if (child->type == AST_BLOCK)
				processAst(child.get());

		for (auto sync: proc->syncs)
			processMemWrites(sync);

		if (initSyncSignals.size() > 0)
		{
			RTLIL::SyncRule *sync = new RTLIL::SyncRule;
			sync->type = RTLIL::SyncType::STi;
			proc->syncs.push_back(sync);

			log_assert(init_lvalue.size() == init_rvalue.size());

			int offset = 0;
			for (auto &init_lvalue_c : init_lvalue.chunks()) {
				RTLIL::SigSpec lhs = init_lvalue_c;
				RTLIL::SigSpec rhs = init_rvalue.extract(offset, init_lvalue_c.width);
				remove_unwanted_lvalue_bits(lhs, rhs);
				sync->actions.push_back(RTLIL::SigSig(lhs, rhs));
				offset += lhs.size();
			}
		}

		outputSignals = RTLIL::SigSpec(subst_lvalue_from);
	}

	void remove_unwanted_lvalue_bits(RTLIL::SigSpec &lhs, RTLIL::SigSpec &rhs)
	{
		RTLIL::SigSpec new_lhs, new_rhs;

		log_assert(GetSize(lhs) == GetSize(rhs));
		for (int i = 0; i < GetSize(lhs); i++) {
			if (lhs[i].wire == nullptr)
				continue;
			new_lhs.append(lhs[i]);
			new_rhs.append(rhs[i]);
		}

		lhs = new_lhs;
		rhs = new_rhs;
	}

	// create new temporary signals
	RTLIL::SigSpec new_temp_signal(RTLIL::SigSpec sig)
	{
		std::vector<RTLIL::SigChunk> chunks = sig.chunks();

		for (int i = 0; i < GetSize(chunks); i++)
		{
			RTLIL::SigChunk &chunk = chunks[i];
			if (chunk.wire == nullptr)
				continue;

			std::string wire_name;
			do {
				wire_name = stringf("$%d%s[%d:%d]", new_temp_count[chunk.wire]++,
						chunk.wire->name.c_str(), chunk.width+chunk.offset-1, chunk.offset);;
				if (chunk.wire->name.str().find('$') != std::string::npos)
					wire_name += stringf("$%d", autoidx++);
			} while (current_module->wires_.count(wire_name) > 0);

			RTLIL::Wire *wire = current_module->addWire(wire_name, chunk.width);
			set_src_attr(wire, always.get());

			chunk.wire = wire;
			chunk.offset = 0;
		}

		return chunks;
	}

	// recursively traverse the AST and collect all assigned signals
	void collect_lvalues(RTLIL::SigSpec &reg, AstNode *ast, bool type_eq, bool type_le, bool run_sort_and_unify = true)
	{
		switch (ast->type)
		{
		case AST_CASE:
			for (auto& child : ast->children)
				if (child != ast->children[0]) {
					if (child->type != AST_COND && child->type != AST_CONDX && child->type != AST_CONDZ) {
						degrade(ast->location.to_loc(), "Skipping case item of type %s while collecting signals.\n", type2str(child->type).c_str());
						continue;
					}
					collect_lvalues(reg, child.get(), type_eq, type_le, false);
				}
			break;

		case AST_COND:
		case AST_CONDX:
		case AST_CONDZ:
		case AST_ALWAYS:
		case AST_INITIAL:
			for (auto& child : ast->children)
				if (child->type == AST_BLOCK)
					collect_lvalues(reg, child.get(), type_eq, type_le, false);
			break;

		case AST_BLOCK:
			for (auto& child : ast->children) {
				if (child->type == AST_ASSIGN_EQ && type_eq)
					reg.append(child->children[0]->genRTLIL());
				if (child->type == AST_ASSIGN_LE && type_le)
					reg.append(child->children[0]->genRTLIL());
				if (child->type == AST_CASE || child->type == AST_BLOCK)
					collect_lvalues(reg, child.get(), type_eq, type_le, false);
			}
			break;

		default:
			log_abort();
		}

		if (run_sort_and_unify) {
			std::set<RTLIL::SigBit> sorted_reg;
			for (auto bit : reg)
				if (bit.wire)
					sorted_reg.insert(bit);
			reg = RTLIL::SigSpec(sorted_reg);
		}
	}

	// remove all assignments to the given signal pattern in a case and all its children.
	// e.g. when the last statement in the code "a = 23; if (b) a = 42; a = 0;" is processed this
	// function is called to clean up the first two assignments as they are overwritten by
	// the third assignment.
	void removeSignalFromCaseTree(const pool<RTLIL::SigBit> &pattern_bits, RTLIL::CaseRule *cs)
	{
		// Optimization: check actions in reverse order and stop early if we've found all pattern bits
		pool<RTLIL::SigBit> remaining_bits = pattern_bits;

		for (auto it = cs->actions.rbegin(); it != cs->actions.rend(); ++it) {
			bool has_pattern = false;
			for (auto &bit : it->first) {
				if (bit.wire != NULL && remaining_bits.count(bit)) {
					has_pattern = true;
					remaining_bits.erase(bit);
				}
			}

			if (has_pattern) {
				it->first.remove2(pattern_bits, &it->second);
			}

			// Early exit if we've processed all bits in pattern
			if (remaining_bits.empty())
				break;
		}

		for (auto it = cs->switches.begin(); it != cs->switches.end(); it++)
			for (auto it2 = (*it)->cases.begin(); it2 != (*it)->cases.end(); it2++)
				removeSignalFromCaseTree(pattern_bits, *it2);
	}

	void removeSignalFromCaseTree(const RTLIL::SigSpec &pattern, RTLIL::CaseRule *cs)
	{
		pool<RTLIL::SigBit> pattern_bits;
		pattern_bits.reserve(pattern.size());
		for (auto &bit : pattern)
			if (bit.wire != NULL)
				pattern_bits.insert(bit);
		removeSignalFromCaseTree(pattern_bits, cs);
	}

	// add an assignment (aka "action") but split it up in chunks. this way huge assignments
	// are avoided and the generated $mux cells have a more "natural" size.
	void addChunkActions(std::vector<RTLIL::SigSig> &actions, RTLIL::SigSpec lvalue, RTLIL::SigSpec rvalue, bool inSyncRule = false)
	{
		if (inSyncRule && initSyncSignals.size() > 0) {
			init_lvalue.append(lvalue.extract(initSyncSignals));
			init_rvalue.append(lvalue.extract(initSyncSignals, &rvalue));
			lvalue.remove2(initSyncSignals, &rvalue);
		}
		log_assert(lvalue.size() == rvalue.size());

		int offset = 0;
		for (auto &lvalue_c : lvalue.chunks()) {
			RTLIL::SigSpec lhs = lvalue_c;
			RTLIL::SigSpec rhs = rvalue.extract(offset, lvalue_c.width);
			if (inSyncRule && lvalue_c.wire && lvalue_c.wire->get_bool_attribute(ID::nosync))
				rhs = RTLIL::SigSpec(RTLIL::State::Sx, rhs.size());
			remove_unwanted_lvalue_bits(lhs, rhs);
			actions.push_back(RTLIL::SigSig(lhs, rhs));
			offset += lhs.size();
		}
	}

	// recursively process the AST and fill the RTLIL::Process
	void processAst(AstNode *ast)
	{
		switch (ast->type)
		{
		case AST_BLOCK:
			for (auto& child : ast->children)
				processAst(child.get());
			break;

		case AST_ASSIGN_EQ:
		case AST_ASSIGN_LE:
			{
				RTLIL::SigSpec unmapped_lvalue = ast->children[0]->genRTLIL(), lvalue = unmapped_lvalue;
				RTLIL::SigSpec rvalue = ast->children[1]->genWidthRTLIL(lvalue.size(), true, &subst_rvalue_map.stdmap());

				pool<SigBit> lvalue_sigbits;
				for (int i = 0; i < GetSize(lvalue); i++) {
					if (lvalue_sigbits.count(lvalue[i]) > 0) {
						unmapped_lvalue.remove(i);
						lvalue.remove(i);
						rvalue.remove(i--);
					} else
						lvalue_sigbits.insert(lvalue[i]);
				}

				lvalue.replace(subst_lvalue_map.stdmap());

				if (ast->type == AST_ASSIGN_EQ) {
					for (int i = 0; i < GetSize(unmapped_lvalue); i++)
						subst_rvalue_map.set(unmapped_lvalue[i], rvalue[i]);
				}

				// Check if any bits in lvalue have been assigned before in current_case
				bool has_overlap = false;
				for (auto &bit : lvalue) {
					if (bit.wire != NULL && current_case_assigned_bits.count(bit)) {
						has_overlap = true;
						break;
					}
				}

				if (has_overlap)
					removeSignalFromCaseTree(lvalue, current_case);

				// Track newly assigned bits
				for (auto &bit : lvalue)
					if (bit.wire != NULL)
						current_case_assigned_bits.insert(bit);

				remove_unwanted_lvalue_bits(lvalue, rvalue);
				current_case->actions.push_back(RTLIL::SigSig(lvalue, rvalue));
			}
			break;

		case AST_CASE:
			{
				int width_hint;
				bool sign_hint;
				ast->detectSignWidth(width_hint, sign_hint);

				RTLIL::SwitchRule *sw = new RTLIL::SwitchRule;
				set_src_attr(sw, ast);
				sw->signal = ast->children[0]->genWidthRTLIL(width_hint, sign_hint, &subst_rvalue_map.stdmap());
				current_case->switches.push_back(sw);

				for (auto &attr : ast->attributes) {
					if (attr.second->type != AST_CONSTANT)
						ast->input_error("Attribute `%s' with non-constant value!\n", attr.first);
					sw->attributes[attr.first] = attr.second->asAttrConst();
				}

				RTLIL::SigSpec this_case_eq_lvalue;
				collect_lvalues(this_case_eq_lvalue, ast, true, false);

				RTLIL::SigSpec this_case_eq_ltemp = new_temp_signal(this_case_eq_lvalue);

				RTLIL::SigSpec this_case_eq_rvalue = this_case_eq_lvalue;
				this_case_eq_rvalue.replace(subst_rvalue_map.stdmap());

				RTLIL::CaseRule *default_case = nullptr;
				RTLIL::CaseRule *last_generated_case = nullptr;
				for (auto& child : ast->children)
				{
					if (child == ast->children[0])
						continue;
					if (child->type != AST_COND && child->type != AST_CONDX && child->type != AST_CONDZ)
						continue;

					subst_lvalue_map.save();
					subst_rvalue_map.save();

					for (int i = 0; i < GetSize(this_case_eq_lvalue); i++)
						subst_lvalue_map.set(this_case_eq_lvalue[i], this_case_eq_ltemp[i]);

					RTLIL::CaseRule *backup_case = current_case;
					current_case = new RTLIL::CaseRule;
					pool<RTLIL::SigBit> backup_assigned_bits = std::move(current_case_assigned_bits);
					current_case_assigned_bits.clear();
					set_src_attr(current_case, child.get());
					last_generated_case = current_case;
					addChunkActions(current_case->actions, this_case_eq_ltemp, this_case_eq_rvalue);
					// Track temp assignments
					for (auto &bit : this_case_eq_ltemp)
						if (bit.wire != NULL)
							current_case_assigned_bits.insert(bit);
					for (auto& node : child->children) {
						if (node->type == AST_DEFAULT)
							default_case = current_case;
						else if (node->type == AST_BLOCK)
							processAst(node.get());
						else
							current_case->compare.push_back(node->genWidthRTLIL(width_hint, sign_hint, &subst_rvalue_map.stdmap()));
					}
					if (default_case != current_case)
						sw->cases.push_back(current_case);
					else
						log_assert(current_case->compare.size() == 0);
					current_case = backup_case;
					current_case_assigned_bits = std::move(backup_assigned_bits);

					subst_lvalue_map.restore();
					subst_rvalue_map.restore();
				}

				if (last_generated_case != nullptr && ast->get_bool_attribute(ID::full_case) && default_case == nullptr) {
			#if 0
					// this is a valid transformation, but as optimization it is premature.
					// better: add a default case that assigns 'x' to everything, and let later
					// optimizations take care of the rest
					last_generated_case->compare.clear();
			#else
					default_case = new RTLIL::CaseRule;
					addChunkActions(default_case->actions, this_case_eq_ltemp, SigSpec(State::Sx, GetSize(this_case_eq_rvalue)));
					sw->cases.push_back(default_case);
			#endif
				} else {
					if (default_case == nullptr) {
						default_case = new RTLIL::CaseRule;
						addChunkActions(default_case->actions, this_case_eq_ltemp, this_case_eq_rvalue);
					}
					sw->cases.push_back(default_case);
				}

				for (int i = 0; i < GetSize(this_case_eq_lvalue); i++)
					subst_rvalue_map.set(this_case_eq_lvalue[i], this_case_eq_ltemp[i]);

				this_case_eq_lvalue.replace(subst_lvalue_map.stdmap());

				// Check if any bits in lvalue have been assigned before in current_case
				bool has_overlap = false;
				for (auto &bit : this_case_eq_lvalue) {
					if (bit.wire != NULL && current_case_assigned_bits.count(bit)) {
						has_overlap = true;
						break;
					}
				}

				if (has_overlap)
					removeSignalFromCaseTree(this_case_eq_lvalue, current_case);

				addChunkActions(current_case->actions, this_case_eq_lvalue, this_case_eq_ltemp);
				// Track newly assigned bits
				for (auto &bit : this_case_eq_lvalue)
					if (bit.wire != NULL)
						current_case_assigned_bits.insert(bit);
			}
			break;

		case AST_WIRE:
			// Procedural declarations stay in the statement list when the block
			// has no scope label. The net, if any, is already a module wire.
			break;

		case AST_ASSIGN:
			ast->input_error("Found continuous assignment in always/initial block!\n");
			break;

		case AST_PARAMETER:
		case AST_LOCALPARAM:
			ast->input_error("Found parameter declaration in block without label!\n");
			break;

		case AST_TCALL:
			if (ast->str == "$fatal" || ast->str == "$error" || ast->str == "$warning" || ast->str == "$info")
				break;
			if (ast->str == "$finish" || ast->str == "$stop") {
				// runtime simulation control: same trigger/EN convention as
				// $print, no arguments; the sim pass ends the run when the
				// cell fires
				std::stringstream sstr;
				sstr << ast->str << "$" << ast->location.begin.filename << ":" << ast->location.begin.line << "$" << (autoidx++);

				Wire *en = current_module->addWire(sstr.str() + "_EN", 1);
				set_src_attr(en, ast);
				proc->root_case.actions.push_back(SigSig(en, false));
				current_case->actions.push_back(SigSig(en, true));

				RTLIL::SigSpec triggers;
				RTLIL::Const::Builder polarity_builder;
				for (auto sync : proc->syncs) {
					if (sync->type == RTLIL::STp) {
						triggers.append(sync->signal);
						polarity_builder.push_back(RTLIL::S1);
					} else if (sync->type == RTLIL::STn) {
						triggers.append(sync->signal);
						polarity_builder.push_back(RTLIL::S0);
					}
				}
				RTLIL::Const polarity = polarity_builder.build();

				RTLIL::Cell *cell = current_module->addCell(sstr.str(), ast->str == "$finish" ? ID($finish) : ID($stop));
				set_src_attr(cell, ast);
				cell->setParam(ID::TRG_WIDTH, triggers.size());
				cell->setParam(ID::TRG_ENABLE, (always->type == AST_INITIAL) || !triggers.empty());
				cell->setParam(ID::TRG_POLARITY, polarity);
				cell->setParam(ID::PRIORITY, --last_effect_priority);
				cell->setPort(ID::TRG, triggers);
				cell->setPort(ID::EN, en);
				break;
			}
			if (ast->str == "$display" || ast->str == "$displayb" || ast->str == "$displayh" || ast->str == "$displayo" ||
		  ast->str == "$write"   || ast->str == "$writeb"   || ast->str == "$writeh"   || ast->str == "$writeo") {
				std::stringstream sstr;
				sstr << ast->str << "$" << ast->location.begin.filename << ":" << ast->location.begin.line << "$" << (autoidx++);

				Wire *en = current_module->addWire(sstr.str() + "_EN", 1);
				set_src_attr(en, ast);
				proc->root_case.actions.push_back(SigSig(en, false));
				current_case->actions.push_back(SigSig(en, true));

				RTLIL::SigSpec triggers;
				RTLIL::Const::Builder polarity_builder;
				for (auto sync : proc->syncs) {
					if (sync->type == RTLIL::STp) {
						triggers.append(sync->signal);
						polarity_builder.push_back(RTLIL::S1);
					} else if (sync->type == RTLIL::STn) {
						triggers.append(sync->signal);
						polarity_builder.push_back(RTLIL::S0);
					}
				}
				RTLIL::Const polarity = polarity_builder.build();

				RTLIL::Cell *cell = current_module->addCell(sstr.str(), ID($print));
				set_src_attr(cell, ast);
				cell->setParam(ID::TRG_WIDTH, triggers.size());
				cell->setParam(ID::TRG_ENABLE, (always->type == AST_INITIAL) || !triggers.empty());
				cell->setParam(ID::TRG_POLARITY, polarity);
				cell->setParam(ID::PRIORITY, --last_effect_priority);
				cell->setPort(ID::TRG, triggers);
				cell->setPort(ID::EN, en);

				int default_base = 10;
				if (ast->str.back() == 'b')
					default_base = 2;
				else if (ast->str.back() == 'o')
					default_base = 8;
				else if (ast->str.back() == 'h')
					default_base = 16;

				std::vector<VerilogFmtArg> args;
				for (auto& node : ast->children) {
					int width;
					bool is_signed;
					node->detectSignWidth(width, is_signed, nullptr);

					VerilogFmtArg arg = {};
					arg.src = node->location.to_loc();
					if (node->type == AST_CONSTANT && node->is_string) {
						arg.type = VerilogFmtArg::STRING;
						arg.str = node->bitsAsConst().decode_string();
						// and in case this will be used as an argument...
						arg.sig = node->bitsAsConst();
						arg.signed_ = false;
					} else if (node->type == AST_IDENTIFIER && node->str == "$time") {
						arg.type = VerilogFmtArg::TIME;
					} else if (node->type == AST_IDENTIFIER && node->str == "$realtime") {
						arg.type = VerilogFmtArg::TIME;
						arg.realtime = true;
					} else {
						arg.type = VerilogFmtArg::INTEGER;
						arg.sig = node->genWidthRTLIL(-1, false, &subst_rvalue_map.stdmap());
						arg.signed_ = is_signed;
					}
					args.push_back(arg);
				}

				Fmt fmt;
				fmt.parse_verilog(args, /*sformat_like=*/false, default_base, /*task_name=*/ast->str, current_module->name);
				if (ast->str.substr(0, 8) == "$display")
					fmt.append_literal("\n");
				fmt.emit_rtlil(cell);
			} else if (!ast->str.empty()) {
				degrade(ast->location.to_loc(), "Ignoring task call `%s' during RTL generation.\n", ast->str.c_str());
			}
			break;

		// generate $check cells
		case AST_ASSERT:
		case AST_ASSUME:
		case AST_LIVE:
		case AST_FAIR:
		case AST_COVER:
			{
				std::string flavor, desc;
				if (ast->type == AST_ASSERT) { flavor = "assert"; desc = "assert ()"; }
				if (ast->type == AST_ASSUME) { flavor = "assume"; desc = "assume ()"; }
				if (ast->type == AST_LIVE) { flavor = "live"; desc = "assert (eventually)"; }
				if (ast->type == AST_FAIR) { flavor = "fair"; desc = "assume (eventually)"; }
				if (ast->type == AST_COVER) { flavor = "cover"; desc = "cover ()"; }

				IdString cellname;
				if (ast->str.empty())
					cellname = stringf("$%s$%s:%d$%d", flavor, RTLIL::encode_filename(*ast->location.begin.filename), ast->location.begin.line, autoidx++);
				else
					cellname = ast->str;
				check_unique_id(current_module, cellname, ast, "procedural assertion");

				RTLIL::SigSpec check = ast->children[0]->genWidthRTLIL(-1, false, &subst_rvalue_map.stdmap());
				if (GetSize(check) != 1)
					check = current_module->ReduceBool(NEW_ID, check);

				Wire *en = current_module->addWire(cellname.str() + "_EN", 1);
				set_src_attr(en, ast);
				proc->root_case.actions.push_back(SigSig(en, false));
				current_case->actions.push_back(SigSig(en, true));

				RTLIL::SigSpec triggers;
				RTLIL::Const::Builder polarity_builder;
				for (auto sync : proc->syncs) {
					if (sync->type == RTLIL::STp) {
						triggers.append(sync->signal);
						polarity_builder.push_back(RTLIL::S1);
					} else if (sync->type == RTLIL::STn) {
						triggers.append(sync->signal);
						polarity_builder.push_back(RTLIL::S0);
					}
				}
				RTLIL::Const polarity = polarity_builder.build();

				RTLIL::Cell *cell = current_module->addCell(cellname, ID($check));
				set_src_attr(cell, ast);
				cell->set_bool_attribute(ID(keep));
				for (auto &attr : ast->attributes) {
					if (attr.second->type != AST_CONSTANT)
						log_file_error(ast->location.to_loc(), "Attribute `%s' with non-constant value!\n", attr.first);
					cell->attributes[attr.first] = attr.second->asAttrConst();
				}
				cell->setParam(ID::FLAVOR, flavor);
				cell->setParam(ID::TRG_WIDTH, triggers.size());
				cell->setParam(ID::TRG_ENABLE, (always->type == AST_INITIAL) || !triggers.empty());
				cell->setParam(ID::TRG_POLARITY, polarity);
				cell->setParam(ID::PRIORITY, --last_effect_priority);
				cell->setPort(ID::TRG, triggers);
				cell->setPort(ID::EN, en);
				cell->setPort(ID::A, check);

				// No message is emitted to ensure Verilog code roundtrips correctly.
				Fmt fmt;
				fmt.emit_rtlil(cell);
				break;
			}

		case AST_NONE:
		case AST_FOR:
			break;

		case AST_REPEAT:
		case AST_WHILE:
		case AST_GENFOR:
		case AST_GENIF:
		case AST_GENCASE:
			break;

		default:
			// ast->dumpAst(nullptr, "ast> ");
			// current_ast_mod->dumpAst(nullptr, "mod> ");
			log_abort();
		}
	}

	void processMemWrites(RTLIL::SyncRule *sync)
	{
		// Maps per-memid AST_MEMWR IDs to indices in the mem_write_actions array.
		dict<std::pair<std::string, int>, int> port_map;
		for (auto& child : always->children)
			if (child->type == AST_MEMWR)
			{
				std::string memid = child->str;
				int portid = child->children[3]->asInt(false);
				int cur_idx = GetSize(sync->mem_write_actions);
				RTLIL::MemWriteAction action;
				set_src_attr(&action, child.get());
				action.memid = memid;
				action.address = child->children[0]->genWidthRTLIL(-1, true, &subst_rvalue_map.stdmap());
				action.data = child->children[1]->genWidthRTLIL(current_module->memories[memid]->width, true, &subst_rvalue_map.stdmap());
				action.enable = child->children[2]->genWidthRTLIL(-1, true, &subst_rvalue_map.stdmap());
				RTLIL::Const orig_priority_mask = child->children[4]->bitsAsConst();
				RTLIL::Const priority_mask = RTLIL::Const(0, cur_idx);
				for (int i = 0; i < portid; i++) {
					int new_bit = port_map[std::make_pair(memid, i)];
					priority_mask.set(new_bit, orig_priority_mask[i]);
				}
				action.priority_mask = priority_mask;
				sync->mem_write_actions.push_back(action);
				port_map[std::make_pair(memid, portid)] = cur_idx;
			}
	}
};

// detect sign and width of an expression
void AstNode::detectSignWidthWorker(int &width_hint, bool &sign_hint, bool *found_real)
{
	bool size_literal = false;  // AST_CAST_SIZE: was the size a literal constant before simplify
	std::string type_name;
	bool sub_sign_hint = true;
	int sub_width_hint = -1;
	int this_width = 0;
	AstNode *range = nullptr;
	AstNode *id_ast = nullptr;

	bool local_found_real = false;
	if (found_real == nullptr)
		found_real = &local_found_real;

	switch (type)
	{
	case AST_NONE:
		// unallocated enum, ignore
		break;
	case AST_CONSTANT:
		width_hint = max(width_hint, GetSize(bits));
		if (!is_signed)
			sign_hint = false;
		break;

	case AST_REALVALUE:
		*found_real = true;
		width_hint = max(width_hint, 32);
		break;

	case AST_IDENTIFIER:
		id_ast = id2ast;
		if (!id_ast) {
			if (current_scope.count(str))
				id_ast = current_scope[str];
			else {
				std::string alt = try_pop_module_prefix();
				if (current_scope.count(alt))
					id_ast = current_scope[alt];
			}
		}
		if (!id_ast) {
			this_width = 1;
		} else if (id_ast->type == AST_PARAMETER || id_ast->type == AST_LOCALPARAM || id_ast->type == AST_ENUM_ITEM) {
			if (id_ast->children.size() > 1 && id_ast->children[1] && id_ast->children[1]->range_valid) {
				this_width = id_ast->children[1]->range_left - id_ast->children[1]->range_right + 1;
			} else if (!id_ast->children.empty() && id_ast->children[0]) {
				if (id_ast->children[0]->type != AST_CONSTANT)
					while (id_ast->simplify(true, 1, -1, false)) { }
				if (id_ast->children[0] && id_ast->children[0]->type == AST_CONSTANT)
					this_width = id_ast->children[0]->bits.size();
				else
					this_width = 1;
			} else
				this_width = 1;
			if (children.size() != 0)
				range = children[0].get();
		} else if (id_ast->type == AST_WIRE || id_ast->type == AST_AUTOWIRE) {
			if (!id_ast->range_valid)
				this_width = 1;
			else {
				this_width = id_ast->range_left - id_ast->range_right + 1;
				if (children.size() != 0)
					range = children[0].get();
			}
		} else if (id_ast->type == AST_GENVAR) {
			this_width = 32;
		} else if (id_ast->type == AST_MEMORY) {
			if (id_ast->children.empty() || !id_ast->children[0] || !id_ast->children[0]->range_valid)
				this_width = 1;
			else
				this_width = std::max(1, id_ast->children[0]->range_left - id_ast->children[0]->range_right + 1);
			if (children.size() > 1)
				range = children[1].get();
		} else if (id_ast->type == AST_STRUCT_ITEM || id_ast->type == AST_STRUCT || id_ast->type == AST_UNION) {
			auto tmp_range = make_index_range(id_ast);
			this_width = tmp_range->range_left - tmp_range->range_right + 1;
		} else
			this_width = 1;
		if (range) {
			if (range->children.size() == 1)
				this_width = 1;
			else if (!range->range_valid) {
				this_width = 1;
			} else
				this_width = range->range_left - range->range_right + 1;
			sign_hint = false;
		}
		width_hint = max(width_hint, this_width);
		if (!id_ast || !id_ast->is_signed)
			sign_hint = false;
		break;

	case AST_TO_BITS:
		while (children[0]->simplify(true, 1, -1, false) == true) { }
		if (children[0]->type != AST_CONSTANT)
			input_error("Left operand of tobits expression is not constant!\n");
		children[1]->detectSignWidthWorker(sub_width_hint, sign_hint);
		width_hint = max(width_hint, children[0]->bitsAsConst().as_int());
		break;

	case AST_TO_SIGNED:
		children.at(0)->detectSignWidthWorker(width_hint, sub_sign_hint);
		break;

	case AST_TO_UNSIGNED:
		children.at(0)->detectSignWidthWorker(width_hint, sub_sign_hint);
		sign_hint = false;
		break;

	case AST_SELFSZ:
		sub_width_hint = 0;
		children.at(0)->detectSignWidthWorker(sub_width_hint, sign_hint);
		break;

	case AST_CAST_SIZE:
		size_literal = children.at(0)->type == AST_CONSTANT;
		while (children.at(0)->simplify(true, 1, -1, false)) { }
		// upstream (IEEE 1800 6.24.1): the size of a static cast is a
		// constant expression and positive; the fork used to accept both
		// silently (register v3 B4: policy tests static_cast_*)
		{
			// a cast to a typedef/enum name (`state_sel_e'(x)`, the type
			// often from a package) reaches width detection as an
			// identifier that simplify normalizes to its width later: not
			// a non-constant size.  Everything else non-constant is.
			AstNode *sz = children.at(0).get();
			bool type_ref = sz->type == AST_IDENTIFIER &&
					(sz->id2ast == nullptr || sz->id2ast->type == AST_TYPEDEF || sz->id2ast->type == AST_ENUM);
			if (sz->type != AST_CONSTANT && !type_ref) {
				// strict: upstream's error.  -permissive: the old lenient
				// path (an unresolved type name from a missing package is
				// an implicit wire here), recorded as a degradation
				if (!AST::flag_permissive)
					input_error("Static cast with non constant expression!\n");
				degrade(location.to_loc(), "Static cast with a non-constant size `%s'; the operand width is used.\n", sz->str.c_str());
			}
		}
		children.at(1)->detectSignWidthWorker(width_hint, sign_hint);
		if (children.at(0)->type == AST_CONSTANT) {
			this_width = children.at(0)->bitsAsConst().as_int();
			// a literal size <= 0 is upstream's error; a parameter that
			// folds to <= 0 on an early width-detection pass (package
			// parameters are substituted later) keeps the lenient path --
			// genRTLIL below checks the final value
			if (this_width <= 0 && size_literal) {
				if (!AST::flag_permissive)
					input_error("Static cast with zero or negative size!\n");
				degrade(location.to_loc(), "Static cast with size %d; the operand width is used.\n", this_width);
			}
			if (this_width > 0)
				width_hint = max(width_hint, this_width);
		}
		if (width_hint <= 0)
			width_hint = 1;
		break;

	case AST_CONCAT:
		for (auto& child : children) {
			if (!child)
				continue;
			sub_width_hint = 0;
			sub_sign_hint = true;
			child->detectSignWidthWorker(sub_width_hint, sub_sign_hint);
			this_width += sub_width_hint;
		}
		width_hint = max(width_hint, this_width);
		sign_hint = false;
		break;

	case AST_REPLICATE:
		while (children[0]->simplify(true, 1, -1, false) == true) { }
		children[1]->detectSignWidthWorker(sub_width_hint, sub_sign_hint);
		if (children[0]->type == AST_CONSTANT)
			width_hint = max(width_hint, children[0]->bitsAsConst().as_int() * sub_width_hint);
		else
			width_hint = max(width_hint, sub_width_hint);
		sign_hint = false;
		break;

	case AST_ASSIGN_PATTERN:
		for (auto& child : children) {
			if (!child)
				continue;
			sub_width_hint = 0;
			sub_sign_hint = true;
			child->detectSignWidthWorker(sub_width_hint, sub_sign_hint);
		}
		sign_hint = false;
		break;

	case AST_NEG:
	case AST_BIT_NOT:
	case AST_POS:
		if (!children.empty() && children[0])
			children[0]->detectSignWidthWorker(width_hint, sign_hint, found_real);
		else
			width_hint = max(width_hint, 1);
		break;

	case AST_BIT_AND:
	case AST_BIT_OR:
	case AST_BIT_XOR:
	case AST_BIT_XNOR:
		for (auto& child : children)
			if (child)
				child->detectSignWidthWorker(width_hint, sign_hint, found_real);
		break;

	case AST_REDUCE_AND:
	case AST_REDUCE_OR:
	case AST_REDUCE_XOR:
	case AST_REDUCE_XNOR:
	case AST_REDUCE_BOOL:
		width_hint = max(width_hint, 1);
		sign_hint = false;
		break;

	case AST_SHIFT_LEFT:
	case AST_SHIFT_RIGHT:
	case AST_SHIFT_SLEFT:
	case AST_SHIFT_SRIGHT:
	case AST_SHIFTX:
	case AST_SHIFT:
	case AST_POW:
		if (!children.empty() && children[0])
			children[0]->detectSignWidthWorker(width_hint, sign_hint, found_real);
		else
			width_hint = max(width_hint, 1);
		break;

	case AST_LT:
	case AST_LE:
	case AST_EQ:
	case AST_NE:
	case AST_EQX:
	case AST_NEX:
	case AST_GE:
	case AST_GT:
		width_hint = max(width_hint, 1);
		sign_hint = false;
		break;

	case AST_ADD:
	case AST_SUB:
	case AST_MUL:
	case AST_DIV:
	case AST_MOD:
		for (auto& child : children)
			if (child)
				child->detectSignWidthWorker(width_hint, sign_hint, found_real);
		if (children.empty())
			width_hint = max(width_hint, 1);
		break;

	case AST_LOGIC_AND:
	case AST_LOGIC_OR:
	case AST_LOGIC_NOT:
		width_hint = max(width_hint, 1);
		sign_hint = false;
		break;

	case AST_TERNARY:
		if (children.size() > 2 && children[1])
			children[1]->detectSignWidthWorker(width_hint, sign_hint, found_real);
		if (children.size() > 2 && children[2])
			children[2]->detectSignWidthWorker(width_hint, sign_hint, found_real);
		if (width_hint < 0)
			width_hint = 1;
		break;

	case AST_MEMRD:
		if (!id2ast->is_signed)
			sign_hint = false;
		if (!id2ast->children[0]->range_valid)
			input_error("Failed to detect width of memory access `%s'!\n", str);
		this_width = id2ast->children[0]->range_left - id2ast->children[0]->range_right + 1;
		width_hint = max(width_hint, this_width);
		break;

	case AST_CASE:
	{
		// This detects the _overall_ sign and width to be used for comparing
		// the case expression with the case item expressions. The case
		// expression and case item expressions are extended to the maximum
		// width among them, and are only interpreted as signed if all of them
		// are signed.
		width_hint = -1;
		sign_hint = true;
		auto visit_case_expr = [&width_hint, &sign_hint] (AstNode *node) {
			int sub_width_hint = -1;
			bool sub_sign_hint = true;
			node->detectSignWidth(sub_width_hint, sub_sign_hint);
			width_hint = max(width_hint, sub_width_hint);
			sign_hint &= sub_sign_hint;
		};
		visit_case_expr(children[0].get());
		for (size_t i = 1; i < children.size(); i++) {
			AstNode *child = children[i].get();
			for (auto& v : child->children) {
				if (v->type != AST_DEFAULT && v->type != AST_BLOCK)
					visit_case_expr(v.get());
			}
		}
		break;
	}

	case AST_PREFIX:
		// Generate-loop prefixes normally collapse to identifiers. A prefix
		// that is still a prefix after simplify is not a value; width 1 lets
		// synthesis continue.
		simplify(true, 1, -1, false);
		if (type != AST_IDENTIFIER) {
			width_hint = max(width_hint, 1);
			sign_hint = false;
			break;
		}
		detectSignWidthWorker(width_hint, sign_hint, found_real);
		break;

	case AST_SVA:
		// Temporal formulas are not boolean vectors. Width-1 keeps them legal
		// as assertion conditions while the operator tree is preserved.
		width_hint = max(width_hint, 1);
		sign_hint = false;
		break;

	case AST_FCALL:
		if (str == "\\$anyconst" || str == "\\$anyseq" || str == "\\$allconst" || str == "\\$allseq") {
			if (GetSize(children) == 1) {
				while (children[0]->simplify(true, 1, -1, false) == true) { }
				if (children[0]->type != AST_CONSTANT)
					input_error("System function %s called with non-const argument!\n",
							RTLIL::unescape_id(str));
				width_hint = max(width_hint, int(children[0]->asInt(true)));
			}
			break;
		}
		if (str == "\\$past" || str == "\\$rose" || str == "\\$fell" || str == "\\$stable" ||
				str == "\\$changed" || str == "\\$sampled") {
			if (GetSize(children) > 0) {
				sub_width_hint = 0;
				sub_sign_hint = true;
				children.at(0)->detectSignWidthWorker(sub_width_hint, sub_sign_hint);
				if (str == "\\$past" || str == "\\$sampled") {
					width_hint = max(width_hint, sub_width_hint);
					sign_hint &= sub_sign_hint;
				} else {
					width_hint = max(width_hint, 1);
					sign_hint = false;
				}
			} else {
				width_hint = max(width_hint, 1);
				sign_hint = false;
			}
			break;
		}
		if (str == "\\$size" || str == "\\$bits" || str == "\\$high" || str == "\\$low" || str == "\\$left" || str == "\\$right") {
			width_hint = max(width_hint, 32);
			break;
		}
		if (current_scope.count(str))
		{
			// This width detection is needed for function calls which are
			// unelaborated, which currently applies to calls to functions
			// reached via unevaluated ternary branches or used in case or case
			// item expressions.
			const AstNode *func = current_scope.at(str);
			if (func->type != AST_FUNCTION)
				input_error("Function call to %s resolved to something that isn't a function!\n", RTLIL::unescape_id(str));
			const AstNode *wire = nullptr;
			for (const auto& child : func->children)
				if (child && child->str == func->str) {
					wire = child.get();
					break;
				}
			if (!wire || wire->type != AST_WIRE) {
				width_hint = max(width_hint, 1);
				sign_hint = false;
				break;
			}
			sign_hint &= wire->is_signed;
			int result_width = 1;
			if (!wire->children.empty())
			{
				log_assert(wire->children.size() == 1);
				const AstNode *range = wire->children.at(0).get();
				log_assert(range->type == AST_RANGE && range->children.size() == 2);
				auto left = range->children.at(0)->clone();
				auto right = range->children.at(1)->clone();
				left->set_in_param_flag(true);
				right->set_in_param_flag(true);
				while (left->simplify(true, 1, -1, false)) { }
				while (right->simplify(true, 1, -1, false)) { }
				if (left->type != AST_CONSTANT || right->type != AST_CONSTANT)
					input_error("Function %s has non-constant width!",
							RTLIL::unescape_id(str));
				result_width = abs(int(left->asInt(true) - right->asInt(true)));
			}
			width_hint = max(width_hint, result_width);
			break;
		}
		// Sequence/property/let instances kept as calls.
		width_hint = max(width_hint, 1);
		sign_hint = false;
		break;

	case AST_AUTOWIRE:
		width_hint = max(width_hint, 1);
		sign_hint = false;
		break;

	case AST_MULTIRANGE:
		width_hint = max(width_hint, 1);
		sign_hint = false;
		break;

	// everything should have been handled above -> print error if not.
	default:
		AstNode *current_scope_ast = current_ast_mod == nullptr ? current_ast : current_ast_mod;
		logger().for_each_sink([&](LogSink &sink) {
			FILE *f = sink.file_handle();
			if (f)
				current_scope_ast->dumpAst(f, "verilog-ast> ");
		});
		input_error("Don't know how to detect sign and width for %s node!\n", type2str(type));

	}

	if (*found_real)
		sign_hint = true;
}

// detect sign and width of an expression
void AstNode::detectSignWidth(int &width_hint, bool &sign_hint, bool *found_real)
{
	width_hint = -1;
	sign_hint = true;
	if (found_real)
		*found_real = false;
	detectSignWidthWorker(width_hint, sign_hint, found_real);

	// 2^27 bits (128 Mbit, 16 MB per expanded constant) still bounds
	// exponential width bombs while admitting testbench-scale array ports
	// flattened by the unpacked-port rewrite (e.g. 1M-word memory emulators).
	constexpr int kWidthLimit = 1 << 27;
	if (width_hint >= kWidthLimit)
		input_error("Expression width %d exceeds implementation limit of %d!\n",
					width_hint, kWidthLimit);
}

// create RTLIL from an AST node
// all generated cells, wires and processes are added to the module pointed to by 'current_module'
// when the AST node is an expression (AST_ADD, AST_BIT_XOR, etc.), the result signal is returned.
//
// note that this function is influenced by a number of global variables that might be set when
// called from genWidthRTLIL(). also note that this function recursively calls itself to transform
// larger expressions into a netlist of cells.
RTLIL::SigSpec AstNode::genRTLIL(int width_hint, bool sign_hint)
{
	{
		uintptr_t raw = reinterpret_cast<uintptr_t>(this);
		asm volatile("" : "+r"(raw));
		if (raw < 4096)
			return RTLIL::SigSpec(RTLIL::State::S0);
	}
	// in the following big switch() statement there are some uses of
	// Clifford's Device (http://www.clifford.at/cfun/cliffdev/). In this
	// cases this variable is used to hold the type of the cell that should
	// be instantiated for this type of AST node.
	IdString type_name;

	switch (type)
	{
	// simply ignore this nodes.
	// they are either leftovers from simplify() or are referenced by other nodes
	// and are only accessed here thru this references
	case AST_BLOCK:
		return RTLIL::SigSpec(RTLIL::State::S0);
	case AST_NONE:
	case AST_TASK:
	case AST_FUNCTION:
	case AST_DPI_FUNCTION:
	case AST_AUTOWIRE:
	case AST_DEFPARAM:
	case AST_GENVAR:
	case AST_GENFOR:
	case AST_GENBLOCK:
	case AST_GENIF:
	case AST_GENCASE:
	case AST_PACKAGE:
	case AST_IMPORT:
	case AST_CLASS:
	case AST_CELLTYPE:
	case AST_ENUM:
	case AST_MODPORT:
	case AST_MODPORTMEMBER:
	case AST_TYPEDEF:
		// a `parameter type` typedef is also a parameter: advertise it so
		// hierarchy accepts an instance's `.T(<type>)` (register v3 D0)
		if (attributes.count(ID(type_param)))
			current_module->avail_parameters(str);
	case AST_STRUCT:
	case AST_UNION:
		break;
	case AST_INTERFACEPORT: {
		// If a port in a module with unknown type is found, mark it with the attribute 'is_interface'
		// This is used by the hierarchy pass to know when it can replace interface connection with the individual
		// signals.
		RTLIL::IdString id = str;
		check_unique_id(current_module, id, this, "interface port");
		RTLIL::Wire *wire = current_module->addWire(id, 1);
		set_src_attr(wire, this);
		wire->start_offset = 0;
		wire->port_id = port_id;
		wire->port_input = true;
		wire->port_output = true;
		wire->set_bool_attribute(ID::is_interface);
		if (children.size() > 0) {
			for(size_t i=0; i<children.size();i++) {
				if(children[i]->type == AST_INTERFACEPORTTYPE) {
					std::pair<std::string,std::string> res = AST::split_modport_from_type(children[i]->str);
					wire->attributes[ID::interface_type] = res.first;
					if (res.second != "")
						wire->attributes[ID::interface_modport] = res.second;
					break;
				}
			}
		}
		wire->upto = 0;
		}
		break;
	case AST_INTERFACEPORTTYPE:
		break;

	// remember the parameter, needed for example in techmap
	case AST_PARAMETER:
		current_module->avail_parameters(str);
		if (GetSize(children) >= 1 && children[0]->type == AST_CONSTANT) {
			current_module->parameter_default_values[str] = children[0]->asParaConst();
		}
		YS_FALLTHROUGH
	case AST_LOCALPARAM:
		if (flag_pwires)
		{
			if (GetSize(children) < 1 || children[0]->type != AST_CONSTANT)
				input_error("Parameter `%s' with non-constant value!\n", str);

			RTLIL::Const val = children[0]->bitsAsConst();
			RTLIL::IdString id = str;
			check_unique_id(current_module, id, this, "pwire");
			RTLIL::Wire *wire = current_module->addWire(id, GetSize(val));
			current_module->connect(wire, val);
			wire->is_signed = children[0]->is_signed;

			set_src_attr(wire, this);
			wire->attributes[type == AST_PARAMETER ? ID::parameter : ID::localparam] = 1;

			for (auto &attr : attributes) {
				if (attr.second->type != AST_CONSTANT)
					input_error("Attribute `%s' with non-constant value!\n", attr.first);
				wire->attributes[attr.first] = attr.second->asAttrConst();
			}
		}
		break;

	// create an RTLIL::Wire for an AST_WIRE node
	case AST_WIRE: {
			if (!range_valid)
				input_error("Signal `%s' with non-constant width!\n", str);

			if (!(range_left + 1 >= range_right))
				input_error("Signal `%s' with invalid width range %d!\n", str, range_left - range_right + 1);

			RTLIL::IdString id = str;
			check_unique_id(current_module, id, this, "signal");
			RTLIL::Wire *wire = current_module->addWire(id, range_left - range_right + 1);
			set_src_attr(wire, this);
			wire->start_offset = range_right;
			wire->port_id = port_id;
			wire->port_input = is_input;
			wire->port_output = is_output;
			wire->upto = range_swapped;

			wire->is_signed = is_signed;

			for (auto &attr : attributes) {
				if (attr.second->type != AST_CONSTANT)
					input_error("Attribute `%s' with non-constant value!\n", attr.first);
				wire->attributes[attr.first] = attr.second->asAttrConst();
			}

			if (is_wand) wire->set_bool_attribute(ID::wand);
			if (is_wor)  wire->set_bool_attribute(ID::wor);
		}
		break;

	// create an RTLIL::Memory for an AST_MEMORY node
	case AST_MEMORY: {
			bool shaped = children.size() >= 2 && children[0] && children[1]
				&& children[0]->type == AST_RANGE && children[1]->type == AST_RANGE
				&& children[0]->range_valid && children[1]->range_valid;
			if (!shaped) {
				RTLIL::Memory *memory = new RTLIL::Memory;
				set_src_attr(memory, this);
				memory->name = str;
				memory->width = 1;
				memory->start_offset = 0;
				memory->size = 1;
				check_unique_id(current_module, memory->name, this, "memory");
				current_module->memories[memory->name] = memory;
				break;
			}

			RTLIL::Memory *memory = new RTLIL::Memory;
			set_src_attr(memory, this);
			memory->name = str;
			memory->width = children[0]->range_left - children[0]->range_right + 1;
			if (children[1]->range_right < children[1]->range_left) {
				memory->start_offset = children[1]->range_right;
				memory->size = children[1]->range_left - children[1]->range_right + 1;
			} else {
				memory->start_offset = children[1]->range_left;
				memory->size = children[1]->range_right - children[1]->range_left + 1;
			}
			check_unique_id(current_module, memory->name, this, "memory");
			current_module->memories[memory->name] = memory;

			for (auto &attr : attributes) {
				if (attr.second->type != AST_CONSTANT)
					input_error("Attribute `%s' with non-constant value!\n", attr.first);
				memory->attributes[attr.first] = attr.second->asAttrConst();
			}
		}
		break;

	// simply return the corresponding RTLIL::SigSpec for an AST_CONSTANT node
	case AST_CONSTANT:
	case AST_REALVALUE:
		{
			if (width_hint < 0)
				detectSignWidth(width_hint, sign_hint);
			is_signed = sign_hint;

			if (type == AST_CONSTANT) {
				if (is_unsized) {
					return RTLIL::SigSpec(bitsAsUnsizedConst(width_hint));
				} else {
					return RTLIL::SigSpec(bitsAsConst());
				}
			}

			RTLIL::SigSpec sig = realAsConst(width_hint);
			log_file_warning(location.to_loc(), "converting real value %e to binary %s.\n", realvalue, log_signal(sig));
			return sig;
		}

	// simply return the corresponding RTLIL::SigSpec for an AST_IDENTIFIER node
	// for identifiers with dynamic bit ranges (e.g. "foo[bar]" or "foo[bar+3:bar]") a
	// shifter cell is created and the output signal of this cell is returned
	case AST_IDENTIFIER:
		{
			RTLIL::Wire *wire = nullptr;
			RTLIL::SigChunk chunk;
			bool is_interface = false;

			AST::AstNode *member_node = nullptr;
			int add_undef_bits_msb = 0;
			int add_undef_bits_lsb = 0;

			if (id2ast == nullptr) {
				degrade(location.to_loc(), "Identifier `%s' has no declaration; using a 1-bit wire.\n", str.c_str());
				if (current_module) {
					RTLIL::Wire *fallback = current_module->wire(str);
					if (!fallback)
						fallback = current_module->addWire(str);
					return fallback;
				}
				return RTLIL::SigSpec(RTLIL::S0, 1);
			}

			if (id2ast->type == AST_AUTOWIRE && current_module->wires_.count(str) == 0) {
				RTLIL::Wire *wire = current_module->addWire(str);
				set_src_attr(wire, this);
				wire->name = str;

				// a bare reference to a named sequence/property/let declaration
				// inside an assertion is the hook sva2check resolves by name
				// (decl_by_name): the wire is by design, not an implicit net,
				// so it is neither a warning nor a degradation (the battery's
				// t10-style files were marked sv_degraded for it, and B5's
				// refusal then rejected them)
				bool sva_decl_ref = keep_sva_cells && sva_local_names.count(current_module) &&
						sva_local_names.at(current_module).count(str);
				if (keep_sva_cells && current_ast_mod && !sva_decl_ref) {
					for (auto &ch : current_ast_mod->children)
						if (ch && ch->type == AST_SVA && (ch->str == "sequence_decl" || ch->str == "property_decl" ||
								ch->str == "let_decl") && !ch->children.empty() && ch->children[0] &&
								ch->children[0]->str == str) {
							sva_decl_ref = true;
							break;
						}
				}
				if (!is_sequence_method_name(str) && !sva_decl_ref) {
					if (flag_autowire)
						// upstream's own leniency under `default_nettype wire`;
						// dotted names included: interface-port members
						// (`intf.driver`) are implicit wires here by design,
						// resolved by the hierarchy pass
						degrade_warn(location.to_loc(), "Identifier `%s' is implicitly declared.\n", str);
					else {
						degrade(location.to_loc(), "Identifier `%s' is implicitly declared; a 1-bit wire is implied.\n", str.c_str());
					}
				}
			}
			else if (id2ast->type == AST_PARAMETER || id2ast->type == AST_LOCALPARAM || id2ast->type == AST_ENUM_ITEM) {
				if (id2ast->children[0]->type != AST_CONSTANT) {
					RTLIL::Wire *fallback = current_module->wire(str);
					if (!fallback)
						fallback = current_module->addWire(str, 32);
					set_src_attr(fallback, this);
					return fallback;
				}
				chunk = RTLIL::Const(id2ast->children[0]->bits);
				goto use_const_chunk;
			}
			else if ((id2ast->type == AST_WIRE || id2ast->type == AST_AUTOWIRE || id2ast->type == AST_MEMORY) && current_module->wires_.count(str) != 0) {
				RTLIL::Wire *current_wire = current_module->wire(str);
				if (current_wire->get_bool_attribute(ID::is_interface))
					is_interface = true;
				// Ignore
			}
			// If an identifier is found that is not already known, assume that it is an interface:
			else if (1) { // FIXME: Check if sv_mode first?
				is_interface = true;
			}
			else {
				input_error("Identifier `%s' doesn't map to any signal!\n", str);
			}

			if (id2ast->type == AST_MEMORY) {
				int width = 1;
				if (!id2ast->children.empty() && id2ast->children[0]->type == AST_RANGE && id2ast->children[0]->range_valid)
					width = std::max(1, id2ast->children[0]->range_left - id2ast->children[0]->range_right + 1);
				RTLIL::IdString dummy_name = stringf("$unexpanded%s", str.c_str());
				RTLIL::Wire *dummy = current_module->wire(dummy_name);
				if (!dummy)
					dummy = current_module->addWire(dummy_name, width);
				return dummy;
			}

			// If identifier is an interface, create a RTLIL::SigSpec with a dummy wire with a attribute called 'is_interface'
			// This makes it possible for the hierarchy pass to see what are interface connections and then replace them
			// with the individual signals:
			if (is_interface) {
				IdString dummy_wire_name = stringf("$dummywireforinterface%s", str);
				RTLIL::Wire *dummy_wire = current_module->wire(dummy_wire_name);
				if (!dummy_wire) {
					dummy_wire = current_module->addWire(dummy_wire_name);
					dummy_wire->set_bool_attribute(ID::is_interface);
				}
				return dummy_wire;
			}

			wire = current_module->wires_[str];
			chunk.wire = wire;
			chunk.width = wire->width;
			chunk.offset = 0;

			if ((member_node = get_struct_member())) {
				// Clamp wire chunk to range of member within struct/union.
				chunk.width = member_node->range_left - member_node->range_right + 1;
				chunk.offset = member_node->range_right;
			}

		use_const_chunk:
			if (!children.empty() && children[0]->type == AST_MULTIRANGE)
				children.clear();
			if (children.size() != 0) {
				if (children[0]->type != AST_RANGE)
					input_error("Single range expected.\n");
				int source_width = id2ast->range_left - id2ast->range_right + 1;
				int source_offset = id2ast->range_right;
				int chunk_left = source_width - 1;
				int chunk_right = 0;

				if (member_node) {
					// Clamp wire chunk to range of member within struct/union.
					log_assert(!source_offset && !id2ast->range_swapped);
					chunk_left = chunk.offset + chunk.width - 1;
					chunk_right = chunk.offset;
				}

				if (!children[0]->range_valid) {
					auto left_at_zero_ast = children[0]->children[0]->clone_at_zero();
					auto right_at_zero_ast = children[0]->children.size() >= 2 ? children[0]->children[1]->clone_at_zero() : left_at_zero_ast->clone();
					while (left_at_zero_ast->simplify(true, 1, -1, false)) { }
					while (right_at_zero_ast->simplify(true, 1, -1, false)) { }
					if (left_at_zero_ast->type != AST_CONSTANT || right_at_zero_ast->type != AST_CONSTANT)
						input_error("Unsupported expression on dynamic range select on signal `%s'!\n", str);
					int width = abs(int(left_at_zero_ast->integer - right_at_zero_ast->integer)) + 1;
					auto fake_ast = std::make_unique<AstNode>(children[0]->location, AST_NONE, clone(), children[0]->children.size() >= 2 ?
							children[0]->children[1]->clone() : children[0]->children[0]->clone());
					fake_ast->children[0]->delete_children();
					if (member_node)
						fake_ast->children[0]->set_attribute(ID::wiretype, member_node->clone());

					int fake_ast_width = 0;
					bool fake_ast_sign = true;
					fake_ast->children[1]->detectSignWidth(fake_ast_width, fake_ast_sign);
					RTLIL::SigSpec shift_val = fake_ast->children[1]->genRTLIL(fake_ast_width, fake_ast_sign);

					if (source_offset != 0) {
						shift_val = current_module->Sub(NEW_ID, shift_val, source_offset, fake_ast_sign);
						fake_ast->children[1]->is_signed = true;
					}
					if (id2ast->range_swapped) {
						shift_val = current_module->Sub(NEW_ID, RTLIL::SigSpec(source_width - width), shift_val, fake_ast_sign);
						fake_ast->children[1]->is_signed = true;
					}
					if (GetSize(shift_val) >= 32)
						fake_ast->children[1]->is_signed = true;
					RTLIL::SigSpec sig = binop2rtlil(fake_ast.get(), ID($shiftx), width, fake_ast->children[0]->genRTLIL(), shift_val);
					return sig;
				} else {
					chunk.width = children[0]->range_left - children[0]->range_right + 1;
					chunk.offset += children[0]->range_right - source_offset;
					if (id2ast->range_swapped)
						chunk.offset = source_width - (chunk.offset + chunk.width);
					if (chunk.offset > chunk_left || chunk.offset + chunk.width < chunk_right) {
						if (chunk.width == 1)
							log_file_warning(location.to_loc(),	"Range select out of bounds on signal `%s': Setting result bit to undef.\n",
									str.c_str());
						else
							log_file_warning(location.to_loc(), "Range select [%d:%d] out of bounds on signal `%s': Setting all %d result bits to undef.\n",
									children[0]->range_left, children[0]->range_right, str.c_str(), chunk.width);
						chunk = RTLIL::SigChunk(RTLIL::State::Sx, chunk.width);
					} else {
						if (chunk.offset + chunk.width - 1 > chunk_left) {
							add_undef_bits_msb = (chunk.offset + chunk.width - 1) - chunk_left;
							chunk.width -= add_undef_bits_msb;
						}
						if (chunk.offset < chunk_right) {
							add_undef_bits_lsb = chunk_right - chunk.offset;
							chunk.width -= add_undef_bits_lsb;
							chunk.offset += add_undef_bits_lsb;
						}
						if (add_undef_bits_lsb)
							log_file_warning(location.to_loc(), "Range [%d:%d] select out of bounds on signal `%s': Setting %d LSB bits to undef.\n",
									children[0]->range_left, children[0]->range_right, str.c_str(), add_undef_bits_lsb);
						if (add_undef_bits_msb)
							log_file_warning(location.to_loc(), "Range [%d:%d] select out of bounds on signal `%s': Setting %d MSB bits to undef.\n",
									children[0]->range_left, children[0]->range_right, str.c_str(), add_undef_bits_msb);
					}
				}
			}

			RTLIL::SigSpec sig = { RTLIL::SigSpec(RTLIL::State::Sx, add_undef_bits_msb), chunk, RTLIL::SigSpec(RTLIL::State::Sx, add_undef_bits_lsb) };

			if (genRTLIL_subst_ptr)
				sig.replace(*genRTLIL_subst_ptr);

			is_signed = children.size() > 0 ? false : id2ast->is_signed && sign_hint;
			return sig;
		}

	// just pass thru the signal. the parent will evaluate the is_signed property and interpret the SigSpec accordingly
	case AST_TO_SIGNED:
	case AST_TO_UNSIGNED:
	case AST_SELFSZ: {
			RTLIL::SigSpec sig = children[0]->genRTLIL();
			if (sig.size() < width_hint)
				sig.extend_u0(width_hint, sign_hint);
			is_signed = sign_hint;
			return sig;
	}

	// changing the size of signal can be done directly using RTLIL::SigSpec
	case AST_CAST_SIZE: {
			RTLIL::SigSpec size = children[0]->genRTLIL();
			int width = 1;
			if (!size.is_fully_const()) {
				if (!AST::flag_permissive)
					input_error("Static cast with non constant expression!\n");
				degrade(location.to_loc(), "Static cast with a non-constant size; the operand width is used.\n");
				width = 1;  // the old lenient value
			} else {
				width = size.as_int();
				if (width <= 0) {
					// strict: upstream's error.  -permissive: the old
					// lenient width 1, recorded (a package parameter that
					// is still 0 inside an inlined package function is
					// register v3 D7)
					if (!AST::flag_permissive)
						input_error("Static cast with zero or negative size!\n");
					degrade(location.to_loc(), "Static cast with size %d; width 1 is used.\n", width);
					width = 1;
				}
			}
			// determine the *signedness* of the expression
			int sub_width_hint = -1;
			bool sub_sign_hint = true;
			children[1]->detectSignWidth(sub_width_hint, sub_sign_hint);
			// generate the signal given the *cast's* size and the
			// *expression's* signedness
			RTLIL::SigSpec sig = children[1]->genWidthRTLIL(width, sub_sign_hint);
			// context may effect this node's signedness, but not that of the
			// casted expression
			is_signed = sign_hint;
			return sig;
		}

	// concatenation of signals can be done directly using RTLIL::SigSpec
	case AST_CONCAT: {
			RTLIL::SigSpec sig;
			for (auto it = children.begin(); it != children.end(); it++)
				sig.append((*it)->genRTLIL());
			if (sig.size() < width_hint)
				sig.extend_u0(width_hint, false);
			return sig;
		}

	// replication of signals can be done directly using RTLIL::SigSpec
	case AST_REPLICATE: {
			if (keep_sva_cells)
				refuse_sva_operand(this);
			RTLIL::SigSpec left = children[0]->genRTLIL();
			RTLIL::SigSpec right = children[1]->genRTLIL();
			int count = 1;
			if (left.is_fully_const())
				count = left.as_int();
			if (count < 0)
				count = 0;
			RTLIL::SigSpec sig;
			for (int i = 0; i < count; i++)
				sig.append(right);
			if (sig.size() < width_hint)
				sig.extend_u0(width_hint, false);
			is_signed = false;
			return sig;
		}

	case AST_ASSIGN_PATTERN: {
			// IEEE 1800: the first element of a positional pattern is the leftmost
			// (most significant) member. SigSpec::append adds bits toward the MSB,
			// so the source order is emitted from last to first.
			RTLIL::SigSpec sig;
			for (int i = GetSize(children) - 1; i >= 0; i--)
				sig.append(children[i]->genRTLIL());
			if (sig.empty())
				sig = RTLIL::SigSpec(RTLIL::State::S0, std::max(1, width_hint));
			if (width_hint > 0 && sig.size() < width_hint)
				sig.extend_u0(width_hint, false);
			return sig;
		}

	// generate cells for unary operations: $not, $pos, $neg
	if (0) { case AST_BIT_NOT: type_name = ID($not); }
	if (0) { case AST_POS:     type_name = ID($pos); }
	if (0) { case AST_NEG:     type_name = ID($neg); }
		{
			RTLIL::SigSpec arg = children[0]->genRTLIL(width_hint, sign_hint);
			is_signed = children[0]->is_signed;
			int width = arg.size();
			if (width_hint > 0) {
				width = width_hint;
				widthExtend(this, arg, width, is_signed);
			}
			return uniop2rtlil(this, type_name, width, arg);
		}

	// generate cells for binary operations: $and, $or, $xor, $xnor
	if (0) { case AST_BIT_AND:  type_name = ID($and); }
	if (0) { case AST_BIT_OR:   type_name = ID($or); }
	if (0) { case AST_BIT_XOR:  type_name = ID($xor); }
	if (0) { case AST_BIT_XNOR: type_name = ID($xnor); }
		{
			if (width_hint < 0)
				detectSignWidth(width_hint, sign_hint);
			if (keep_sva_cells)
				refuse_sva_operand(this);
			RTLIL::SigSpec left = children[0]->genRTLIL(width_hint, sign_hint);
			RTLIL::SigSpec right = children[1]->genRTLIL(width_hint, sign_hint);
			int width = max(left.size(), right.size());
			if (width_hint > 0)
				width = width_hint;
			is_signed = children[0]->is_signed && children[1]->is_signed;
			return binop2rtlil(this, type_name, width, left, right);
		}

	// generate cells for unary operations: $reduce_and, $reduce_or, $reduce_xor, $reduce_xnor
	if (0) { case AST_REDUCE_AND:  type_name = ID($reduce_and); }
	if (0) { case AST_REDUCE_OR:   type_name = ID($reduce_or); }
	if (0) { case AST_REDUCE_XOR:  type_name = ID($reduce_xor); }
	if (0) { case AST_REDUCE_XNOR: type_name = ID($reduce_xnor); }
		{
			RTLIL::SigSpec arg = children[0]->genRTLIL();
			RTLIL::SigSpec sig = uniop2rtlil(this, type_name, max(width_hint, 1), arg);
			return sig;
		}

	// generate cells for unary operations: $reduce_bool
	// (this is actually just an $reduce_or, but for clarity a different cell type is used)
	if (0) { case AST_REDUCE_BOOL:  type_name = ID($reduce_bool); }
		{
			RTLIL::SigSpec arg = children[0]->genRTLIL();
			RTLIL::SigSpec sig = arg.size() > 1 ? uniop2rtlil(this, type_name, max(width_hint, 1), arg) : arg;
			return sig;
		}

	// generate cells for binary operations: $shl, $shr, $sshl, $sshr
	if (0) { case AST_SHIFT_LEFT:   type_name = ID($shl); }
	if (0) { case AST_SHIFT_RIGHT:  type_name = ID($shr); }
	if (0) { case AST_SHIFT_SLEFT:  type_name = ID($sshl); }
	if (0) { case AST_SHIFT_SRIGHT: type_name = ID($sshr); }
	if (0) { case AST_SHIFTX:       type_name = ID($shiftx); }
	if (0) { case AST_SHIFT:        type_name = ID($shift); }
		{
			if (width_hint < 0)
				detectSignWidth(width_hint, sign_hint);
			if (keep_sva_cells)
				refuse_sva_operand(this);
			RTLIL::SigSpec left = children[0]->genRTLIL(width_hint, sign_hint);
			// for $shift and $shiftx, the second operand can be negative
			RTLIL::SigSpec right = children[1]->genRTLIL(-1, type == AST_SHIFT || type == AST_SHIFTX);
			int width = width_hint > 0 ? width_hint : left.size();
			is_signed = children[0]->is_signed;
			return binop2rtlil(this, type_name, width, left, right);
		}

	// generate cells for binary operations: $pow
	case AST_POW:
		{
			int right_width;
			bool right_signed;
			children[1]->detectSignWidth(right_width, right_signed);
			if (width_hint < 0)
				detectSignWidth(width_hint, sign_hint);
			if (keep_sva_cells)
				refuse_sva_operand(this);
			RTLIL::SigSpec left = children[0]->genRTLIL(width_hint, sign_hint);
			RTLIL::SigSpec right = children[1]->genRTLIL(right_width, right_signed);
			int width = width_hint > 0 ? width_hint : left.size();
			is_signed = children[0]->is_signed;
			if (!flag_noopt && left.is_fully_const() && left.as_int() == 2 && !right_signed)
				return binop2rtlil(this, ID($shl), width, RTLIL::SigSpec(1, left.size()), right);
			return binop2rtlil(this, ID($pow), width, left, right);
		}

	// generate cells for binary operations: $lt, $le, $eq, $ne, $ge, $gt
	if (0) { case AST_LT:  type_name = ID($lt); }
	if (0) { case AST_LE:  type_name = ID($le); }
	if (0) { case AST_EQ:  type_name = ID($eq); }
	if (0) { case AST_NE:  type_name = ID($ne); }
	if (0) { case AST_EQX: type_name = ID($eqx); }
	if (0) { case AST_NEX: type_name = ID($nex); }
	if (0) { case AST_GE:  type_name = ID($ge); }
	if (0) { case AST_GT:  type_name = ID($gt); }
		{
			int width = max(width_hint, 1);
			width_hint = -1, sign_hint = true;
			children[0]->detectSignWidthWorker(width_hint, sign_hint);
			children[1]->detectSignWidthWorker(width_hint, sign_hint);
			if (keep_sva_cells)
				refuse_sva_operand(this);
			RTLIL::SigSpec left = children[0]->genRTLIL(width_hint, sign_hint);
			RTLIL::SigSpec right = children[1]->genRTLIL(width_hint, sign_hint);
			RTLIL::SigSpec sig = binop2rtlil(this, type_name, width, left, right);
			return sig;
		}

	// generate cells for binary operations: $add, $sub, $mul, $div, $mod
	if (0) { case AST_ADD: type_name = ID($add); }
	if (0) { case AST_SUB: type_name = ID($sub); }
	if (0) { case AST_MUL: type_name = ID($mul); }
	if (0) { case AST_DIV: type_name = ID($div); }
	if (0) { case AST_MOD: type_name = ID($mod); }
		{
			if (width_hint < 0)
				detectSignWidth(width_hint, sign_hint);
			if (keep_sva_cells)
				refuse_sva_operand(this);
			RTLIL::SigSpec left = children[0]->genRTLIL(width_hint, sign_hint);
			RTLIL::SigSpec right = children[1]->genRTLIL(width_hint, sign_hint);
		#if 0
			int width = max(left.size(), right.size());
			if (width > width_hint && width_hint > 0)
				width = width_hint;
			if (width < width_hint) {
				if (type == AST_ADD || type == AST_SUB || type == AST_DIV)
					width++;
				if (type == AST_SUB && (!children[0]->is_signed || !children[1]->is_signed))
					width = width_hint;
				if (type == AST_MUL)
					width = min(left.size() + right.size(), width_hint);
			}
		#else
			int width = max(max(left.size(), right.size()), width_hint);
		#endif
			is_signed = children[0]->is_signed && children[1]->is_signed;
			return binop2rtlil(this, type_name, width, left, right);
		}

	// generate cells for binary operations: $logic_and, $logic_or
	if (0) { case AST_LOGIC_AND: type_name = ID($logic_and); }
	if (0) { case AST_LOGIC_OR:  type_name = ID($logic_or); }
		{
			if (keep_sva_cells)
				refuse_sva_operand(this);
			RTLIL::SigSpec left = children[0]->genRTLIL();
			RTLIL::SigSpec right = children[1]->genRTLIL();
			return binop2rtlil(this, type_name, max(width_hint, 1), left, right);
		}

	// generate cells for unary operations: $logic_not
	case AST_LOGIC_NOT:
		{
			if (keep_sva_cells)
				refuse_sva_operand(this);
			RTLIL::SigSpec arg = children[0]->genRTLIL();
			return uniop2rtlil(this, ID($logic_not), max(width_hint, 1), arg);
		}

	// generate multiplexer for ternary operator (aka ?:-operator)
	case AST_TERNARY:
		{
			if (keep_sva_cells)
				refuse_sva_operand(this);
			if (width_hint < 0)
				detectSignWidth(width_hint, sign_hint);
			is_signed = sign_hint;

			RTLIL::SigSpec cond = children[0]->genRTLIL();
			RTLIL::SigSpec sig;

			if (cond.is_fully_def())
			{
				if (cond.as_bool()) {
					sig = children[1]->genRTLIL(width_hint, sign_hint);
					log_assert(is_signed == children[1]->is_signed);
				} else {
					sig = children[2]->genRTLIL(width_hint, sign_hint);
					log_assert(is_signed == children[2]->is_signed);
				}

				widthExtend(this, sig, sig.size(), is_signed);
			}
			else
			{
				RTLIL::SigSpec val1 = children[1]->genRTLIL(width_hint, sign_hint);
				RTLIL::SigSpec val2 = children[2]->genRTLIL(width_hint, sign_hint);

				if (cond.size() > 1)
					cond = uniop2rtlil(this, ID($reduce_bool), 1, cond, false);

				int width = max(val1.size(), val2.size());
				log_assert(is_signed == children[1]->is_signed);
				log_assert(is_signed == children[2]->is_signed);
				widthExtend(this, val1, width, is_signed);
				widthExtend(this, val2, width, is_signed);

				sig = mux2rtlil(this, cond, val1, val2);
			}

			if (sig.size() < width_hint)
				sig.extend_u0(width_hint, sign_hint);
			return sig;
		}

	// generate $memrd cells for memory read ports
	case AST_MEMRD:
		{
			std::stringstream sstr;
			sstr << "$memrd$" << str << "$" << RTLIL::encode_filename(*location.begin.filename) << ":" << location.begin.line << "$" << (autoidx++);

			RTLIL::Cell *cell = current_module->addCell(sstr.str(), ID($memrd));
			set_src_attr(cell, this);

			RTLIL::Wire *wire = current_module->addWire(cell->name.str() + "_DATA", current_module->memories[str]->width);
			set_src_attr(wire, this);

			int mem_width, mem_size, addr_bits;
			is_signed = id2ast->is_signed;
			wire->is_signed = is_signed;
			id2ast->meminfo(mem_width, mem_size, addr_bits);

			RTLIL::SigSpec addr_sig = children[0]->genRTLIL();

			cell->setPort(ID::CLK, RTLIL::SigSpec(RTLIL::State::Sx, 1));
			cell->setPort(ID::EN, RTLIL::SigSpec(RTLIL::State::Sx, 1));
			cell->setPort(ID::ADDR, addr_sig);
			cell->setPort(ID::DATA, RTLIL::SigSpec(wire));

			cell->parameters[ID::MEMID] = RTLIL::Const(str);
			cell->parameters[ID::ABITS] = RTLIL::Const(GetSize(addr_sig));
			cell->parameters[ID::WIDTH] = RTLIL::Const(wire->width);

			cell->parameters[ID::CLK_ENABLE] = RTLIL::Const(0);
			cell->parameters[ID::CLK_POLARITY] = RTLIL::Const(0);
			cell->parameters[ID::TRANSPARENT] = RTLIL::Const(0);

			if (!sign_hint)
				is_signed = false;

			return RTLIL::SigSpec(wire);
		}

	// generate $meminit cells
	case AST_MEMINIT:
		{
			std::stringstream sstr;
			sstr << "$meminit$" << str << "$" << RTLIL::encode_filename(*location.begin.filename) << ":" << location.begin.line << "$" << (autoidx++);

			SigSpec en_sig = children[2]->genRTLIL();

			RTLIL::Cell *cell = current_module->addCell(sstr.str(), ID($meminit_v2));
			set_src_attr(cell, this);

			int mem_width, mem_size, addr_bits;
			id2ast->meminfo(mem_width, mem_size, addr_bits);

			if (children[3]->type != AST_CONSTANT)
				input_error("Memory init with non-constant word count!\n");
			int num_words = int(children[3]->asInt(false));
			cell->parameters[ID::WORDS] = RTLIL::Const(num_words);

			SigSpec addr_sig = children[0]->genRTLIL();

			cell->setPort(ID::ADDR, addr_sig);
			cell->setPort(ID::DATA, children[1]->genWidthRTLIL(current_module->memories[str]->width * num_words, true));
			cell->setPort(ID::EN, en_sig);

			cell->parameters[ID::MEMID] = RTLIL::Const(str);
			cell->parameters[ID::ABITS] = RTLIL::Const(GetSize(addr_sig));
			cell->parameters[ID::WIDTH] = RTLIL::Const(current_module->memories[str]->width);

			cell->parameters[ID::PRIORITY] = RTLIL::Const(autoidx-1);
		}
		break;

	// generate $check cells
	case AST_ASSERT:
	case AST_ASSUME:
	case AST_LIVE:
	case AST_FAIR:
	case AST_COVER:
		{
			std::string flavor, desc;
			if (type == AST_ASSERT) { flavor = "assert"; desc = "assert property ()"; }
			if (type == AST_ASSUME) { flavor = "assume"; desc = "assume property ()"; }
			if (type == AST_LIVE) { flavor = "live"; desc = "assert property (eventually)"; }
			if (type == AST_FAIR) { flavor = "fair"; desc = "assume property (eventually)"; }
			if (type == AST_COVER) { flavor = "cover"; desc = "cover property ()"; }

			IdString cellname;
			if (str.empty())
				cellname = stringf("$%s$%s:%d$%d", flavor, RTLIL::encode_filename(*location.begin.filename), location.begin.line, autoidx++);
			else
				cellname = str;
			check_unique_id(current_module, cellname, this, "procedural assertion");

			RTLIL::SigSpec check = children[0]->genRTLIL();
			if (GetSize(check) != 1)
				check = current_module->ReduceBool(NEW_ID, check);

			RTLIL::Cell *cell = current_module->addCell(cellname, ID($check));
			set_src_attr(cell, this);
			for (auto &attr : attributes) {
				if (attr.second->type != AST_CONSTANT)
					input_error("Attribute `%s' with non-constant value!\n", attr.first);
				cell->attributes[attr.first] = attr.second->asAttrConst();
			}
			cell->setParam(ID(FLAVOR), flavor);
			cell->parameters[ID::TRG_WIDTH] = 0;
			cell->parameters[ID::TRG_ENABLE] = 0;
			cell->parameters[ID::TRG_POLARITY] = 0;
			cell->parameters[ID::PRIORITY] = 0;
			cell->setPort(ID::TRG, RTLIL::SigSpec());
			cell->setPort(ID::EN, RTLIL::S1);
			cell->setPort(ID::A, check);

			// No message is emitted to ensure Verilog code roundtrips correctly.
			Fmt fmt;
			fmt.emit_rtlil(cell);
		}
		break;

	// add entries to current_module->connections for assignments (outside of always blocks)
	case AST_ASSIGN:
		{
			if (keep_sva_cells)
				refuse_sva_operand(this);
			RTLIL::SigSpec left = children[0]->genRTLIL();
			RTLIL::SigSpec right = children[1]->genWidthRTLIL(left.size(), true);
			if (left.has_const()) {
				RTLIL::SigSpec new_left, new_right;
				for (int i = 0; i < GetSize(left); i++)
					if (left[i].wire) {
						new_left.append(left[i]);
						new_right.append(right[i]);
					}
				log_file_warning(location.to_loc(), "Ignoring assignment to constant bits:\n"
						"    old assignment: %s = %s\n    new assignment: %s = %s.\n",
						log_signal(left), log_signal(right),
						log_signal(new_left), log_signal(new_right));
				left = new_left;
				right = new_right;
			}
			current_module->connect(RTLIL::SigSig(left, right));
		}
		break;

	// create an RTLIL::Cell for an AST_CELL
	case AST_CELL:
		{
			int port_counter = 0, para_counter = 0;

			RTLIL::IdString id = str;
			check_unique_id(current_module, id, this, "cell");
			RTLIL::Cell *cell = current_module->addCell(id, "");
			set_src_attr(cell, this);

			for (auto it = children.begin(); it != children.end(); it++) {
				auto* child = it->get();
				if (child->type == AST_CELLTYPE) {
					cell->type = child->str;
					if (flag_icells && cell->type.begins_with("\\$"))
						cell->type = cell->type.substr(1);
					continue;
				}
				if (child->type == AST_PARASET) {
					IdString paraname = child->str.empty() ? stringf("$%d", ++para_counter) : child->str;
					const auto* value = child->children[0].get();
					if (value->type == AST_REALVALUE)
						log_file_warning(location.to_loc(), "Replacing floating point parameter %s.%s = %f with string.\n",
								cell, paraname.unescape(), value->realvalue);
					else if (value->type != AST_CONSTANT) {
						// IEEE 1800-2017 6.20.3: a type parameter override
						// arrives as a non-constant expression naming a
						// typedef. Carry the resolved (package-qualified)
						// typedef name through as a string parameter so
						// derive can splice the type definition into the
						// parametric module.
						if (value->type == AST_IDENTIFIER && value->id2ast && value->id2ast->type == AST_TYPEDEF) {
							// a typedef of THIS module is invisible to the child:
							// qualify it `module::name` so derive can look it up in
							// this module's AST (register v3 D0); file-scope and
							// package typedefs keep their own names
							std::string tn = value->id2ast->str;
							bool local = false;
							if (current_ast_mod && current_ast_mod->type == AST_MODULE)
								for (auto &c : current_ast_mod->children)
									if (c.get() == value->id2ast) {
										local = true;
										break;
									}
							if (local && tn.compare(0, 1, "\\") == 0 && tn.find("::") == std::string::npos)
								tn = current_ast_mod->str + "::" + tn.substr(1);
							cell->parameters[paraname] = RTLIL::Const(tn);
							continue;
						}
						cell->parameters[paraname] = RTLIL::Const(0);
						continue;
					}
					cell->parameters[paraname] = value->asParaConst();
					continue;
				}
				if (child->type == AST_ARGUMENT) {
					RTLIL::SigSpec sig;
					if (child->children.size() > 0) {
						auto* arg = child->children[0].get();
						int local_width_hint = -1;
						bool local_sign_hint = false;
						// don't inadvertently attempt to detect the width of interfaces
						if (arg->type != AST_IDENTIFIER || !arg->id2ast || arg->id2ast->type != AST_CELL)
							arg->detectSignWidth(local_width_hint, local_sign_hint);
						sig = arg->genRTLIL(local_width_hint, local_sign_hint);
						log_assert(local_sign_hint == arg->is_signed);
						if (sig.is_wire()) {
							// if the resulting SigSpec is a wire, its
							// signedness should match that of the AstNode
							if (arg->type == AST_IDENTIFIER && arg->id2ast && arg->id2ast->is_signed && !arg->is_signed)
								// fully-sliced signed wire will be resolved
								// once the module becomes available
								log_assert(attributes.count(ID::reprocess_after));
							else
								log_assert(arg->is_signed == sig.as_wire()->is_signed);
						} else if (arg->is_signed) {
							// non-trivial signed nodes are indirected through
							// signed wires to enable sign extension
							RTLIL::IdString wire_name = NEW_ID;
							RTLIL::Wire *wire = current_module->addWire(wire_name, GetSize(sig));
							wire->is_signed = true;
							current_module->connect(wire, sig);
							sig = wire;
						}
					}
					if (child->str.size() == 0) {
						char buf[100];
						snprintf(buf, 100, "$%d", ++port_counter);
						cell->setPort(buf, sig);
					} else {
						cell->setPort(child->str, sig);
					}
					continue;
				}
				log_abort();
			}

			// Set attribute 'module_not_derived' which will be cleared again after the hierarchy pass
			if (cell->type.isPublic())
				cell->set_bool_attribute(ID::module_not_derived);

			for (auto &attr : attributes) {
				if (attr.second->type != AST_CONSTANT)
					input_error("Attribute `%s' with non-constant value.\n", attr.first);
				cell->attributes[attr.first] = attr.second->asAttrConst();
			}
			if (cell->type == ID($specify2)) {
				int src_width = GetSize(cell->getPort(ID::SRC));
				int dst_width = GetSize(cell->getPort(ID::DST));
				bool full = cell->getParam(ID::FULL).as_bool();
				if (!full && src_width != dst_width)
					input_error("Parallel specify SRC width does not match DST width.\n");
				cell->setParam(ID::SRC_WIDTH, Const(src_width));
				cell->setParam(ID::DST_WIDTH, Const(dst_width));
			}
			else if (cell->type ==  ID($specify3)) {
				int dat_width = GetSize(cell->getPort(ID::DAT));
				int dst_width = GetSize(cell->getPort(ID::DST));
				if (dat_width != dst_width)
					input_error("Specify DAT width does not match DST width.\n");
				int src_width = GetSize(cell->getPort(ID::SRC));
				cell->setParam(ID::SRC_WIDTH, Const(src_width));
				cell->setParam(ID::DST_WIDTH, Const(dst_width));
			}
			else if (cell->type == ID($specrule)) {
				int src_width = GetSize(cell->getPort(ID::SRC));
				int dst_width = GetSize(cell->getPort(ID::DST));
				cell->setParam(ID::SRC_WIDTH, Const(src_width));
				cell->setParam(ID::DST_WIDTH, Const(dst_width));
			}
		}
		break;

	// use ProcessGenerator for always blocks
	case AST_ALWAYS: {
			ProcessGenerator generator(this->clone());
			ignoreThisSignalsInInitial.append(generator.outputSignals);
		} break;

	case AST_INITIAL: {
			auto always = this->clone();
			ProcessGenerator generator(this->clone(), ignoreThisSignalsInInitial);
		} break;

	case AST_TECALL: {
			int sz = children.size();
			if (str == "$info") {
				if (sz > 0)
					log_file_info(location.to_loc(), "%s.\n", children[0]->str);
				else
					log_file_info(location.to_loc(), "\n");
			} else if (str == "$warning") {
				if (sz > 0)
					log_file_warning(location.to_loc(), "%s.\n", children[0]->str);
				else
					log_file_warning(location.to_loc(), "\n");
			} else if (str == "$error") {
				if (sz > 0)
					log_file_warning(location.to_loc(), "%s.\n", children[0]->str.c_str());
			} else if (str == "$fatal") {
				if (sz > 0)
					log_file_warning(location.to_loc(), "FATAL: %s.\n", children[0]->str.c_str());
				else
					log_file_warning(location.to_loc(), "FATAL.\n");
			} else {
				input_error("Unknown elaboration system task '%s'.\n", str);
			}
		} break;

	case AST_FCALL: {
			if (str == "\\$anyconst" || str == "\\$anyseq" || str == "\\$allconst" || str == "\\$allseq")
			{
				string myid = stringf("%s$%d", str.c_str() + 1, autoidx++);
				int width = width_hint;

				if (GetSize(children) > 1)
					input_error("System function %s got %d arguments, expected 1 or 0.\n",
							RTLIL::unescape_id(str), GetSize(children));

				if (GetSize(children) == 1) {
					if (children[0]->type != AST_CONSTANT)
						input_error("System function %s called with non-const argument!\n",
								RTLIL::unescape_id(str));
					width = children[0]->asInt(true);
				}

				if (width <= 0)
					input_error("Failed to detect width of %s!\n", RTLIL::unescape_id(str));

				Cell *cell = current_module->addCell(myid, str.substr(1));
				set_src_attr(cell, this);
				cell->parameters[ID::WIDTH] = width;

				if (attributes.count(ID::reg)) {
					auto &attr = attributes.at(ID::reg);
					if (attr->type != AST_CONSTANT)
						input_error("Attribute `reg' with non-constant value!\n");
					cell->attributes[ID::reg] =  attr->asAttrConst();
				}

				Wire *wire = current_module->addWire(myid + "_wire", width);
				set_src_attr(wire, this);
				cell->setPort(ID::Y, wire);

				is_signed = sign_hint;
				return SigSpec(wire);
			}
			if (str == "\\$past" || str == "\\$rose" || str == "\\$fell" || str == "\\$stable" ||
					str == "\\$changed" || str == "\\$sampled") {
				if (children.empty())
					input_error("System function %s missing argument.\n", RTLIL::unescape_id(str));
				// No enclosing clocked block (concurrent assertion context):
				// keep the call as a real cell so sva2check can lower it
				// against the property's sampling clock.  $sampled(e) == e
				// (the current sampled value), folded here for that reason.
				if (str == "\\$sampled") {
					RTLIL::SigSpec sig = children[0]->genRTLIL();
					is_signed = sign_hint;
					return sig;
				}
				RTLIL::SigSpec asig = children[0]->genRTLIL();
				int ywidth = str == "\\$past" ? std::max(1, GetSize(asig)) : 1;
				RTLIL::Wire *ywire = current_module->addWire(NEW_ID, ywidth);
				set_src_attr(ywire, this);
				Cell *cell = current_module->addCell(NEW_ID, str);
				set_src_attr(cell, this);
				cell->setPort(ID(A), asig);
				cell->setPort(ID(Y), ywire);
				if (str == "\\$past") {
					if (GetSize(children) > 2)
						input_error("System function %s got %d arguments, expected at most 2.\n",
								RTLIL::unescape_id(str), int(GetSize(children)));
					RTLIL::SigSpec steps = GetSize(children) == 2 ? children[1]->genRTLIL() : RTLIL::SigSpec(1, 32);
					cell->setPort(ID(B), steps);
				} else {
					// an explicit clocking-event argument, if any, is dropped:
					// the property's own sampling clock is used instead
					cell->setPort(ID(B), RTLIL::SigSpec(0, 32));
				}
				is_signed = sign_hint;
				return SigSpec(ywire);
			}
			if (keep_sva_cells && str.size() >= 2 && str[1] == '$') {
				// A system function inside a concurrent-assertion expression.
				// Until register v3 C3 (2026-10-17) every such call became a
				// `$sva` call cell whose Y wire nothing ever drove: sva2check
				// only compiles $sva cells it reaches as sequence/property
				// operands, so `!$isunknown(x)` (OpenTitan's ASSERT_KNOWN),
				// `$countones(x) <= 1` and `$size(x) == 4` were verdicts on a
				// free 1-bit wire (DIS @0 with no circuit in the cone).  The
				// circuit-mutation leg found 202 such disproofs.  Compile the
				// ones with a two-state RTLIL meaning here; refuse the rest.
				if (str == "\\$isunknown") {
					if (GetSize(children) != 1)
						input_error("System function %s got %d arguments, expected 1.\n",
								RTLIL::unescape_id(str), int(GetSize(children)));
					children[0]->genRTLIL();   // elaborate the argument (width errors etc.)
					log_file_warning(location.to_loc(),
							"$isunknown in a concurrent assertion is constant 0 (two-state semantics for every "
							"-formal consumer, sim included; ASSERT_KNOWN holds trivially).\n");
					is_signed = false;
					return RTLIL::SigSpec(RTLIL::State::S0);
				}
				if (str == "\\$countones" || str == "\\$onehot" || str == "\\$onehot0") {
					if (GetSize(children) != 1)
						input_error("System function %s got %d arguments, expected 1.\n",
								RTLIL::unescape_id(str), int(GetSize(children)));
					RTLIL::SigSpec arg = children[0]->genRTLIL();
					int n = GetSize(arg);
					int cw = 1;
					while ((1 << cw) <= n)
						cw++;
					// popcount: a balanced adder tree over the bits
					std::vector<RTLIL::SigSpec> terms;
					for (int i = 0; i < n; i++)
						terms.push_back(RTLIL::SigSpec(arg[i]));
					while (GetSize(terms) > 1) {
						std::vector<RTLIL::SigSpec> next;
						for (int i = 0; i + 1 < GetSize(terms); i += 2) {
							RTLIL::Wire *w = current_module->addWire(NEW_ID, cw);
							set_src_attr(w, this);
							RTLIL::Cell *add = current_module->addAdd(NEW_ID, terms[i], terms[i + 1], w, false);
							set_src_attr(add, this);
							next.push_back(RTLIL::SigSpec(w));
						}
						if (GetSize(terms) % 2)
							next.push_back(terms.back());
						terms = next;
					}
					RTLIL::SigSpec cnt = terms.empty() ? RTLIL::SigSpec(0, cw) : terms[0];
					if (GetSize(cnt) < cw)
						cnt.extend_u0(cw, false);
					is_signed = false;
					if (str == "\\$countones")
						return cnt;
					RTLIL::Wire *y = current_module->addWire(NEW_ID, 1);
					set_src_attr(y, this);
					RTLIL::Cell *cmp = (str == "\\$onehot")
							? current_module->addEq(NEW_ID, cnt, RTLIL::SigSpec(1, cw), y, false)
							: current_module->addLe(NEW_ID, cnt, RTLIL::SigSpec(1, cw), y, false);
					set_src_attr(cmp, this);
					return RTLIL::SigSpec(y);
				}
				if (str == "\\$bits" || str == "\\$size") {
					// elaboration-time width of a packed expression: genrtlil
					// can still compute it (simplify's fold does not run inside
					// property expressions)
					if (GetSize(children) != 1)
						input_error("System function %s inside a concurrent assertion needs exactly one argument.\n",
								RTLIL::unescape_id(str));
					int w = 0;
					bool sg = false;
					children[0]->detectSignWidth(w, sg);
					is_signed = false;
					return RTLIL::SigSpec(w, 32);
				}
				if (str == "\\$high" || str == "\\$low" || str == "\\$left" || str == "\\$right") {
					// bounds of a declared packed vector (register v3 D8: these
					// used to be undriven placeholders, then loud errors)
					AstNode *arg = GetSize(children) == 1 ? children[0].get() : nullptr;
					AstNode *decl = (arg && arg->type == AST_IDENTIFIER) ? arg->id2ast : nullptr;
					AstNode *rng = (decl && !decl->children.empty() && decl->children[0] &&
							decl->children[0]->type == AST_RANGE && decl->children[0]->range_valid)
							? decl->children[0].get() : nullptr;
					if (!rng)
						input_error("System function %s inside a concurrent assertion needs a declared packed "
								"vector as its (only) argument.\n", RTLIL::unescape_id(str));
					int l = rng->range_left, r = rng->range_right;
					if (rng->range_swapped)   // declared [lo:hi]: the AST keeps it normalized
						std::swap(l, r);
					int v = str == "\\$high" ? std::max(l, r) : str == "\\$low" ? std::min(l, r)
							: str == "\\$left" ? l : r;
					is_signed = true;
					return RTLIL::SigSpec(RTLIL::Const(v, 32));
				}
				if (str == "\\$clog2") {
					if (GetSize(children) != 1)
						input_error("System function %s got %d arguments, expected 1.\n",
								RTLIL::unescape_id(str), int(GetSize(children)));
					RTLIL::SigSpec a = children[0]->genRTLIL();
					if (!a.is_fully_const())
						input_error("System function $clog2 inside a concurrent assertion needs a constant argument.\n");
					RTLIL::Const c = a.as_const();
					int v = 0;
					while (c.as_int() > (1 << v) && v < 31)
						v++;
					is_signed = false;
					return RTLIL::SigSpec(c.as_int() <= 1 ? 0 : v, 32);
				}
				// No silent success: an unsupported system function inside a
				// property must not become a free wire.
				input_error("System function %s is not supported inside a concurrent assertion "
						"(it would become an undriven placeholder).\n", RTLIL::unescape_id(str));
			}
			if (str.size() < 2 || str[1] != '$') {
			if (!keep_sva_cells)
				return RTLIL::SigSpec(RTLIL::State::S1);
			RTLIL::Wire *ywire = current_module->addWire(NEW_ID, 1);
				set_src_attr(ywire, this);
				// No const-1 tie: the A port must be driven solely by this
				// tree so opt between read and sva2check cannot fold the
				// placeholder, and an uncompiled property reads x (loud)
				// instead of passing vacuously.
				RTLIL::Cell *cell = current_module->addCell(NEW_ID, "\\$sva");
				set_src_attr(cell, this);
				cell->setParam("\\OP", RTLIL::Const(str));
				cell->setPort(ID::Y, ywire);
				// Named sequence/property instance: the actual arguments are
				// wired into the argument ports (uniform 32-bit width).
				static const IdString arg_ports[] = {ID::A, ID::B, ID::C, ID::D, ID::E};
				int argi = 0;
				for (auto &child : children) {
					if (!child)
						continue;
					RTLIL::SigSpec sig = child->genRTLIL();
					if (sig.empty())
						sig = RTLIL::SigSpec(RTLIL::State::S1);
					if (child->type != AST_SVA && GetSize(sig) != 1 && !sig.is_fully_const())
						sig = current_module->ReduceBool(NEW_ID, sig);
					if (GetSize(sig) < 32)
						sig.extend_u0(32, false);
					IdString port;
					if (argi < 5)
						port = arg_ports[argi];
					else
						port = stringf("\\P%d", argi).c_str();
					cell->setPort(port, sig);
					argi++;
				}
				for (auto port : {ID::A, ID::B, ID::C, ID::D, ID::E})
					if (!cell->hasPort(port))
						cell->setPort(port, Const(0, 32));
				cell->setParam("\\NARGS", argi);
				is_signed = false;
				return RTLIL::SigSpec(ywire);
			}
		}
		YS_FALLTHROUGH

	// Sequence and property operators. The tree is kept as $sva cells. A single
	// boolean net is returned so assert/assume/cover can still build a $check.
	// It is not a temporal evaluator: plain booleans stay ordinary expressions.
	case AST_SVA:
		// A checker declaration is a template: instantiations were inlined
		// during simplify, and an uninstantiated checker compiles to
		// nothing.
		if (str == "checker_decl")
			break;
		{
			static bool warned = false;
			if (!warned && str != "sequence_decl" && str != "property_decl" && str != "let_decl") {
				warned = true;
				log_file_warning(location.to_loc(), "SVA temporal operators are parsed into $sva cells. The assert $check uses a placeholder, not a full cycle-accurate evaluator.\n");
			}

			// `(e, w = expr)`: the match-item target is a local variable of the
			// assertion (the fork accepts it undeclared, as the battery does);
			// record it so the identifier path does not treat its wire as an
			// implicit net (which marked the module sv_degraded on a strict
			// read, and B5's refusal then rejected the file)
			if (str == "assign" && !children.empty() && children[0] && children[0]->type == AST_IDENTIFIER)
				sva_local_names[current_module].insert(children[0]->str);

			auto child_sig = [&](AstNode *child) -> RTLIL::SigSpec {
				if (!child)
					return RTLIL::SigSpec(RTLIL::State::S1);
				bool is_temporal = child->type == AST_SVA;
				RTLIL::SigSpec sig = child->genRTLIL();
				if (sig.empty())
					sig = RTLIL::SigSpec(RTLIL::State::S1);
				// Temporal children produce a 1-bit placeholder wire; plain
				// expressions keep their width (inside/weq operands).  Fully
				// constant children keep full width either way: delay values
				// and repetition bounds are carried in the low bits.
				if (is_temporal && GetSize(sig) != 1)
					sig = current_module->ReduceBool(NEW_ID, sig);
				return sig;
			};

			bool is_decl = str == "sequence_decl" || str == "property_decl" || str == "let_decl";
			int first_body = 0;
			std::string decl_name;
			if (is_decl && !children.empty()) {
				decl_name = children[0]->str;
				first_body = 1;
			}

			if (!keep_sva_cells)
				return RTLIL::SigSpec(RTLIL::State::S1);

			RTLIL::Wire *ywire = current_module->addWire(NEW_ID, 1);
			set_src_attr(ywire, this);
			// See the AST_FCALL case above: no const-1 tie on the
			// placeholder; the tree must stay the A-port driver.

			RTLIL::Cell *cell = current_module->addCell(NEW_ID, "\\$sva");
			set_src_attr(cell, this);
			cell->setParam("\\OP", RTLIL::Const(str));
			if (is_decl)
				cell->setParam("\\NAME", RTLIL::Const(decl_name));
			cell->setPort(ID::Y, ywire);

			static const IdString arg_ports[] = {ID::A, ID::B, ID::C, ID::D, ID::E};
			int argi = 0;
			for (int i = first_body; i < GetSize(children); i++) {
				RTLIL::SigSpec sig = child_sig(children[i].get());
				// All argument ports share one width so that later design
				// normalization (hierarchy) cannot resize individual ports
				// with padding bits; numeric args (delays, bounds) keep
				// their full 32-bit constant value in the low bits.
				if (GetSize(sig) < 32)
					sig.extend_u0(32, false);
				IdString port;
				if (argi < 5)
					port = arg_ports[argi];
				else
					port = stringf("\\P%d", argi).c_str();
				cell->setPort(port, sig);
				argi++;
			}
			for (auto port : {ID::A, ID::B, ID::C, ID::D, ID::E})
				if (!cell->hasPort(port))
					cell->setPort(port, Const(0, 32));
			cell->setParam("\\NARGS", argi);

			is_signed = false;
			return RTLIL::SigSpec(ywire);
		}

	case AST_PREFIX: {
			simplify(true, 1, -1, false);
			if (type != AST_PREFIX)
				return genRTLIL(width_hint, sign_hint);
			degrade(location.to_loc(), "Hierarchical prefix did not resolve; using constant 0.\n");
			int width = width_hint > 0 ? width_hint : 1;
			return RTLIL::SigSpec(RTLIL::S0, width);
		}

	// everything should have been handled above -> print error if not.
	default:
		logger().for_each_sink([&](LogSink &sink) {
			FILE *f = sink.file_handle();
			if (f)
				current_ast_mod->dumpAst(f, "verilog-ast> ");
		});
		input_error("Don't know how to generate RTLIL code for %s node!\n", type2str(type));
	}

	return RTLIL::SigSpec();
}

// this is a wrapper for AstNode::genRTLIL() when a specific signal width is requested and/or
// signals must be substituted before being used as input values (used by ProcessGenerator)
// note that this is using some global variables to communicate this special settings to AstNode::genRTLIL().
RTLIL::SigSpec AstNode::genWidthRTLIL(int width, bool sgn, const dict<RTLIL::SigBit, RTLIL::SigBit> *new_subst_ptr)
{
	const dict<RTLIL::SigBit, RTLIL::SigBit> *backup_subst_ptr = genRTLIL_subst_ptr;

	if (new_subst_ptr)
		genRTLIL_subst_ptr = new_subst_ptr;

	bool sign_hint = sgn;
	int width_hint = width;
	detectSignWidthWorker(width_hint, sign_hint);
	RTLIL::SigSpec sig = genRTLIL(width_hint, sign_hint);

	genRTLIL_subst_ptr = backup_subst_ptr;

	if (width >= 0)
		sig.extend_u0(width, is_signed);

	return sig;
}

YOSYS_NAMESPACE_END
