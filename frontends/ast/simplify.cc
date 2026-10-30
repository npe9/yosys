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
#include "libs/sha1/sha1.h"
#include "frontends/verilog/verilog_frontend.h"
#include "ast.h"
#include "kernel/io.h"

#include <sstream>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <optional>
#include <numeric>

YOSYS_NAMESPACE_BEGIN

using namespace AST;
using namespace AST_INTERNAL;

void AstNode::set_in_lvalue_flag(bool flag, bool no_descend)
{
	if (flag != in_lvalue_from_above) {
		in_lvalue_from_above = flag;
		if (!no_descend)
			fixup_hierarchy_flags();
	}
}

void AstNode::set_in_param_flag(bool flag, bool no_descend)
{
	if (flag != in_param_from_above) {
		in_param_from_above = flag;
		if (!no_descend)
			fixup_hierarchy_flags();
	}
}

void AstNode::fixup_hierarchy_flags(bool force_descend)
{
	// With forced descend, we disable the implicit
	// descend from within the set_* functions, instead
	// we do an explicit descend at the end of this function

	in_param = in_param_from_above;

	switch (type) {
	case AST_PARAMETER:
	case AST_LOCALPARAM:
	case AST_DEFPARAM:
	case AST_PARASET:
	case AST_PREFIX:
		in_param = true;
		for (auto& child : children)
			child->set_in_param_flag(true, force_descend);
		break;

	case AST_REPLICATE:
	case AST_WIRE:
	case AST_GENIF:
	case AST_GENCASE:
		for (auto& child : children)
			child->set_in_param_flag(in_param, force_descend);
		if (children.size() >= 1)
			children[0]->set_in_param_flag(true, force_descend);
		break;

	case AST_ASSIGN_PATTERN:
		for (auto& child : children)
			child->set_in_param_flag(in_param, force_descend);
		break;

	case AST_GENFOR:
	case AST_FOR:
		for (auto& child : children) {
			log_assert((bool)child);
			child->set_in_param_flag(in_param, force_descend);
		}
		if (children.size() >= 2)
			children[1]->set_in_param_flag(true, force_descend);
		break;

	default:
		in_param = in_param_from_above;
		for (auto& child : children)
			child->set_in_param_flag(in_param, force_descend);
	}

	for (auto& attr : attributes)
		attr.second->set_in_param_flag(true, force_descend);

	in_lvalue = in_lvalue_from_above;

	switch (type) {
	case AST_ASSIGN:
	case AST_ASSIGN_EQ:
	case AST_ASSIGN_LE:
		if (children.size() >= 1)
			children[0]->set_in_lvalue_flag(true, force_descend);
		if (children.size() >= 2)
			children[1]->set_in_lvalue_flag(in_lvalue, force_descend);
		break;

	default:
		for (auto& child : children)
			child->set_in_lvalue_flag(in_lvalue, force_descend);
	}

	if (force_descend) {
		for (auto& child : children)
			child->fixup_hierarchy_flags(true);
		for (auto& attr : attributes)
			attr.second->fixup_hierarchy_flags(true);
	}
}

// Process a format string and arguments for $display, $write, $sprintf, etc

Fmt AstNode::processFormat(int stage, bool sformat_like, int default_base, size_t first_arg_at, bool may_fail) {
	std::vector<VerilogFmtArg> args;
	for (size_t index = first_arg_at; index < children.size(); index++) {
		AstNode *node_arg = children[index].get();
		while (node_arg->simplify(true, stage, -1, false)) { }

		VerilogFmtArg arg = {};
		arg.src = location.to_loc();
		if (node_arg->type == AST_CONSTANT && node_arg->is_string) {
			arg.type = VerilogFmtArg::STRING;
			arg.str = node_arg->bitsAsConst().decode_string();
			// and in case this will be used as an argument...
			arg.sig = node_arg->bitsAsConst();
			arg.signed_ = false;
		} else if (node_arg->type == AST_IDENTIFIER && node_arg->str == "$time") {
			arg.type = VerilogFmtArg::TIME;
		} else if (node_arg->type == AST_IDENTIFIER && node_arg->str == "$realtime") {
			arg.type = VerilogFmtArg::TIME;
			arg.realtime = true;
		} else if (node_arg->type == AST_CONSTANT) {
			arg.type = VerilogFmtArg::INTEGER;
			arg.sig = node_arg->bitsAsConst();
			arg.signed_ = node_arg->is_signed;
		} else if (may_fail) {
			log_file_info(arg.src, "Skipping system task `%s' with non-constant argument at position %zu.\n", str, index + 1);
			return Fmt();
		} else {
			log_file_error(arg.src, "Failed to evaluate system task `%s' with non-constant argument at position %zu.\n", str, index + 1);
		}
		args.push_back(arg);
	}

	Fmt fmt;
	fmt.parse_verilog(args, sformat_like, default_base, /*task_name=*/str, current_module->name);
	return fmt;
}

void AstNode::annotateTypedEnums(AstNode *template_node)
{
	//check if enum
	if (template_node->attributes.count(ID::enum_type)) {
		//get reference to enum node:
		std::string enum_type = template_node->attributes[ID::enum_type]->str.c_str();
		//			log("enum_type=%s (count=%lu)\n", enum_type, current_scope.count(enum_type));
		//			log("current scope:\n");
		//			for (auto &it : current_scope)
		//				log("  %s\n", it.first);
		log_assert(current_scope.count(enum_type) == 1);
		AstNode *enum_node = current_scope.at(enum_type);
		log_assert(enum_node->type == AST_ENUM);
		while (enum_node->simplify(true, 1, -1, false)) { }
		//get width from 1st enum item:
		log_assert(enum_node->children.size() >= 1);
		AstNode *enum_item0 = enum_node->children[0].get();
		log_assert(enum_item0->type == AST_ENUM_ITEM);
		int width;
		bool base_signed = false;
		if (!enum_item0->range_valid) {
			// Typed enums (typedef enum base_t {...}) inherit the packed
			// base type's range; each item carries the base type as an
			// AST_WIRETYPE child instead of an explicit range node.
			width = -1;
			for (auto &ch : enum_item0->children) {
				if (ch->type != AST_WIRETYPE)
					continue;
				AstNode *tdef = current_scope.count(ch->str) ? current_scope.at(ch->str) : nullptr;
				AstNode *twire = tdef;
				if (twire && twire->type == AST_TYPEDEF && !twire->children.empty())
					twire = twire->children[0].get();
				if (twire && twire->type == AST_WIRE) {
					base_signed = twire->is_signed;
					if (twire->range_valid)
						width = std::max(twire->range_left, twire->range_right) -
							std::min(twire->range_left, twire->range_right) + 1;
					else if (!twire->children.empty() && twire->children[0]->type == AST_RANGE &&
							twire->children[0]->range_valid)
						width = twire->children[0]->range_left - twire->children[0]->range_right + 1;
				}
			}
			if (width < 1)
				width = 1;
		}
		else if (enum_item0->range_swapped)
			width = enum_item0->range_right - enum_item0->range_left + 1;
		else
			width = enum_item0->range_left - enum_item0->range_right + 1;
		if (width <= 0)
			width = 1;
		//add declared enum items:
		for (auto& enum_item : enum_node->children){
			log_assert(enum_item->type == AST_ENUM_ITEM);
			//get is_signed
			bool is_signed;
			if (enum_item->children.size() == 1){
				is_signed = false;
			} else if (enum_item->children.size() == 2){
				if (enum_item->children[1]->type == AST_WIRETYPE) {
					// typed enum: signedness inherited from the packed base
					// type (resolved while computing the width above)
					is_signed = base_signed;
				} else {
					log_assert(enum_item->children[1]->type == AST_RANGE);
					is_signed = enum_item->children[1]->is_signed;
				}
			} else {
				log_error("enum_item children size==%zu, expected 1 or 2 for %s (%s)\n",
						  (size_t) enum_item->children.size(),
						  enum_item->str.c_str(), enum_node->str.c_str()
				);
			}
			//start building attribute string
			std::string enum_item_str = "\\enum_value_";
			//get enum item value
			if(enum_item->children[0]->type != AST_CONSTANT){
				log_error("expected const, got %s for %s (%s)\n",
						  type2str(enum_item->children[0]->type).c_str(),
						  enum_item->str.c_str(), enum_node->str.c_str()
						);
			}
			RTLIL::Const val = enum_item->children[0]->bitsAsConst(width, is_signed);
			enum_item_str.append(val.as_string());
			//set attribute for available val to enum item name mappings
			set_attribute(enum_item_str.c_str(), mkconst_str(location, enum_item->str));
		}
	}
}

static std::unique_ptr<AstNode> make_range(AstSrcLocType loc, int left, int right, bool is_signed = false)
{
	// generate a pre-validated range node for a fixed signal range.
	auto range = std::make_unique<AstNode>(loc, AST_RANGE);
	range->range_left = left;
	range->range_right = right;
	range->range_valid = true;
	range->children.push_back(AstNode::mkconst_int(loc, left, true));
	range->children.push_back(AstNode::mkconst_int(loc, right, true));
	range->is_signed = is_signed;
	return range;
}

// An identifier from a package that was never compiled resolves to the
// implicit 1-bit wire created for it. Such an identifier cannot decide a
// range bound; find it so the bound can be replaced by a constant, like
// other missing-package degradations in this frontend.
// true when `name' (e.g. \\k) is declared somewhere in the module subtree
// under that name or as the last component of a scoped name (\\g.p.k): a
// genvar or a procedural loop variable referenced from a named block is
// still bound to a transient autowire on the first simplify pass, before
// generate expansion and block scoping resolve it (2026-10-22: a genvar in
// `always_comb begin : p ... s[k-1]` inside a generate loop was degraded to
// constant 0 -- ~20 OpenTitan / Veer files FAIL@parse; stock elaborates them)
static bool name_declared_in(AstNode *node, const std::string &name, int depth = 0)
{
	if (!node || depth > 64)
		return false;
	if (node->type != AST_AUTOWIRE && node->type != AST_IDENTIFIER && !node->str.empty() &&
			(node->type == AST_GENVAR || node->type == AST_WIRE || node->type == AST_LOCALPARAM ||
			 node->type == AST_PARAMETER || node->type == AST_MEMORY)) {
		const std::string &s = node->str;
		if (s == name)
			return true;
		std::string tail = "." + name.substr(name[0] == '\\' ? 1 : 0);
		if (s.size() > tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0)
			return true;
	}
	for (auto &child : node->children)
		if (name_declared_in(child.get(), name, depth + 1))
			return true;
	return false;
}

static AstNode *find_missing_autowire(AstNode *node, int depth = 0)
{
	if (node->type == AST_IDENTIFIER && node->id2ast) {
		if (node->id2ast->type == AST_AUTOWIRE)
			return node;
		if (depth < 8 && (node->id2ast->type == AST_PARAMETER || node->id2ast->type == AST_LOCALPARAM))
			for (auto &child : node->id2ast->children)
				if (child) {
					AstNode *found = find_missing_autowire(child.get(), depth + 1);
					if (found)
						return found;
				}
	}
	if (depth >= 8)
		return nullptr;
	for (auto &child : node->children)
		if (child) {
			AstNode *found = find_missing_autowire(child.get(), depth + 1);
			if (found)
				return found;
		}
	return nullptr;
}

// Rewrite references to a flattened unpacked-array port: the leading index
// select becomes a part-select on the wide wire (word width w).
static AstNode *resolve_wiretype_struct(AstNode *decl_node);

static void rewrite_unpacked_port_refs(AstNode *node, const std::string &name, int w, AstNode *port_node)
{
	if (!node)
		return;
	// Element member select on a flattened struct-array port:
	// W[idx].member -> W[idx][member_bits] so the identifier branch below
	// can turn it into a part-select on the wide wire.
	if (node->type == AST_PREFIX && node->str == name && port_node &&
			node->children.size() == 2 && node->children[1]->type == AST_IDENTIFIER) {
		AstNode *stype = resolve_wiretype_struct(port_node);
		if (stype && stype->range_valid) {
			std::string mname = node->children[1]->str;
			if (!mname.empty() && mname[0] == '\\')
				mname = mname.substr(1);
			AstNode *item = nullptr;
			for (auto &cand : stype->children) {
				std::string cname = cand->str;
				if (!cname.empty() && cname[0] == '\\')
					cname = cname.substr(1);
				if (cname == mname) {
					item = cand.get();
					break;
				}
			}
			if (item && item->range_valid) {
				auto loc = node->location;
				auto mr = std::make_unique<AstNode>(loc, AST_MULTIRANGE);
				mr->children.push_back(std::make_unique<AstNode>(loc, AST_RANGE, node->children[0]->clone()));
				mr->children.push_back(std::make_unique<AstNode>(loc, AST_RANGE,
						AstNode::mkconst_int(loc, item->range_left, true),
						AstNode::mkconst_int(loc, item->range_right, true)));
				node->children.clear();
				node->children.push_back(std::move(mr));
				node->type = AST_IDENTIFIER;
			}
		}
	}
	if (node->type == AST_IDENTIFIER && node->str == name && !node->children.empty() &&
			node->children[0]->type == AST_RANGE) {
		AstNode *idx = node->children[0].get();
		// A stage-1 member-select rewrite may already have produced
		// W[idx][part] as a single MULTIRANGE child; split it into the
		// two-range shape this branch works on.
		if (node->children.size() == 1 && idx->children.size() == 2 &&
				idx->children[0]->type == AST_RANGE && idx->children[1]->type == AST_RANGE) {
			node->children.insert(node->children.begin(), std::move(node->children[0]->children[1]));
			node->children.insert(node->children.begin(), std::move(node->children[0]->children[0]));
			idx = node->children[0].get();
		}
		auto loc = idx->location;
		std::unique_ptr<AstNode> off;
		if (idx->range_valid) {
			off = AstNode::mkconst_int(loc, idx->range_left * w, true);
		} else {
			// Unwrap the index expression from its single-child range node and
			// fold constant indices now: downstream width detection recurses
			// into these bounds, and a bare RANGE inside an expression (or
			// non-const bounds shaped like one) trips it up.
			AstNode *ix = idx->children.size() == 1 ? idx->children[0].get() : nullptr;
			if (ix && ix->type == AST_CONSTANT)
				off = AstNode::mkconst_int(loc, ix->integer * w, true);
			if (!off) {
				if (!ix)
					ix = idx;
				off = std::make_unique<AstNode>(loc, AST_MUL, ix->clone(), AstNode::mkconst_int(loc, w, true));
			}
		}
		node->children.erase(node->children.begin());
		std::unique_ptr<AstNode> left, right;
		if (!node->children.empty() && node->children[0]->type == AST_RANGE && node->children[0]->range_valid) {
			// word select plus packed sub-select [a:b]
			int a = node->children[0]->range_left, b = node->children[0]->range_right;
			node->children.erase(node->children.begin());
			if (a == w - 1 && b == 0) {
				left = off->clone();
				right = std::move(off);
			} else if (off->type == AST_CONSTANT) {
				left = AstNode::mkconst_int(loc, off->integer + a, true);
				right = AstNode::mkconst_int(loc, off->integer + b, true);
			} else {
				left = std::make_unique<AstNode>(loc, AST_ADD, off->clone(), AstNode::mkconst_int(loc, a, true));
				right = std::move(off);
				if (b != 0)
					right = std::make_unique<AstNode>(loc, AST_ADD, std::move(right), AstNode::mkconst_int(loc, b, true));
			}
		} else {
			if (off->type == AST_CONSTANT) {
				left = AstNode::mkconst_int(loc, off->integer + w - 1, true);
				right = std::move(off);
			} else {
				left = std::make_unique<AstNode>(loc, AST_ADD, off->clone(), AstNode::mkconst_int(loc, w - 1, true));
				right = std::move(off);
			}
		}
		node->children.insert(node->children.begin(),
				std::make_unique<AstNode>(loc, AST_RANGE, std::move(left), std::move(right)));
		node->integer = 0;
		node->basic_prep = false;
	}
	for (auto &child : node->children)
		rewrite_unpacked_port_refs(child.get(), name, w, port_node);
	for (auto &attr : node->attributes)
		rewrite_unpacked_port_refs(attr.second.get(), name, w, port_node);
}

// Information about a plain 1-D unpacked memory (packed range + exactly one
// unpacked range, not a typed/struct array). Bounds are resolved on demand;
// returns false for any other shape (multidim, unresolved, struct).
// Whole-array rewrites are limited to small memories; larger ones are left
// to the legacy memory path (direct whole-memory assignments still work
// there, while a concatenation of their elements would exceed the
// expression width implementation limit).
static const int64_t whole_array_rewrite_max_bits = 65536;

// `<seq>.triggered` / `<seq>.matched` (IEEE 1800-2009 16.14.6): the parser
// sees the sequence method as a hierarchical identifier, so elaboration
// reaches the implicit-declaration diagnostic for the underlying wire.
// sva2check later drives such wires with the sequence's end pulse; here we
// only recognize the form so it does not warn/error when the stem names a
// sequence declared in this module.
static bool is_sequence_method_ref(const std::string &str)
{
	if (!current_ast_mod)
		return false;
	for (int m = 0; m < 2; m++) {
		const char *suf = m ? ".matched" : ".triggered";
		size_t sl = strlen(suf);
		if (str.size() <= sl || str.compare(str.size() - sl, sl, suf) != 0)
			continue;
		std::string stem = str.substr(0, str.size() - sl);
		if (!stem.empty() && stem[0] == '\\')
			stem = stem.substr(1);
		for (auto &child : current_ast_mod->children) {
			if (child->type != AST_SVA || child->str != "sequence_decl" ||
					child->children.empty())
				continue;
			if (child->children[0]->str == stem)
				return true;
		}
	}
	return false;
}

// A bare reference to a sequence / property / let declaration
// (`assert property (using_bus_type_correctly)`, the AXI4-Stream FVIP).  The
// frontend leaves it as an identifier; its implicit wire is how sva2check
// finds the declaration by name, so it is not an implicit NET and must not
// trip `default_nettype none`.  Declarations in this module or in a package.
static bool is_sva_decl_ref(const std::string &str)
{
	std::string name = (!str.empty() && str[0] == '\\') ? str.substr(1) : str;
	auto match = [&](AstNode *scope) {
		if (!scope)
			return false;
		for (auto &child : scope->children) {
			if (!child || child->type != AST_SVA || child->children.empty() || !child->children[0])
				continue;
			if (child->str != "sequence_decl" && child->str != "property_decl" && child->str != "let_decl")
				continue;
			const std::string &dn = child->children[0]->str;
			if (dn == name || dn == str)
				return true;
		}
		return false;
	};
	if (match(current_ast_mod))
		return true;
	if (current_ast)
		for (auto &node : current_ast->children)
			if (node && node->type == AST_PACKAGE && match(node.get()))
				return true;
	return false;
}

static bool get_1d_unpacked_mem_info(AstNode *mem, int &elem_width, int &lo, int &hi, bool *struct_array = nullptr){
	log_assert(mem->type == AST_MEMORY);
	if (mem->children.size() != 2)
		return false;
	AstNode *prange = mem->children[0].get();
	AstNode *urange = mem->children[1].get();
	if (!prange || !urange || prange->type != AST_RANGE || urange->type != AST_RANGE)
		return false;
	// Struct/union-typed arrays get element reads/connections rewritten
	// positionally (each element is one packed word); whole-array ASSIGN
	// splits must stay away because '{...} patterns have their own
	// positioning machinery (is_whole_array_assign rejects via this flag).
	if (struct_array)
		*struct_array = mem->attributes.count(ID::wiretype) && resolve_wiretype_struct(mem);
	while (!prange->range_valid && prange->simplify(true, 1, -1, false)) { }
	while (!urange->range_valid && urange->simplify(true, 1, -1, false)) { }
	if (!prange->range_valid || !urange->range_valid)
		return false;
	elem_width = std::abs(prange->range_left - prange->range_right) + 1;
	lo = std::min(urange->range_left, urange->range_right);
	hi = std::max(urange->range_left, urange->range_right);
	if ((int64_t)(hi - lo + 1) * elem_width > whole_array_rewrite_max_bits)
		return false;
	return true;
}

// Element reference mimicking parser output for `mem[idx]` on a 1-D unpacked
// memory: single AST_RANGE index child, integer = number of indexed dims.
static std::unique_ptr<AstNode> make_mem_elem_ref(Location loc, AstNode *mem, int index)
{
	auto elem = std::make_unique<AstNode>(loc, AST_IDENTIFIER,
			std::make_unique<AstNode>(loc, AST_RANGE, AstNode::mkconst_int(loc, index, true)));
	elem->str = mem->str;
	elem->id2ast = mem;
	elem->integer = 1;
	elem->basic_prep = false;
	return elem;
}

// A 2-D unpacked memory `logic [w-1:0] m [A][B]`: element width and the two
// unpacked index ranges.
static bool get_2d_unpacked_mem_info(AstNode *mem, int &elem_width, int &lo0, int &hi0, int &lo1, int &hi1)
{
	// after stage 1 the memory keeps one flattened unpacked range child;
	// its shape is in `dimensions` (unpacked dimensions first)
	if (mem->type != AST_MEMORY || mem->unpacked_dimensions != 2 || GetSize(mem->dimensions) < 3)
		return false;
	auto &d0 = mem->dimensions[0], &d1 = mem->dimensions[1];
	if (d0.range_width < 1 || d1.range_width < 1)
		return false;
	elem_width = 1;
	for (int i = 2; i < GetSize(mem->dimensions); i++)
		elem_width *= mem->dimensions[i].range_width;
	lo0 = d0.range_right;
	hi0 = d0.range_right + d0.range_width - 1;
	lo1 = d1.range_right;
	hi1 = d1.range_right + d1.range_width - 1;
	return (int64_t)d1.range_width * elem_width <= whole_array_rewrite_max_bits;
}

// Rewrite whole-array accesses to plain 1-D unpacked memories into indexed
// element form, before child simplification: reads and cell connection
// arguments become a concatenation {mem[hi], ..., mem[lo]} so the element at
// the lowest index lands in the LSBs (matching the flattened unpacked-port
// convention in rewrite_unpacked_port_refs and packed multidim arrays), and
// assignments to a whole array split into per-element assignments driven
// through a temporary vector wire (keeping the source expression's width
// padding/truncation semantics). Any memory referenced this way is marked
// with the mem2reg attribute so the element wires referenced by the rewrite
// always exist; stock yosys rejected whole-array accesses to memories
// ("Invalid array access"). References inside system function/task calls and
// nested in an assignment lvalue concatenation are left alone.
static bool is_whole_array_assign(AstNode *node, AstNode *&mem, int &elem_width, int &lo, int &hi)
{
	if (!(node->type == AST_ASSIGN || node->type == AST_ASSIGN_EQ || node->type == AST_ASSIGN_LE))
		return false;
	if (node->children.size() < 2 || !node->children[0] ||
			node->children[0]->type != AST_IDENTIFIER || !node->children[0]->children.empty())
		return false;
	AstNode *lhs = node->children[0].get();
	if (!lhs->id2ast || lhs->id2ast->type != AST_MEMORY)
		return false;
	bool struct_array = false;
	if (!get_1d_unpacked_mem_info(lhs->id2ast, elem_width, lo, hi, &struct_array) || struct_array)
		return false;
	mem = lhs->id2ast;
	return true;
}

// Detect a multi-word slice write `mem[sel_hi:sel_lo] = expr` on a plain
// 1-D unpacked memory with constant in-bounds bounds.
static bool is_slice_array_assign(AstNode *node, AstNode *&mem, int &elem_width, int &lo, int &hi)
{
	if (!(node->type == AST_ASSIGN || node->type == AST_ASSIGN_EQ || node->type == AST_ASSIGN_LE))
		return false;
	if (node->children.size() < 2 || !node->children[0] ||
			node->children[0]->type != AST_IDENTIFIER)
		return false;
	AstNode *lhs = node->children[0].get();
	if (!lhs->id2ast || lhs->id2ast->type != AST_MEMORY)
		return false;
	if (lhs->children.size() != 1 || !lhs->children[0] || lhs->children[0]->type != AST_RANGE)
		return false;
	AstNode *range = lhs->children[0].get();
	if (!range->range_valid || range->range_left == range->range_right)
		return false;
	int dim_lo, dim_hi;
	if (!get_1d_unpacked_mem_info(lhs->id2ast, elem_width, dim_lo, dim_hi))
		return false;
	lo = std::min(range->range_left, range->range_right);
	hi = std::max(range->range_left, range->range_right);
	if (lo < dim_lo || hi > dim_hi)
		return false;
	mem = lhs->id2ast;
	return true;
}

// Expand `mem = expr` into a temporary vector wire driven by expr plus one
// assignment per element. The temporary keeps the source expression's width
// padding/truncation semantics. In procedural context (always/initial block,
// function/task body) the temporary is a module-level reg sampled by a
// blocking assignment at the statement position, so non-blocking whole-array
// writes keep their evaluate-now/update-later semantics; a continuous
// assignment would be rejected by proc there.
static std::vector<std::unique_ptr<AstNode>> expand_whole_array_assign(AstNode *node, AstNode *mem, int elem_width, int lo, int hi, bool procedural)
{
	auto loc = node->location;

	std::string tmpname = stringf("$wholeassign$%d", autoidx++);
	auto tmp_wire = std::make_unique<AstNode>(loc, AST_WIRE,
			make_range(loc, (hi - lo + 1) * elem_width - 1, 0));
	tmp_wire->str = tmpname;
	tmp_wire->is_logic = true;
	tmp_wire->is_reg = procedural;
	// in a process the temporary is written right before every read (the
	// split assignments follow it in the same branch): not state, so a
	// branch that leaves it unassigned must not make proc infer a latch
	// or a flip-flop for it (aes_cipher_core: `state_d = add_round_key_out`
	// in one arm of an always_comb case)
	if (procedural)
		tmp_wire->set_attribute(ID::nosync, AstNode::mkconst_int(loc, 1, false));
	AstNode *tmp_wire_p = tmp_wire.get();
	while (tmp_wire_p->simplify(true, 1, -1, false)) { }
	current_scope[tmpname] = tmp_wire_p;
	current_ast_mod->children.push_back(std::move(tmp_wire));

	auto driver = std::make_unique<AstNode>(loc, procedural ? AST_ASSIGN_EQ : AST_ASSIGN);
	driver->children.push_back(std::make_unique<AstNode>(loc, AST_IDENTIFIER));
	driver->children[0]->str = tmpname;
	driver->children[0]->id2ast = tmp_wire_p;
	driver->children.push_back(std::move(node->children[1]));
	driver->was_checked = true;

	std::vector<std::unique_ptr<AstNode>> expanded;
	expanded.push_back(std::move(driver));
	for (int i = hi; i >= lo; i--) {
		auto assign = std::make_unique<AstNode>(loc, node->type);
		assign->children.push_back(make_mem_elem_ref(loc, mem, i));
		auto rhs = std::make_unique<AstNode>(loc, AST_IDENTIFIER,
				make_range(loc, (i - lo + 1) * elem_width - 1, (i - lo) * elem_width));
		rhs->str = tmpname;
		rhs->id2ast = tmp_wire_p;
		assign->children.push_back(std::move(rhs));
		expanded.push_back(std::move(assign));
	}
	return expanded;
}

static void rewrite_whole_array_refs(AstNode *node, bool in_lvalue = false, bool in_procedural = false)
{
	if (!node)
		return;

	bool node_is_assign = node->type == AST_ASSIGN || node->type == AST_ASSIGN_EQ || node->type == AST_ASSIGN_LE;
	bool procedural = in_procedural || node->type == AST_ALWAYS || node->type == AST_INITIAL;
	// calls are left alone ($bits/$size/... would see the concatenation's
	// shape), except the sampled-value functions, which are element-wise:
	// $past(arr) / $stable(arr) of a whole unpacked array (OpenTitan
	// aes_cipher_core, caliptra doe / sha512 properties) are those of the
	// element concatenation
	bool opaque_call = (node->type == AST_FCALL || node->type == AST_TCALL) &&
			!(node->type == AST_FCALL && (node->str == "\\$past" || node->str == "\\$stable" ||
			node->str == "\\$changed" || node->str == "\\$sampled"));

	for (size_t i = 0; i < node->children.size(); i++) {
		AstNode *child = node->children[i].get();

		// split `mem = expr` into per-element assignments, spliced into this
		// node's children list
		AstNode *mem = nullptr;
		int elem_width = 0, lo = 0, hi = -1;
		if (is_whole_array_assign(child, mem, elem_width, lo, hi)) {
			auto expanded = expand_whole_array_assign(child, mem, elem_width, lo, hi, procedural);
			mem->set_attribute(ID::mem2reg, AstNode::mkconst_int(node->location, 1, false));
			// the driver expression may itself contain whole-array reads
			for (auto &it : expanded)
				rewrite_whole_array_refs(it.get(), false);
			node->children.erase(node->children.begin() + i);
			for (auto &it : expanded) {
				node->children.insert(node->children.begin() + i, std::move(it));
				i++;
			}
			i--;
			continue;
		}

		// split a multi-word slice write `mem[hi:lo] = expr` into per-element
		// assignments driven through a temporary, same shape as whole-array
		if (is_slice_array_assign(child, mem, elem_width, lo, hi)) {
			auto expanded = expand_whole_array_assign(child, mem, elem_width, lo, hi, procedural);
			mem->set_attribute(ID::mem2reg, AstNode::mkconst_int(node->location, 1, false));
			for (auto &it : expanded)
				rewrite_whole_array_refs(it.get(), false);
			node->children.erase(node->children.begin() + i);
			for (auto &it : expanded) {
				node->children.insert(node->children.begin() + i, std::move(it));
				i++;
			}
			i--;
			continue;
		}

		// multi-word slice read: replace `mem[hi:lo]` with a concatenation of
		// element references, lowest index in the LSBs (same convention as the
		// whole-array read below); out-of-bounds or oversized slices are left
		// for the generic memory-read path
		if (child->type == AST_IDENTIFIER && !in_lvalue && !(node_is_assign && i == 0) && child->id2ast &&
				child->id2ast->type == AST_MEMORY && !opaque_call &&
				child->children.size() == 1 && child->children[0] && child->children[0]->type == AST_RANGE &&
				child->children[0]->range_valid && child->children[0]->range_left != child->children[0]->range_right &&
				get_1d_unpacked_mem_info(child->id2ast, elem_width, lo, hi)) {
			AstNode *sel_range = child->children[0].get();
			int sel_lo = std::min(sel_range->range_left, sel_range->range_right);
			int sel_hi = std::max(sel_range->range_left, sel_range->range_right);
			if (sel_lo >= lo && sel_hi <= hi) {
				AstNode *smem = child->id2ast;
				auto loc = child->location;
				child->type = AST_CONCAT;
				child->str.clear();
				child->id2ast = nullptr;
				child->is_signed = false;
				child->children.clear();
				for (int j = sel_lo; j <= sel_hi; j++)
					child->children.push_back(make_mem_elem_ref(loc, smem, j));
				smem->set_attribute(ID::mem2reg, AstNode::mkconst_int(loc, 1, false));
				// element references are terminal; no recursion needed
				continue;
			}
		}

		// a row of a 2-D unpacked memory as a cell connection
		// (`.in_i(mux_in[k])`, OpenTitan aes_ghash; IEEE 1800-2017 23.3.3):
		// the concatenation of its elements, lowest index in the LSBs like
		// the flattened unpacked port it connects to.  (Only connections:
		// upstream pins the sub-array ASSIGNMENT as an error,
		// tests/svtypes/multirange_subarray_access.)
		// (a connection to a known module's port may already have been
		// moved into `assign $indirect$<port>... = <arg>`)
		bool is_connection = node->type == AST_ARGUMENT ||
				(node->type == AST_ASSIGN && i == 1 && node->children.size() == 2 && node->children[0] &&
				 node->children[0]->type == AST_IDENTIFIER && node->children[0]->str.rfind("$indirect$", 0) == 0);
		if (is_connection && child->type == AST_IDENTIFIER && child->id2ast &&
				child->id2ast->type == AST_MEMORY && child->attributes.count(ID(sv_subarray_row))) {
			int ew, lo0, hi0, lo1, hi1;
			if (get_2d_unpacked_mem_info(child->id2ast, ew, lo0, hi0, lo1, hi1)) {
				auto idx = child->attributes.at(ID(sv_subarray_row))->clone();
				while (idx->simplify(true, 1, -1, false)) { }
				if (idx->type == AST_CONSTANT) {
					int k = idx->asInt(true);
					if (k >= lo0 && k <= hi0) {
						AstNode *mem2 = child->id2ast;
						auto loc = child->location;
						child->type = AST_CONCAT;
						child->str.clear();
						child->id2ast = nullptr;
						child->is_signed = false;
						child->children.clear();
						child->attributes.erase(ID(sv_subarray_row));
						for (int j = lo1; j <= hi1; j++) {
							auto mr = std::make_unique<AstNode>(loc, AST_MULTIRANGE,
									std::make_unique<AstNode>(loc, AST_RANGE, AstNode::mkconst_int(loc, k, true)),
									std::make_unique<AstNode>(loc, AST_RANGE, AstNode::mkconst_int(loc, j, true)));
							auto elem = std::make_unique<AstNode>(loc, AST_IDENTIFIER, std::move(mr));
							elem->str = mem2->str;
							elem->id2ast = mem2;
							elem->integer = 2;
							elem->basic_prep = false;
							child->children.push_back(std::move(elem));
						}
						mem2->set_attribute(ID::mem2reg, AstNode::mkconst_int(loc, 1, false));
						continue;
					}
				}
			}
		}

		// whole-array read: replace the bare identifier with a concatenation
		// of element references, lowest index in the LSBs (AST_CONCAT
		// children: first child = low bits)
		if (child->type == AST_IDENTIFIER && !in_lvalue && !(node_is_assign && i == 0) && child->children.empty() && child->id2ast &&
				child->id2ast->type == AST_MEMORY && !opaque_call &&
				get_1d_unpacked_mem_info(child->id2ast, elem_width, lo, hi)) {
			AstNode *mem = child->id2ast;
			auto loc = child->location;
			child->type = AST_CONCAT;
			child->str.clear();
			child->id2ast = nullptr;
			child->is_signed = false;
			for (int j = lo; j <= hi; j++)
				child->children.push_back(make_mem_elem_ref(loc, mem, j));
			mem->set_attribute(ID::mem2reg, AstNode::mkconst_int(loc, 1, false));
			// element references are terminal; no recursion needed
			continue;
		}

		// whole-array read of a 1-D unpacked parameter array (a function's
		// unpacked-array argument bound to a package constant: OpenTitan
		// aes_sbox_dom `aes_mvm(data_i, A2X)`; `m = P`): the same element
		// concatenation, whose element reads fold to the pattern's values.
		// The parameter has no value of its own as one constant, so read
		// bare it was an undriven wire
		if (child->type == AST_IDENTIFIER && !in_lvalue && !(node_is_assign && i == 0) && child->children.empty() && child->id2ast &&
				(child->id2ast->type == AST_PARAMETER || child->id2ast->type == AST_LOCALPARAM) && !opaque_call &&
				child->id2ast->unpacked_dimensions == 1 && !child->id2ast->dimensions.empty() &&
				child->id2ast->dimensions[0].range_width >= 1 && child->id2ast->dimensions[0].range_width <= 4096) {
			const auto &dim = child->id2ast->dimensions[0];
			auto loc = child->location;
			auto ref = child->clone();
			child->type = AST_CONCAT;
			child->str.clear();
			child->id2ast = nullptr;
			child->is_signed = false;
			for (int j = 0; j < dim.range_width; j++) {
				auto elem = ref->clone();
				elem->children.push_back(std::make_unique<AstNode>(loc, AST_RANGE,
						AstNode::mkconst_int(loc, dim.range_right + j, true)));
				elem->integer = 1;
				elem->basic_prep = false;
				child->children.push_back(std::move(elem));
			}
			continue;
		}

		rewrite_whole_array_refs(child, node_is_assign && i == 0, procedural);
	}
	for (auto &attr : node->attributes)
		rewrite_whole_array_refs(attr.second.get(), false);
}


static int range_width(AstNode *node, AstNode *rnode)
{
	log_assert(rnode->type==AST_RANGE);
	if (!rnode->range_valid) {
		// A freshly cloned range (e.g. a struct type template handed to a
		// later declaration via the wiretype attribute, or the $sv_unit /
		// verilog_globals duplication of compilation-unit decls) can carry
		// constant children before any simplify pass sets range_valid.
		if (rnode->children.size() == 2 &&
				rnode->children[0]->type == AST_CONSTANT &&
				rnode->children[1]->type == AST_CONSTANT) {
			rnode->range_left = rnode->children[0]->integer;
			rnode->range_right = rnode->children[1]->integer;
			rnode->range_valid = true;
		} else {
			node->input_error("Non-constant range in declaration of %s\n", node->str);
		}
	}
	// note: range swapping has already been checked for
	return rnode->range_left - rnode->range_right + 1;
}

static int add_dimension(AstNode *node, AstNode *rnode)
{
	int width = range_width(node, rnode);
	node->dimensions.push_back({ rnode->range_right, width, rnode->range_swapped });
	return width;
}

[[noreturn]] static void struct_array_packing_error(AstNode *node)
{
	node->input_error("Unpacked array in packed struct/union member %s\n", node->str);
}

// Check if node is an unexpanded array reference (AST_IDENTIFIER -> AST_MEMORY without indexing)
static bool is_unexpanded_array_ref(AstNode *node)
{
	if (node->type != AST_IDENTIFIER)
		return false;
	if (node->id2ast == nullptr || node->id2ast->type != AST_MEMORY)
		return false;
	// No indexing children = whole array reference
	return node->children.empty();
}

// Check if two memories have compatible unpacked dimensions for array assignment
static bool arrays_have_compatible_dims(AstNode *mem_a, AstNode *mem_b)
{
	if (mem_a->unpacked_dimensions != mem_b->unpacked_dimensions)
		return false;
	for (int i = 0; i < mem_a->unpacked_dimensions; i++) {
		if (mem_a->dimensions[i].range_width != mem_b->dimensions[i].range_width)
			return false;
	}
	// Also check packed dimensions (element width)
	int a_width, a_size, a_bits;
	int b_width, b_size, b_bits;
	mem_a->meminfo(a_width, a_size, a_bits);
	mem_b->meminfo(b_width, b_size, b_bits);
	return a_width == b_width;
}

// Check if mem_b matches mem_a's unpacked dimensions starting at first_dim.
static bool arrays_have_compatible_dims_from(AstNode *mem_a, int first_dim, AstNode *mem_b)
{
	if (mem_b->unpacked_dimensions != mem_a->unpacked_dimensions - first_dim)
		return false;
	for (int i = 0; i < mem_b->unpacked_dimensions; i++) {
		if (mem_a->dimensions[first_dim + i].range_width != mem_b->dimensions[i].range_width)
			return false;
	}
	// Also check packed dimensions (element width)
	int a_width, a_size, a_bits;
	int b_width, b_size, b_bits;
	mem_a->meminfo(a_width, a_size, a_bits);
	mem_b->meminfo(b_width, b_size, b_bits);
	return a_width == b_width;
}

// Convert per-dimension element positions to declared index values.
// Position 0 is the first declared element for each unpacked dimension.

static std::vector<int> array_indices_from_position(AstNode *mem, const std::vector<int> &position)
{
	int num_dims = mem->unpacked_dimensions;
	log_assert(GetSize(position) == num_dims);

	std::vector<int> indices(num_dims);
	for (int d = 0; d < num_dims; d++) {
		int low = mem->dimensions[d].range_right;
		int high = low + mem->dimensions[d].range_width - 1;
		indices[d] = mem->dimensions[d].range_swapped ? (low + position[d]) : (high - position[d]);
	}
	return indices;
}

// Generate all element positions for a multi-dimensional unpacked array and
// call callback once for each combination.
static void foreach_array_position(AstNode *mem, std::function<void(const std::vector<int>&)> callback)
{
	int num_dims = mem->unpacked_dimensions;
	if (num_dims == 0) {
		callback({});
		return;
	}

	std::vector<int> position(num_dims, 0);
	std::vector<int> sizes(num_dims);

	for (int d = 0; d < num_dims; d++)
		sizes[d] = mem->dimensions[d].range_width;

	// Iterate through all position combinations (rightmost dimension fastest).
	while (true) {
		callback(position);

		int d = num_dims - 1;
		while (d >= 0) {
			position[d]++;
			if (position[d] < sizes[d])
				break;
			position[d] = 0;
			d--;
		}
		if (d < 0)
			break;
	}
}

static int size_packed_struct(AstNode *snode, int base_offset)
{
	// Struct members will be laid out in the structure contiguously from left to right.
	// Union members all have zero offset from the start of the union.
	// Determine total packed size and assign offsets.  Store these in the member node.
	bool is_union = (snode->type == AST_UNION);
	int offset = 0;
	int packed_width = -1;
	// examine members from last to first
	for (auto it = snode->children.rbegin(); it != snode->children.rend(); ++it) {
		auto node = it->get();
		if (node->attributes.count(ID::sv_void_member)) {
			// tagged-union void member: tag-only, contributes no bits and
			// is exempt from the packed-union same-size check
			node->range_right = base_offset + offset;
			node->range_left = base_offset + offset - 1;
			node->range_valid = true;
			node->dimensions.push_back({ 0, 0, false });
			continue;
		}
		int width;
		if (node->type == AST_STRUCT || node->type == AST_UNION) {
			// embedded struct or union
			width = size_packed_struct(node, base_offset + offset);
		}
		else if (node->type == AST_MEMORY || node->type == AST_WIRE) {
			// member declared through a user-defined type: resolving the
			// member's WIRETYPE rewrites the STRUCT_ITEM into the typedef's
			// wire or memory node before the struct is sized. Pack the
			// dimensions the same way as the unpacked-array extension below.
			log_assert(!node->children.empty() && node->children[0]->type == AST_RANGE);
			node->dimensions.clear();
			width = add_dimension(node, node->children[0].get());
			if (node->type == AST_MEMORY) {
				if (node->children.size() != 2 || node->children[1]->type != AST_RANGE)
					struct_array_packing_error(node);
				auto rnode = node->children[1].get();
				if (rnode->children.size() == 1) {
					// C-style array size, e.g. typedef bit [63:0] t [4]; t a;
					node->dimensions.push_back({ 0, rnode->range_left, true });
					width *= rnode->range_left;
				} else {
					width *= add_dimension(node, rnode);
				}
			}
			// range nodes are now redundant
			node->children.clear();
			// keep the member a struct item for downstream consumers
			node->type = AST_STRUCT_ITEM;
		}
		else {
			log_assert(node->type == AST_STRUCT_ITEM);
			if (node->children.size() > 0 && node->children[0]->type == AST_RANGE) {
				// member width e.g. bit [7:0] a
				width = range_width(node, node->children[0].get());
				if (node->children.size() == 2) {
					// Unpacked array. Note that this is a Yosys extension; only packed data types
					// and integer data types are allowed in packed structs / unions in SystemVerilog.
					if (node->children[1]->type == AST_RANGE) {
						// Unpacked array, e.g. bit [63:0] a [0:3]
						// Pretend it's declared as a packed array, e.g. bit [0:3][63:0] a
						auto rnode = node->children[1].get();
						if (rnode->children.size() == 1) {
							// C-style array size, e.g. bit [63:0] a [4]
							node->dimensions.push_back({ 0, rnode->range_left, true });
							width *= rnode->range_left;
						} else {
							width *= add_dimension(node, rnode);
						}
						add_dimension(node, node->children[0].get());
					}
					else {
						// The Yosys extension for unpacked arrays in packed structs / unions
						// only supports memories, i.e. e.g. logic [7:0] a [256] - see above.
						struct_array_packing_error(node);
					}
				} else {
					// Vector
					add_dimension(node, node->children[0].get());
				}
				// range nodes are now redundant
				node->children.clear();
			}
			else if (node->children.size() > 0 && node->children[0]->type == AST_MULTIRANGE) {
				// Packed array, e.g. bit [3:0][63:0] a
				if (node->children.size() != 1) {
					// The Yosys extension for unpacked arrays in packed structs / unions
					// only supports memories, i.e. e.g. logic [7:0] a [256] - see above.
					struct_array_packing_error(node);
				}
				width = 1;
				for (auto& rnode : node->children[0]->children) {
					width *= add_dimension(node, rnode.get());
				}
				// range nodes are now redundant
				node->children.clear();
			}
			else if (node->range_left < 0) {
				// 1 bit signal: bit, logic or reg
				width = 1;
				node->dimensions.push_back({ 0, width, false });
			}
			else {
				// already resolved and compacted
				width = node->range_left - node->range_right + 1;
			}
			if (is_union) {
				node->range_right = base_offset;
				node->range_left = base_offset + width - 1;
			}
			else {
				node->range_right = base_offset + offset;
				node->range_left = base_offset + offset + width - 1;
			}
			node->range_valid = true;
		}
		if (is_union) {
			// check that all members have the same size. Tagged and
			// unpacked unions (IEEE 1800-2017 7.3.2) may have
			// differently-sized members; the union is sized to the
			// widest member plus the tag bits.
			if (packed_width == -1) {
				// first member
				packed_width = width;
			}
			else if (snode->attributes.count(ID::sv_tagged) || snode->attributes.count(ID(sv_unpacked))) {
				packed_width = std::max(packed_width, width);
			}
			else {
				if (packed_width != width)
					node->input_error("member %s of a packed union has %d bits, expecting %d\n", node->str, width, packed_width);
			}
		}
		else {
			offset += width;
		}
	}

	int width = is_union ? packed_width : offset;

	// tagged unions (IEEE 1800-2017 7.3.2) carry an implicit tag encoding
	// the active member, stored ahead of the member bits
	if (snode->attributes.count(ID::sv_tagged)) {
		int nmembers = 0;
		for (auto &child : snode->children)
			if (child && child->type == AST_STRUCT_ITEM)
				nmembers++;
		int tag_bits = 1;
		while ((1 << tag_bits) < nmembers)
			tag_bits++;
		width += tag_bits;
	}

	snode->range_right = base_offset;
	snode->range_left = base_offset + width - 1;
	snode->range_valid = true;
	if (snode->attributes.count(ID(sv_elem_range))) {
		// a struct member that is a packed array of a struct type
		// (`mreg_t [3:0] en;`): the N element copies are laid out
		// above; record the outer dimension so `s.en[i]` is one
		// element and `s.en[i].q` (AST_PREFIX rewrite) is its member
		AstNode *r = snode->attributes.at(ID(sv_elem_range)).get();
		int n = std::abs(r->range_left - r->range_right) + 1;
		snode->dimensions.clear();
		snode->dimensions.push_back({ std::min(r->range_left, r->range_right), n, r->range_left < r->range_right });
		snode->dimensions.push_back({ 0, width / n, false });
	} else
		snode->dimensions.push_back({ 0, width, false });

	return width;
}

// A struct type whose typedef'd struct members (`sub_t sel;`) still carry
// their WIRETYPE: sized as is, such a member is one bit and every member
// offset after it is wrong.
static bool struct_has_unresolved_members(AstNode *st)
{
	for (auto &it : st->children) {
		if (!it)
			continue;
		if (it->type == AST_STRUCT || it->type == AST_UNION) {
			if (struct_has_unresolved_members(it.get()))
				return true;
		} else if (it->type == AST_STRUCT_ITEM) {
			for (auto &ch : it->children)
				if (ch && ch->type == AST_WIRETYPE)
					return true;
		}
	}
	return false;
}

// A sized copy of the struct with typedef'd struct members replaced by
// their struct types; nullptr if a member cannot be resolved that way
// (a typedef'd vector, or packed dimensions on a struct member).
static std::unique_ptr<AstNode> resolve_struct_layout(AstNode *stype)
{
	std::function<bool(AstNode *)> fix = [&](AstNode *st) -> bool {
		for (auto &it : st->children) {
			if (!it)
				continue;
			if (it->type == AST_STRUCT || it->type == AST_UNION) {
				if (!fix(it.get()))
					return false;
				continue;
			}
			if (it->type != AST_STRUCT_ITEM)
				continue;
			AstNode *wt = nullptr;
			bool ranged = false;
			for (auto &ch : it->children) {
				if (ch && ch->type == AST_WIRETYPE)
					wt = ch.get();
				if (ch && (ch->type == AST_RANGE || ch->type == AST_MULTIRANGE))
					ranged = true;
			}
			if (!wt)
				continue;
			if (ranged || !current_scope.count(wt->str) || current_scope.at(wt->str)->type != AST_TYPEDEF ||
					current_scope.at(wt->str)->children.empty())
				return false;
			AstNode *t = current_scope.at(wt->str)->children[0].get();
			if (t->type != AST_STRUCT && t->type != AST_UNION)
				return false;
			auto rep = t->clone();
			rep->str = it->str;
			if (!fix(rep.get()))
				return false;
			it = std::move(rep);
		}
		return true;
	};
	auto copy = stype->clone();
	if (!fix(copy.get()))
		return nullptr;
	size_packed_struct(copy.get(), 0);
	return copy;
}

static std::unique_ptr<AstNode> node_int(AstSrcLocType loc, int ival)
{
	return AstNode::mkconst_int(loc, ival, true);
}

static std::unique_ptr<AstNode> multiply_by_const(std::unique_ptr<AstNode> expr_node, int stride)
{
	auto loc = expr_node->location;
	return std::make_unique<AstNode>(loc, AST_MUL, std::move(expr_node), node_int(loc, stride));
}

static std::unique_ptr<AstNode> normalize_index(AstNode *expr, AstNode *decl_node, int dimension)
{
	auto new_expr = expr->clone();
	auto loc = expr->location;

	int offset = decl_node->dimensions[dimension].range_right;
	if (offset) {
		new_expr = std::make_unique<AstNode>(loc, AST_SUB, std::move(new_expr), node_int(loc, offset));
	}

	// Packed dimensions are normally indexed by lsb, while unpacked dimensions are normally indexed by msb.
	if ((dimension < decl_node->unpacked_dimensions) ^ decl_node->dimensions[dimension].range_swapped) {
		// Swap the index if the dimension is declared the "wrong" way.
		int left = decl_node->dimensions[dimension].range_width - 1;
		new_expr = std::make_unique<AstNode>(loc, AST_SUB, node_int(loc, left), std::move(new_expr));
	}

	return new_expr;
}

static std::unique_ptr<AstNode> index_offset(std::unique_ptr<AstNode> offset, AstNode *rnode, AstNode *decl_node, int dimension, int &stride)
{
	stride /= decl_node->dimensions[dimension].range_width;
	auto right = normalize_index(rnode->children.back().get(), decl_node, dimension);
	auto add_offset = stride > 1 ? multiply_by_const(std::move(right), stride) : std::move(right);
	return offset ? std::make_unique<AstNode>(rnode->location, AST_ADD, std::move(offset), std::move(add_offset)) : std::move(add_offset);
}

static std::unique_ptr<AstNode> index_msb_offset(std::unique_ptr<AstNode> lsb_offset, AstNode *rnode, AstNode *decl_node, int dimension, int stride)
{
	log_assert(rnode->children.size() <= 2);
	auto loc = rnode->location;

	// Offset to add to LSB
	std::unique_ptr<AstNode> add_offset;
	if (rnode->children.size() == 1) {
		// Index, e.g. s.a[i]
		add_offset = node_int(rnode->location, stride - 1);
	}
	else {
		// rnode->children.size() == 2
		// Slice, e.g. s.a[i:j]
		auto left = normalize_index(rnode->children[0].get(), decl_node, dimension);
		auto right = normalize_index(rnode->children[1].get(), decl_node, dimension);
		add_offset = std::make_unique<AstNode>(loc, AST_SUB, std::move(left), std::move(right));
		if (stride > 1) {
			// offset = (msb - lsb + 1)*stride - 1
			auto slice_width = std::make_unique<AstNode>(loc, AST_ADD, std::move(add_offset), node_int(loc, 1));
			add_offset = std::make_unique<AstNode>(loc, AST_SUB, multiply_by_const(std::move(slice_width), stride), node_int(loc, 1));
		}
	}

	return std::make_unique<AstNode>(loc, AST_ADD, std::move(lsb_offset), std::move(add_offset));
}


std::unique_ptr<AstNode> AstNode::make_index_range(AstNode *decl_node, bool unpacked_range)
{
	// Work out the range in the packed array that corresponds to a struct member
	// taking into account any range operations applicable to the current node
	// such as array indexing or slicing
	if (children.empty()) {
		// no range operations apply, return the whole width
		return make_range(decl_node->location, decl_node->range_left - decl_node->range_right, 0);
	}

	log_assert(children.size() == 1);

	// Range operations
	AstNode *rnode = children[0].get();
	std::unique_ptr<AstNode> offset = nullptr;
	int dim = unpacked_range ? 0 : decl_node->unpacked_dimensions;
	int max_dim = unpacked_range ? decl_node->unpacked_dimensions : GetSize(decl_node->dimensions);

	int stride = 1;
	for (int i = dim; i < max_dim; i++) {
		stride *= decl_node->dimensions[i].range_width;
	}

	// Calculate LSB offset for the final index / slice
	if (rnode->type == AST_RANGE) {
		offset = index_offset(std::move(offset), rnode, decl_node, dim, stride);
	}
	else if (rnode->type == AST_MULTIRANGE) {
		// Add offset for each dimension
		AstNode *mrnode = rnode;
		int stop_dim = std::min(GetSize(mrnode->children), max_dim);
		for (; dim < stop_dim; dim++) {
			rnode = mrnode->children[dim].get();
			offset = index_offset(std::move(offset), rnode, decl_node, dim, stride);
		}
		dim--;  // Step back to the final index / slice
	}
	else {
		input_error("Unsupported range operation for %s\n", str);
	}

	std::unique_ptr<AstNode> index_range = std::make_unique<AstNode>(rnode->location, AST_RANGE);

	if (!unpacked_range && (stride > 1 || GetSize(rnode->children) == 2)) {
		// Calculate MSB offset for the final index / slice of packed dimensions.
		std::unique_ptr<AstNode>msb_offset = index_msb_offset(offset->clone(), rnode, decl_node, dim, stride);
		index_range->children.push_back(std::move(msb_offset));
	}

	index_range->children.push_back(std::move(offset));

	return index_range;
}

AstNode *AstNode::get_struct_member() const
{
	AstNode *member_node;
	if (attributes.count(ID::wiretype) && (member_node = attributes.at(ID::wiretype).get()) &&
		(member_node->type == AST_STRUCT_ITEM || member_node->type == AST_STRUCT || member_node->type == AST_UNION))
	{
		return member_node;
	}
	return nullptr;
}

static void add_members_to_scope(AstNode *snode, std::string name)
{
	// add all the members in a struct or union to local scope
	// in case later referenced in assignments
	log_assert(snode->type==AST_STRUCT || snode->type==AST_UNION);
	for (auto &node : snode->children) {
		auto member_name = name + "." + node->str;
		current_scope[member_name] = node.get();
		if (node->type != AST_STRUCT_ITEM) {
			// embedded struct or union
			add_members_to_scope(node.get(), name + "." + node->str);
		}
	}
}

// Resolve the struct/union template behind a wire or memory declared
// through a (possibly chained) user-defined type. The wiretype attribute
// holds either the struct node itself (packed-struct wires, memories of
// packed structs) or the typedef name as a string; the typedef target may
// in turn be an array-of-struct memory carrying the struct in its own
// wiretype attribute.
static AstNode *resolve_wiretype_struct(AstNode *decl_node)
{
	if (!decl_node->attributes.count(ID::wiretype))
		return nullptr;
	AstNode *wt = decl_node->attributes.at(ID::wiretype).get();
	if (wt->type == AST_STRUCT || wt->type == AST_UNION)
		return wt;
	if (wt->type != AST_CONSTANT)
		return nullptr;
	std::string tname = wt->str.c_str();
	AstNode *td = current_scope.count(tname) ? current_scope.at(tname) : nullptr;
	if (!td && current_ast)
		for (auto &node : current_ast->children) {
			if (!node)
				continue;
			if (node->type == AST_TYPEDEF && node->str == tname)
				td = node.get();
			if (node->type == AST_PACKAGE)
				for (auto &inner : node->children)
					if (inner && inner->type == AST_TYPEDEF && inner->str == tname)
						td = inner.get();
		}
	AstNode *t = (td && td->type == AST_TYPEDEF && !td->children.empty()) ? td->children[0].get() : nullptr;
	// a typedef'd packed array of a struct: element accesses see the
	// element struct (its member copies repeat every member name)
	if (t && (t->type == AST_STRUCT || t->type == AST_UNION) && t->attributes.count(ID(sv_elem_type)))
		return t->attributes.at(ID(sv_elem_type)).get();
	if (t && (t->type == AST_STRUCT || t->type == AST_UNION))
		return t;
	if (t && (t->type == AST_WIRE || t->type == AST_MEMORY))
		return resolve_wiretype_struct(t);
	return nullptr;
}

static void prepend_ranges(std::unique_ptr<AstNode> &range, AstNode *range_add);

// packed_dims: the declaration's own packed dimensions for a packed array
// of the struct (`st_t [3:0] x`): the wire becomes the multirange
// [3:0][E-1:0], so `x[i]` is one element and `x[i].m` (rewritten below in
// the AST_PREFIX branch) is that element's member bits; without it the
// outer dimension was silently dropped and the wire was one element wide.
std::unique_ptr<AstNode> make_packed_struct(AstNode *template_node, std::string &name, decltype(AstNode::attributes) &attributes, AstNode *packed_dims = nullptr)
{
	// create a wire for the packed struct
	auto loc = template_node->location;
	auto wnode = std::make_unique<AstNode>(loc, AST_WIRE, make_range(loc, template_node->range_left, 0));
	wnode->str = name;
	wnode->is_logic = true;
	wnode->range_valid = true;
	wnode->is_signed = template_node->is_signed;
	for (auto &pair : attributes) {
		wnode->set_attribute(pair.first, pair.second->clone());
	}
	if (packed_dims) {
		prepend_ranges(wnode->children[0], packed_dims);
		wnode->range_valid = false;
	}
	// resolve packed dimension
	while (wnode->simplify(true, 1, -1, false)) {}
	// make sure this node is the one in scope for this name
	current_scope[name] = wnode.get();
	// add all the struct members to scope under the wire's name
	add_members_to_scope(template_node, name);
	return wnode;
}

// Replace each `initial` (a local's declaration initializer) in an inlined
// function or task body by its assignments, made blocking, in place.
static void inline_function_initializers(std::vector<std::unique_ptr<AstNode>> &stmts)
{
	std::vector<std::unique_ptr<AstNode>> out;
	for (auto &stmt : stmts) {
		if (stmt && stmt->type == AST_INITIAL) {
			std::vector<std::unique_ptr<AstNode>> body;
			for (auto &c : stmt->children) {
				if (c && c->type == AST_BLOCK && c->str.empty())
					for (auto &g : c->children)
						body.push_back(std::move(g));
				else if (c)
					body.push_back(std::move(c));
			}
			for (auto &b : body) {
				if (b->type == AST_ASSIGN_LE)
					b->type = AST_ASSIGN_EQ;
			}
			inline_function_initializers(body);
			for (auto &b : body)
				out.push_back(std::move(b));
			continue;
		}
		if (stmt && stmt->type != AST_FUNCTION && stmt->type != AST_TASK)
			for (auto &c : stmt->children)
				if (c && (c->type == AST_BLOCK || c->type == AST_GENBLOCK)) {
					inline_function_initializers(c->children);
				} else if (c && (c->type == AST_COND || c->type == AST_CONDX || c->type == AST_CONDZ ||
						c->type == AST_FOR || c->type == AST_WHILE || c->type == AST_REPEAT || c->type == AST_CASE)) {
					std::vector<std::unique_ptr<AstNode>> one;
					one.push_back(std::move(c));
					inline_function_initializers(one);
					c = std::move(one[0]);
				}
		out.push_back(std::move(stmt));
	}
	stmts.swap(out);
}

static void prepend_ranges(std::unique_ptr<AstNode> &range, AstNode *range_add)
{
	// Convert range to multirange.
	auto loc = range->location;
	if (range->type == AST_RANGE)
		range = std::make_unique<AstNode>(loc, AST_MULTIRANGE, std::move(range));

	// Add range or ranges.
	if (range_add->type == AST_RANGE)
		range->children.insert(range->children.begin(), range_add->clone());
	else {
		int i = 0;
		for (auto& child : range_add->children)
			range->children.insert(range->children.begin() + i++, child->clone());
	}
}

// check if a node or its children contains an assignment to the given variable
static bool node_contains_assignment_to(const AstNode* node, const AstNode* var)
{
	if (node->type == AST_ASSIGN_EQ || node->type == AST_ASSIGN_LE) {
		// current node is iteslf an assignment
		log_assert(node->children.size() >= 2);
		const AstNode* lhs = node->children[0].get();
		if (lhs->type == AST_IDENTIFIER && lhs->str == var->str)
			return false;
	}
	for (auto& child : node->children) {
		// if this child shadows the given variable
		if (child.get() != var && child->str == var->str && child->type == AST_WIRE)
			break; // skip the remainder of this block/scope
		// depth-first short circuit
		if (!node_contains_assignment_to(child.get(), var))
			return false;
	}
	return true;
}

static std::string prefix_id(const std::string &prefix, const std::string &str)
{
	// An empty (or unescaped) identifier can never resolve; return it
	// unchanged instead of asserting. Error-recovery and class/method
	// parsing can leave such nodes in the tree (e.g. dropped member
	// attributes), and they are discarded before RTLIL generation.
	if (str.empty() || (str.front() != '$' && str.front() != '\\'))
		return str;
	log_assert(!prefix.empty() && (prefix.front() == '$' || prefix.front() == '\\'));
	log_assert(!str.empty() && (str.front() == '$' || str.front() == '\\'));
	log_assert(prefix.back() == '.');
	if (str.front() == '\\')
		return prefix + str.substr(1);
	return prefix + str;
}

// Identifier references that miss current_scope inside an inlined function
// body (the expansion prefix carries "$func$") are resolved by the
// function-inlining machinery (names carry the "$func$" prefix), not by
// module-scope scanning — which would walk every module child per reference
// and emit dead autowires.
static bool prefix_is_function_inline(const std::string &prefix)
{
	return prefix.find("$func$") != std::string::npos;
}

// direct access to this global should be limited to the following two functions
static const RTLIL::Design *simplify_design_context = nullptr;

void AST::set_simplify_design_context(const RTLIL::Design *design)
{
	log_assert(!simplify_design_context || !design);
	simplify_design_context = design;
}

// lookup the module with the given name in the current design context
static const RTLIL::Module* lookup_module(const std::string &name)
{
	return simplify_design_context->module(name);
}

// The struct/union type of parameter `paraname` (or the pos-th parameter
// when paraname is empty) of module `modname`, cloned; nullptr when it has
// none or cannot be resolved.  An assignment pattern used as an instance
// parameter override (`leaf #('{A:5, B:9})`, the AXI4 FVIP testbench's
// `amba_axi4_protocol_checker #('{ID_WIDTH:4, ...})`) has no type in the
// instantiating module: its type is the overridden parameter's.
static std::unique_ptr<AstNode> override_struct_type(const std::string &modname, const std::string &paraname, int pos)
{
	// the elaborated module's stored ORIGINAL AST first (the copy in
	// current_ast may already be simplified: its parameter type folded away)
	AstNode *mast = nullptr;
	{
		const RTLIL::Module *m = lookup_module(modname);
		if (!m)
			m = lookup_module("$abstract" + modname);
		auto *am = dynamic_cast<const AST::AstModule *>(m);
		if (am && am->ast)
			mast = am->ast.get();
	}
	if (!mast && current_ast)
		for (auto &c : current_ast->children)
			if (c && (c->type == AST_MODULE || c->type == AST_INTERFACE) && c->str == modname)
				mast = c.get();
	if (!mast)
		return nullptr;
	AstNode *param = nullptr;
	int k = 0;
	for (auto &c : mast->children)
		if (c && c->type == AST_PARAMETER) {
			if (paraname.empty() ? k == pos : c->str == paraname) {
				param = c.get();
				break;
			}
			k++;
		}
	if (!param)
		return nullptr;
	auto struct_of = [](AstNode *n) -> std::unique_ptr<AstNode> {
		if (n && n->type == AST_TYPEDEF && !n->children.empty())
			n = n->children[0].get();
		if (n && (n->type == AST_STRUCT || n->type == AST_UNION))
			return n->clone();
		return nullptr;
	};
	if (param->attributes.count(ID::wiretype))
		if (auto t = struct_of(param->attributes.at(ID::wiretype).get()))
			return t;
	std::string tname;
	for (auto &c : param->children) {
		if (c && (c->type == AST_STRUCT || c->type == AST_UNION))
			return c->clone();
		if (c && c->type == AST_WIRETYPE)
			tname = c->str;
	}
	if (tname.empty())
		return nullptr;
	std::string bare = tname[0] == '\\' ? tname.substr(1) : tname;
	// the module's own (and its appended package / compilation-unit) typedefs
	for (auto &c : mast->children)
		if (c && c->type == AST_TYPEDEF && c->str == tname)
			return struct_of(c.get());
	// imported: `import pkg::*` -> pkg::name, appended or in this file's packages
	for (auto &imp : mast->children) {
		if (!imp || imp->type != AST_IMPORT)
			continue;
		std::string q = imp->str + "::" + bare;
		for (auto &c : mast->children)
			if (c && c->type == AST_TYPEDEF && c->str == q)
				return struct_of(c.get());
		if (current_ast)
			for (auto &pk : current_ast->children)
				if (pk && pk->type == AST_PACKAGE && pk->str == imp->str)
					for (auto &c : pk->children)
						if (c && c->type == AST_TYPEDEF && c->str == tname)
							return struct_of(c.get());
		if (simplify_design_context)
			for (auto &pk : simplify_design_context->verilog_packages)
				if (pk && pk->str == imp->str)
					for (auto &c : pk->children)
						if (c && c->type == AST_TYPEDEF && c->str == tname)
							return struct_of(c.get());
	}
	return nullptr;
}

// True when a parameter value is plain arithmetic over constants and
// (possibly qualified) identifiers - safe to force-fold early when a
// referencing module runs before the defining package's own fold pass.
static bool expr_is_plain_arith(AstNode *n, int depth = 0)
{
	if (!n || depth > 32)
		return false;
	switch (n->type) {
	case AST_CONSTANT:
		return true;
	case AST_IDENTIFIER: {
		// An identifier that would resolve to an implicit (autowire) net
		// must not be force-folded: the missing-package degradation paths
		// handle those, and folding them early produces stale state.
		AstNode *pt = n->id2ast;
		if ((!pt || (pt->type != AST_PARAMETER && pt->type != AST_LOCALPARAM &&
				pt->type != AST_ENUM_ITEM && pt->type != AST_CONSTANT)) &&
				current_scope.count(n->str))
			pt = current_scope[n->str];
		if ((!pt || (pt->type != AST_PARAMETER && pt->type != AST_LOCALPARAM &&
				pt->type != AST_ENUM_ITEM && pt->type != AST_CONSTANT)) &&
				simplify_design_context) {
			// resolves later via a design package member (possibly
			// package-qualified basename match)
			for (auto &pn : simplify_design_context->verilog_packages) {
				if (!pn)
					continue;
				for (auto &mem : pn->children) {
					if (!mem)
						continue;
					const std::string &ms = mem->str;
					size_t pos = ms.rfind("::");
					std::string base = pos == std::string::npos ? ms : ms.substr(pos + 2);
					if (base == n->str || ms == n->str) {
						pt = mem.get();
						break;
					}
				}
				if (pt)
					break;
			}
		}
		if (!pt)
			return false;
		if (pt->type != AST_PARAMETER && pt->type != AST_LOCALPARAM &&
				pt->type != AST_ENUM_ITEM && pt->type != AST_CONSTANT)
			return false;
		return true;
	}
	case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
	case AST_POW: case AST_SHIFT_LEFT: case AST_SHIFT_RIGHT: case AST_SHIFT_SLEFT: case AST_SHIFT_SRIGHT:
	case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR: case AST_BIT_XNOR:
	case AST_BIT_NOT:
		break;
	default:
		return false;
	}
	for (auto &ch : n->children)
		if (!expr_is_plain_arith(ch.get(), depth + 1))
			return false;
	return true;
}

const RTLIL::Module* AstNode::lookup_cell_module()
{
	log_assert(type == AST_CELL);

	auto reprocess_after = [this] (const std::string &modname) {
		if (!attributes.count(ID::reprocess_after))
			set_attribute(ID::reprocess_after, AstNode::mkconst_str(location, modname));
	};

	const AstNode *celltype = nullptr;
	for (auto& child : children)
		if (child->type == AST_CELLTYPE) {
			celltype = child.get();
			break;
		}
	log_assert(celltype != nullptr);

	const RTLIL::Module *module = lookup_module(celltype->str);
	if (!module)
		module = lookup_module("$abstract" + celltype->str);
	if (!module) {
		if (celltype->str.at(0) != '$')
			reprocess_after(celltype->str);
		return nullptr;
	}

	// build a mapping from true param name to param value
	size_t para_counter = 0;
	dict<RTLIL::IdString, RTLIL::Const> cell_params_map;
	for (auto& child : children) {
		if (child->type != AST_PARASET)
			continue;

		if (child->str.empty() && para_counter >= module->avail_parameters.size())
			return nullptr; // let hierarchy handle this error
		IdString paraname = child->str.empty() ? module->avail_parameters[para_counter++] : child->str;

		const AstNode *value = child->children[0].get();
		if (value->type != AST_REALVALUE && value->type != AST_CONSTANT)
			return nullptr; // let genrtlil handle this error
		cell_params_map[paraname] = value->asParaConst();
	}

	// put the parameters in order and generate the derived module name
	std::vector<std::pair<RTLIL::IdString, RTLIL::Const>> named_parameters;
	for (RTLIL::IdString param : module->avail_parameters) {
		auto it = cell_params_map.find(param);
		if (it != cell_params_map.end())
			named_parameters.emplace_back(it->first, it->second);
	}
	std::string modname = celltype->str;
	if (cell_params_map.size()) // not named_parameters to cover hierarchical defparams
		modname = derived_module_name(celltype->str, named_parameters);

	// try to find the resolved module
	module = lookup_module(modname);
	if (!module) {
		reprocess_after(modname);
		return nullptr;
	}
	return module;
}

// returns whether an expression contains an unbased unsized literal; does not
// check the literal exists in a self-determined context
static bool contains_unbased_unsized(const AstNode *node)
{
	if (node->type == AST_CONSTANT)
		return node->is_unsized;
	for (auto& child : node->children)
		if (contains_unbased_unsized(child.get()))
			return true;
	return false;
}

// adds a wire to the current module with the given name that matches the
// dimensions of the given wire reference
void add_wire_for_ref(Location loc, const RTLIL::Wire *ref, const std::string &str)
{
	auto left = AstNode::mkconst_int(loc, ref->width - 1 + ref->start_offset, true);
	auto right = AstNode::mkconst_int(loc, ref->start_offset, true);
	if (ref->upto)
		std::swap(left, right);
	auto range = std::make_unique<AstNode>(loc, AST_RANGE, std::move(left), std::move(right));

	auto wire = std::make_unique<AstNode>(loc, AST_WIRE, std::move(range));
	wire->is_signed = ref->is_signed;
	wire->is_logic = true;
	wire->str = str;

	current_scope[str] = wire.get();
	current_ast_mod->children.push_back(std::move(wire));
}

enum class IdentUsage {
	NotReferenced, // target variable is neither read or written in the block
	Assigned, // target variable is always assigned before use
	SyncRequired, // target variable may be used before it has been assigned
};

// determines whether a local variable a block is always assigned before it is
// used, meaning the nosync attribute can automatically be added to that
// variable
static IdentUsage always_asgn_before_use(const AstNode *node, const std::string &target)
{
	// This variable has been referenced before it has necessarily been assigned
	// a value in this procedure.
	if (node->type == AST_IDENTIFIER && node->str == target)
		return IdentUsage::SyncRequired;

	// For case statements (which are also used for if/else), we check each
	// possible branch. If the variable is assigned in all branches, then it is
	// assigned, and a sync isn't required. If it used before assignment in any
	// branch, then a sync is required.
	if (node->type == AST_CASE) {
		bool all_defined = true;
		bool any_used = false;
		bool has_default = false;
		for (auto& child : node->children) {
			if (child->type == AST_COND && child->children.at(0)->type == AST_DEFAULT)
				has_default = true;
			IdentUsage nested = always_asgn_before_use(child.get(), target);
			if (nested != IdentUsage::Assigned && child->type == AST_COND)
				all_defined = false;
			if (nested == IdentUsage::SyncRequired)
				any_used = true;
		}
		if (any_used)
			return IdentUsage::SyncRequired;
		else if (all_defined && has_default)
			return IdentUsage::Assigned;
		else
			return IdentUsage::NotReferenced;
	}

	// Check if this is an assignment to the target variable. For simplicity, we
	// don't analyze sub-ranges of the variable.
	if (node->type == AST_ASSIGN_EQ) {
		auto& ident = node->children.at(0);
		if (ident->type == AST_IDENTIFIER && ident->str == target)
			return IdentUsage::Assigned;
	}

	for (auto& child : node->children) {
		IdentUsage nested = always_asgn_before_use(child.get(), target);
		if (nested != IdentUsage::NotReferenced)
			return nested;
	}
	return IdentUsage::NotReferenced;
}

std::unique_ptr<AstNode> AstNode::clone_at_zero()
{
	int width_hint;
	bool sign_hint;
	AstNode *pointee;

	switch (type) {
	case AST_IDENTIFIER:
		if (id2ast)
			pointee = id2ast;
		else if (current_scope.count(str))
			pointee = current_scope[str];
		else
			break;

		if (pointee->type != AST_WIRE &&
				pointee->type != AST_AUTOWIRE &&
				pointee->type != AST_MEMORY)
			break;

		YS_FALLTHROUGH
	case AST_MEMRD:
		detectSignWidth(width_hint, sign_hint);
		return mkconst_int(location, 0, sign_hint, width_hint);

	default:
		break;
	}

	auto that = clone();
	for (auto &it : that->children)
		it = it->clone_at_zero();
	for (auto &it : that->attributes)
		it.second = it.second->clone();

	that->set_in_lvalue_flag(false);
	that->set_in_param_flag(false);
	that->fixup_hierarchy_flags();

	return that;
}

static bool try_determine_range_width(AstNode *range, int &result_width)
{
	log_assert(range->type == AST_RANGE);

	if (range->children.size() == 1) {
		result_width = 1;
		return true;
	}

	auto left_at_zero_ast = range->children[0]->clone_at_zero();
	auto right_at_zero_ast = range->children[1]->clone_at_zero();

	while (left_at_zero_ast->simplify(true, 1, -1, false)) {}
	while (right_at_zero_ast->simplify(true, 1, -1, false)) {}

	bool ok = false;
	if (left_at_zero_ast->type == AST_CONSTANT
			&& right_at_zero_ast->type == AST_CONSTANT) {
		ok = true;
		result_width = abs(int(left_at_zero_ast->integer - right_at_zero_ast->integer)) + 1;
	}

	return ok;
}

static const std::string auto_nosync_prefix = "\\AutoNosync";

// mark a local variable in an always_comb block for automatic nosync
// consideration
static void mark_auto_nosync(AstNode *block, const AstNode *wire)
{
	log_assert(block->type == AST_BLOCK);
	log_assert(wire->type == AST_WIRE);
	block->set_attribute(auto_nosync_prefix + wire->str, AstNode::mkconst_int(block->location, 1, false));
}

// IEEE 1800-2009 12.8: break/continue in a constant iteration stop or skip it.
// 1 = break, 2 = continue, 0 = neither. The control statement and everything
// after it in this list are removed.
static int trim_uncond_loop_exit(std::vector<std::unique_ptr<AstNode>> &stmts)
{
	for (size_t i = 0; i < stmts.size(); i++) {
		AstNode *stmt = stmts[i].get();
		if (!stmt)
			continue;
		int direct = 0;
		if (stmt->type == AST_TCALL && stmt->str == "\\break")
			direct = 1;
		else if (stmt->type == AST_TCALL && stmt->str == "\\continue")
			direct = 2;
		if (direct) {
			stmts.erase(stmts.begin() + i, stmts.end());
			return direct;
		}
		int nested = 0;
		if (stmt->type == AST_BLOCK)
			nested = trim_uncond_loop_exit(stmt->children);
		else if (stmt->type == AST_CASE && !stmt->children.empty() && stmt->children[0]->type == AST_CONSTANT) {
			AstNode *arm = nullptr;
			int arms = 0;
			for (size_t c = 1; c < stmt->children.size(); c++) {
				if (stmt->children[c] && stmt->children[c]->type == AST_COND) {
					arm = stmt->children[c].get();
					arms++;
				}
			}
			if (arms == 1 && arm) {
				for (auto &ch : arm->children) {
					if (ch && ch->type == AST_BLOCK)
						nested = trim_uncond_loop_exit(ch->children);
				}
			}
		}
		if (!nested)
			continue;
		stmts.erase(stmts.begin() + i + 1, stmts.end());
		return nested;
	}
	return 0;
}

// A break/continue left in an unrolled iteration after constant folding
// (one under a data-dependent condition), not counting nested loops.
static bool contains_loop_exit(const AstNode *node)
{
	if (node->type == AST_TCALL && (node->str == "\\break" || node->str == "\\continue"))
		return true;
	if (node->type == AST_FOR || node->type == AST_WHILE || node->type == AST_REPEAT)
		return false;
	for (auto &c : node->children)
		if (c && contains_loop_exit(c.get()))
			return true;
	return false;
}

// IEEE 1800-2017 12.8 with a data-dependent condition: `break` sets the
// break flag, `continue` the continue flag, and every statement after a
// possible exit runs only while neither is set.
static bool guard_loop_exits(std::vector<std::unique_ptr<AstNode>> &stmts, const std::string &brk,
		const std::string &cont, AstSrcLocType loc)
{
	auto flag_id = [&](const std::string &name) {
		auto id = std::make_unique<AstNode>(loc, AST_IDENTIFIER);
		id->str = name;
		return id;
	};
	for (size_t i = 0; i < stmts.size(); i++) {
		AstNode *stmt = stmts[i].get();
		if (!stmt)
			continue;
		if (stmt->type == AST_TCALL && (stmt->str == "\\break" || stmt->str == "\\continue")) {
			stmts[i] = std::make_unique<AstNode>(loc, AST_ASSIGN_EQ,
					flag_id(stmt->str == "\\break" ? brk : cont), AstNode::mkconst_int(loc, 1, false, 1));
			stmts.erase(stmts.begin() + i + 1, stmts.end());
			return true;
		}
		bool exits = false;
		if (stmt->type == AST_BLOCK)
			exits = guard_loop_exits(stmt->children, brk, cont, loc);
		else if (stmt->type == AST_CASE)
			for (auto &arm : stmt->children)
				if (arm && (arm->type == AST_COND || arm->type == AST_CONDX || arm->type == AST_CONDZ))
					for (auto &body : arm->children)
						if (body && body->type == AST_BLOCK && guard_loop_exits(body->children, brk, cont, loc))
							exits = true;
		if (!exits)
			continue;
		if (i + 1 < stmts.size()) {
			auto rest = std::make_unique<AstNode>(loc, AST_BLOCK);
			for (size_t k = i + 1; k < stmts.size(); k++)
				rest->children.push_back(std::move(stmts[k]));
			stmts.erase(stmts.begin() + i + 1, stmts.end());
			auto go = std::make_unique<AstNode>(loc, AST_REDUCE_BOOL, std::make_unique<AstNode>(loc, AST_LOGIC_NOT,
					std::make_unique<AstNode>(loc, AST_LOGIC_OR, flag_id(brk), flag_id(cont))));
			auto guard = std::make_unique<AstNode>(loc, AST_CASE, std::move(go),
					std::make_unique<AstNode>(loc, AST_COND, AstNode::mkconst_int(loc, 1, false, 1), std::move(rest)));
			stmts.push_back(std::move(guard));
		}
		return true;
	}
	return false;
}

// IEEE 1800-2009 9.6.2: an unconditional disable ends the named block there.
// The block name may already carry an outer prefix (\blk.inner) while the
// disable statement still names the local block (\inner).
static bool disable_names_match(const std::string &block, const std::string &target)
{
	if (block == target)
		return true;
	size_t dot = block.rfind('.');
	if (dot == std::string::npos)
		return false;
	std::string local = block.substr(dot + 1);
	if (!target.empty() && target[0] == '\\')
		return target.substr(1) == local;
	return target == local;
}

static void subst_known_reads(AstNode *node, const dict<std::string, int> &known, bool lvalue)
{
	if (!node)
		return;
	if (!lvalue && node->type == AST_IDENTIFIER && node->children.empty() && known.count(node->str)) {
		AstNode::mkconst_int(node->location, known.at(node->str), true)->cloneInto(*node);
		return;
	}
	for (size_t i = 0; i < node->children.size(); i++) {
		bool child_lv = (node->type == AST_ASSIGN || node->type == AST_ASSIGN_EQ || node->type == AST_ASSIGN_LE) && i == 0;
		subst_known_reads(node->children[i].get(), known, child_lv);
	}
}

// eval_known_int / eval_known_const fold a throwaway copy of an expression,
// but simplify runs in the live context: inlining a call there inserted its
// argument and body statements into the enclosing block (current_block) and
// its wires into the module, before the named block's locals were renamed.
// The stale copy read the locals as module-level implicit wires (a false
// sv_degraded mark; `p = f(u)` in `always_comb begin : b logic u; ..`, or a
// genvar in a generate loop's always block). While probing, a call that
// does not evaluate as a constant function is left as is (not a constant).
static int probe_eval_depth = 0;

static bool eval_known_int(AstNode *expr, const dict<std::string, int> &known, int &out)
{
	if (!expr)
		return false;
	auto copy = expr->clone();
	subst_known_reads(copy.get(), known, false);
	probe_eval_depth++;
	while (copy->simplify(true, 1, -1, false)) { }
	probe_eval_depth--;
	if (copy->type != AST_CONSTANT)
		return false;
	out = copy->asInt(true);
	return true;
}

// Evaluate an expression under the known blocking-assignment values to a
// fully defined constant node (nullptr if that is not possible).
static std::unique_ptr<AstNode> eval_known_const(AstNode *expr, const dict<std::string, int> &known)
{
	if (!expr)
		return nullptr;
	auto copy = expr->clone();
	subst_known_reads(copy.get(), known, false);
	probe_eval_depth++;
	while (copy->simplify(true, 1, -1, false)) { }
	probe_eval_depth--;
	if (copy->type != AST_CONSTANT || !copy->bits_only_01())
		return nullptr;
	return copy;
}

// Does a case item match the selector?  Both are fully defined constants;
// they are compared at the case statement's overall width and signedness
// (IEEE 1800 12.5: extend to the widest operand, signed only if every
// operand is signed) -- never as raw ints, which sign-extended a 4-bit
// unsigned `1111` selector to -1 and dropped the matching `15` arm
// (upstream tests/verilog/unreachable_case_sign.ys, tests/simple/process.v).
static bool const_case_item_match(AstNode *case_stmt, AstNode *sel, AstNode *item)
{
	int width = -1;
	bool is_signed = true;
	case_stmt->detectSignWidth(width, is_signed);
	width = std::max(width, std::max(GetSize(sel->bits), GetSize(item->bits)));
	RTLIL::Const a = sel->bitsAsConst(width, is_signed);
	RTLIL::Const b = item->bitsAsConst(width, is_signed);
	RTLIL::Const match = RTLIL::const_eq(a, b, is_signed, is_signed, 1);
	return match.size() == 1 && match.front() == RTLIL::State::S1;
}

static bool erase_uncond_disable(std::vector<std::unique_ptr<AstNode>> &stmts, const std::string &name, dict<std::string, int> &known)
{
	for (size_t i = 0; i < stmts.size(); i++) {
		AstNode *stmt = stmts[i].get();
		if (!stmt)
			continue;
		if (stmt->type == AST_SVA && stmt->str == "disable" && !stmt->children.empty() &&
				stmt->children[0]->type == AST_IDENTIFIER && disable_names_match(name, stmt->children[0]->str)) {
			stmts.erase(stmts.begin() + i, stmts.end());
			return true;
		}
		if ((stmt->type == AST_ASSIGN_EQ || stmt->type == AST_ASSIGN_LE || stmt->type == AST_ASSIGN) &&
				stmt->children.size() >= 2 && stmt->children[0]->type == AST_IDENTIFIER && stmt->children[0]->children.empty()) {
			int val = 0;
			if (eval_known_int(stmt->children[1].get(), known, val))
				known[stmt->children[0]->str] = val;
			else
				known.erase(stmt->children[0]->str);
		}
		if (stmt->type == AST_BLOCK && erase_uncond_disable(stmt->children, name, known)) {
			stmts.erase(stmts.begin() + i + 1, stmts.end());
			return true;
		}
		if (stmt->type == AST_CASE && !stmt->children.empty()) {
			// case matches compares each pattern with a boolean, not with the dummy selector.
			if (stmt->str == "\\matches")
				continue;
			auto sel = eval_known_const(stmt->children[0].get(), known);
			if (!sel)
				continue;
			// Resolving which arm runs is only sound when every non-default
			// compare is a fully defined constant; in `case (1'b1) ...` style
			// reverse-cases the compares are dynamic and the default arm is
			// not necessarily the one that executes.
			bool resolvable = true;
			for (size_t c = 1; c < stmt->children.size(); c++) {
				AstNode *arm = stmt->children[c].get();
				if (!arm || arm->type != AST_COND) {
					resolvable = false;
					break;
				}
				for (auto &ch : arm->children) {
					if (!ch || ch->type == AST_BLOCK || ch->type == AST_DEFAULT)
						continue;
					if (!eval_known_const(ch.get(), known)) {
						resolvable = false;
						break;
					}
				}
				if (!resolvable)
					break;
			}
			if (!resolvable)
				continue;
			AstNode *taken = nullptr;
			AstNode *fallback = nullptr;
			bool matched = false;
			for (size_t c = 1; c < stmt->children.size(); c++) {
				AstNode *arm = stmt->children[c].get();
				if (!arm || arm->type != AST_COND)
					continue;
				AstNode *blk = nullptr;
				bool is_default = false;
				bool arm_match = false;
				for (auto &ch : arm->children) {
					if (!ch)
						continue;
					if (ch->type == AST_BLOCK)
						blk = ch.get();
					else if (ch->type == AST_DEFAULT)
						is_default = true;
					else {
						auto item = eval_known_const(ch.get(), known);
						if (item && const_case_item_match(stmt, sel.get(), item.get()))
							arm_match = true;
					}
				}
				if (is_default)
					fallback = blk;
				else if (arm_match) {
					taken = blk;
					matched = true;
					break;
				}
			}
			if (!matched)
				taken = fallback;
			if (taken && erase_uncond_disable(taken->children, name, known)) {
				stmts.erase(stmts.begin() + i + 1, stmts.end());
				return true;
			}
		}
	}
	return false;
}

static bool contains_function_return(const AstNode *node)
{
	if (!node)
		return false;
	if (node->type == AST_TCALL && node->str == "\\return")
		return true;
	for (auto &child : node->children)
		if (contains_function_return(child.get()))
			return true;
	return false;
}

static void strip_function_return(std::vector<std::unique_ptr<AstNode>> &stmts)
{
	for (size_t i = 0; i < stmts.size();) {
		AstNode *stmt = stmts[i].get();
		if (stmt && stmt->type == AST_TCALL && stmt->str == "\\return") {
			stmts.erase(stmts.begin() + i);
			continue;
		}
		// A return inside a loop is decided when the loop is unrolled.
		if (stmt && stmt->type != AST_FOR && stmt->type != AST_WHILE && stmt->type != AST_REPEAT)
			strip_function_return(stmt->children);
		i++;
	}
}

static std::unique_ptr<AstNode> return_if(AstNode *cond, std::unique_ptr<AstNode> taken, std::vector<std::unique_ptr<AstNode>> &else_stmts)
{
	auto cas = std::make_unique<AstNode>(cond->location, AST_CASE);
	cas->children.push_back(std::make_unique<AstNode>(cond->location, AST_REDUCE_BOOL, cond->clone()));
	auto then_blk = std::move(taken);
	then_blk->children.push_back(std::make_unique<AstNode>(cond->location, AST_TCALL));
	then_blk->children.back()->str = "\\return";
	auto then_arm = std::make_unique<AstNode>(cond->location, AST_COND, AstNode::mkconst_int(cond->location, 1, false), std::move(then_blk));
	auto else_blk = std::make_unique<AstNode>(cond->location, AST_BLOCK);
	for (auto &stmt : else_stmts)
		else_blk->children.push_back(std::move(stmt));
	else_stmts.clear();
	auto else_arm = std::make_unique<AstNode>(cond->location, AST_COND, std::make_unique<AstNode>(cond->location, AST_DEFAULT), std::move(else_blk));
	cas->children.push_back(std::move(then_arm));
	cas->children.push_back(std::move(else_arm));
	return cas;
}

// Replace a case whose selector is constant with the taken arm. A constant 0
// if around a later live return otherwise hides that return from proc.
static bool unwrap_const_case(std::vector<std::unique_ptr<AstNode>> &stmts)
{
	bool did = false;
	for (size_t i = 0; i < stmts.size();) {
		AstNode *stmt = stmts[i].get();
		if (!stmt) {
			i++;
			continue;
		}
		if (stmt->type == AST_BLOCK) {
			if (unwrap_const_case(stmt->children))
				did = true;
			i++;
			continue;
		}
		if (stmt->type != AST_CASE || stmt->children.empty() || !stmt->children[0]) {
			i++;
			continue;
		}
		for (size_t c = 1; c < stmt->children.size(); c++) {
			AstNode *arm = stmt->children[c].get();
			if (!arm)
				continue;
			for (auto &ch : arm->children)
				if (ch && ch->type == AST_BLOCK && unwrap_const_case(ch->children))
					did = true;
		}
		auto sel = stmt->children[0]->clone();
		while (sel->simplify(true, 1, -1, false)) { }
		if (sel->type != AST_CONSTANT || !sel->bits_only_01()) {
			i++;
			continue;
		}
		int sel_v = sel->asInt(true);
		// Only unwrap when every arm is provably decided by the constant
		// selector: a `case (1'b1) tag[0]: ...` reverse-case has a constant
		// selector but dynamic compares, and folding it to the default arm
		// would drop the conditional logic.
		bool resolvable = true;
		for (size_t c = 1; c < stmt->children.size(); c++) {
			AstNode *arm = stmt->children[c].get();
			if (!arm || arm->type != AST_COND) {
				resolvable = false;
				break;
			}
			for (auto &ch : arm->children) {
				if (!ch || ch->type == AST_BLOCK || ch->type == AST_DEFAULT)
					continue;
				if (ch->type != AST_CONSTANT || !ch->bits_only_01()) {
					resolvable = false;
					break;
				}
			}
			if (!resolvable)
				break;
		}
		if (!resolvable) {
			i++;
			continue;
		}
		AstNode *taken = nullptr;
		AstNode *fallback = nullptr;
		for (size_t c = 1; c < stmt->children.size(); c++) {
			AstNode *arm = stmt->children[c].get();
			if (!arm)
				continue;
			AstNode *blk = nullptr;
			bool is_default = false;
			bool matched = false;
			for (auto &ch : arm->children) {
				if (!ch)
					continue;
				if (ch->type == AST_BLOCK)
					blk = ch.get();
				else if (ch->type == AST_DEFAULT)
					is_default = true;
				else if (ch->type == AST_CONSTANT && ch->asInt(true) == sel_v)
					matched = true;
			}
			if (is_default)
				fallback = blk;
			else if (matched)
				taken = blk;
		}
		if (!taken)
			taken = fallback;
		if (!taken) {
			i++;
			continue;
		}
		std::vector<std::unique_ptr<AstNode>> body;
		for (auto &ch : taken->children)
			body.push_back(std::move(ch));
		stmts.erase(stmts.begin() + i);
		stmts.insert(stmts.begin() + i, std::make_move_iterator(body.begin()), std::make_move_iterator(body.end()));
		did = true;
	}
	return did;
}

// IEEE 1800-2009 13.4.1: return leaves the function. Statements after it run
// only on paths that do not return. Returns true when every path returns.
static bool absorb_function_return(std::vector<std::unique_ptr<AstNode>> &stmts)
{
	auto is_return = [](const AstNode *n) {
		return n && n->type == AST_TCALL && n->str == "\\return";
	};
	auto arm_body = [](AstNode *arm) -> std::vector<std::unique_ptr<AstNode>>* {
		if (!arm)
			return nullptr;
		for (int i = GetSize(arm->children) - 1; i >= 0; i--) {
			if (arm->children[i] && arm->children[i]->type == AST_BLOCK)
				return &arm->children[i]->children;
		}
		return nullptr;
	};
	for (size_t i = 0; i < stmts.size(); i++) {
		AstNode *stmt = stmts[i].get();
		if (!stmt)
			continue;
		if (is_return(stmt)) {
			stmts.erase(stmts.begin() + i, stmts.end());
			return true;
		}
		if (stmt->type == AST_FOR || stmt->type == AST_WHILE || stmt->type == AST_REPEAT) {
			if (stmt->type == AST_FOR) {
				// extra initializers run once before the loop, extra steps
				// after each body (see the for-loop unroller)
				std::vector<std::unique_ptr<AstNode>> extra_inits, extra_steps;
				while (GetSize(stmt->children) > 1 && stmt->children[0] && stmt->children[1] &&
						stmt->children[0]->type == AST_ASSIGN_EQ && stmt->children[1]->type == AST_ASSIGN_EQ) {
					extra_inits.push_back(std::move(stmt->children[1]));
					stmt->children.erase(stmt->children.begin() + 1);
				}
				while (GetSize(stmt->children) > 4 && stmt->children[3] &&
						stmt->children[3]->type != AST_BLOCK && stmt->children[3]->type != AST_GENBLOCK) {
					extra_steps.push_back(std::move(stmt->children[3]));
					stmt->children.erase(stmt->children.begin() + 3);
				}
				if (GetSize(stmt->children) > 3 && stmt->children[3] && stmt->children[3]->type == AST_BLOCK)
					for (auto &step : extra_steps)
						stmt->children[3]->children.push_back(std::move(step));
				if (!extra_inits.empty()) {
					size_t n = extra_inits.size();
					stmts.insert(stmts.begin() + i, std::make_move_iterator(extra_inits.begin()), std::make_move_iterator(extra_inits.end()));
					i += n;
					stmt = stmts[i].get();
				}
			}
			int body_at = stmt->type == AST_FOR ? 3 : 1;
			int cond_at = stmt->type == AST_FOR ? 1 : 0;
			if (body_at < GetSize(stmt->children) && stmt->children[body_at] && stmt->children[body_at]->type == AST_BLOCK &&
					absorb_function_return(stmt->children[body_at]->children)) {
				bool always_once = stmt->type == AST_WHILE && stmt->get_bool_attribute(ID(sv_do));
				auto body = std::move(stmt->children[body_at]);
				if (always_once) {
					std::vector<std::unique_ptr<AstNode>> body_stmts = std::move(body->children);
					stmts.erase(stmts.begin() + i, stmts.end());
					stmts.insert(stmts.begin() + i, std::make_move_iterator(body_stmts.begin()), std::make_move_iterator(body_stmts.end()));
					return true;
				}
				auto cond = stmt->children[cond_at]->clone();
				std::unique_ptr<AstNode> init;
				if (stmt->type == AST_FOR && stmt->children[0])
					init = std::move(stmt->children[0]);
				std::vector<std::unique_ptr<AstNode>> rest;
				for (size_t r = i + 1; r < stmts.size(); r++)
					rest.push_back(std::move(stmts[r]));
				stmts.erase(stmts.begin() + i, stmts.end());
				if (init)
					stmts.push_back(std::move(init));
				stmts.push_back(return_if(cond.get(), std::move(body), rest));
				continue;
			}
		}
		if (stmt->type == AST_BLOCK) {
			if (absorb_function_return(stmt->children)) {
				stmts.erase(stmts.begin() + i + 1, stmts.end());
				return true;
			}
			// A loop inside this block does not see the statements after the block.
			// Bring those statements in, then fold them onto the paths that fall through.
			if (contains_function_return(stmt) && i + 1 < stmts.size()) {
				stmt->set_attribute(ID(sv_noexpand), AstNode::mkconst_int(stmt->location, 1, false));
				for (size_t r = i + 1; r < stmts.size(); r++)
					stmt->children.push_back(std::move(stmts[r]));
				stmts.erase(stmts.begin() + i + 1, stmts.end());
				absorb_function_return(stmt->children);
			}
			continue;
		}
		if (stmt->type == AST_CASE) {
			bool saw_arm = false;
			bool any_return = false;
			bool all_return = true;
			bool has_default = false;
			std::vector<std::vector<std::unique_ptr<AstNode>>*> open_arms;
			std::vector<std::vector<std::unique_ptr<AstNode>>*> returned_arms;
			for (size_t c = 1; c < stmt->children.size(); c++) {
				AstNode *arm = stmt->children[c].get();
				if (arm) {
					for (auto &ch : arm->children)
						if (ch && ch->type == AST_DEFAULT)
							has_default = true;
				}
				auto *body = arm_body(arm);
				if (!body && arm) {
					auto blk = std::make_unique<AstNode>(arm->location, AST_BLOCK);
					std::vector<std::unique_ptr<AstNode>> head;
					bool took_cmp = false;
					for (auto &ch : arm->children) {
						bool stmt = ch && (ch->type == AST_ASSIGN_EQ || ch->type == AST_ASSIGN || ch->type == AST_ASSIGN_LE ||
								ch->type == AST_TCALL || ch->type == AST_CASE || ch->type == AST_FOR || ch->type == AST_WHILE ||
								ch->type == AST_REPEAT || ch->type == AST_BLOCK);
						if (!took_cmp && ch && ch->type != AST_DEFAULT && !stmt) {
							head.push_back(std::move(ch));
							took_cmp = true;
							continue;
						}
						if (ch && ch->type == AST_DEFAULT) {
							head.push_back(std::move(ch));
							continue;
						}
						if (ch)
							blk->children.push_back(std::move(ch));
					}
					if (!blk->children.empty()) {
						head.push_back(std::move(blk));
						arm->children.clear();
						for (auto &ch : head)
							if (ch)
								arm->children.push_back(std::move(ch));
					}
					body = arm_body(arm);
				}
				if (!body)
					continue;
				saw_arm = true;
				if (absorb_function_return(*body)) {
					any_return = true;
					returned_arms.push_back(body);
					continue;
				}
				all_return = false;
				open_arms.push_back(body);
			}
			// No default means an unselected case falls through.
			if (!has_default)
				all_return = false;
			if (saw_arm && all_return) {
				stmts.erase(stmts.begin() + i + 1, stmts.end());
				return true;
			}
			if (any_return && i + 1 < stmts.size()) {
				std::vector<std::unique_ptr<AstNode>> rest;
				for (size_t r = i + 1; r < stmts.size(); r++)
					rest.push_back(std::move(stmts[r]));
				stmts.erase(stmts.begin() + i + 1, stmts.end());
				for (auto *body : open_arms) {
					for (auto &extra : rest)
						body->push_back(extra->clone());
					absorb_function_return(*body);
				}
				if (!has_default) {
					auto blk = std::make_unique<AstNode>(stmt->location, AST_BLOCK);
					for (auto &extra : rest)
						blk->children.push_back(extra->clone());
					auto cond = std::make_unique<AstNode>(stmt->location, AST_COND);
					cond->children.push_back(std::make_unique<AstNode>(stmt->location, AST_DEFAULT));
					cond->children.push_back(std::move(blk));
					stmt->children.push_back(std::move(cond));
				}
			}
			// The return was consumed while folding this arm. Keep it when this
			// case itself can fall through, so a loop around it still stops there.
			if (any_return && !all_return) {
				for (auto *body : returned_arms) {
					bool has_return = false;
					for (auto &ch : *body)
						if (ch && ch->type == AST_TCALL && ch->str == "\\return")
							has_return = true;
					if (has_return)
						continue;
					auto stop = std::make_unique<AstNode>(stmt->location, AST_TCALL);
					stop->str = "\\return";
					body->push_back(std::move(stop));
				}
			}
		}
	}
	return false;
}

// Fold returns in statements inserted for one loop, plus whatever follows the
// loop in the same block. The loop node itself stays owned by that block.
static void absorb_loop_tail(AstNode *block, AstNode *self, size_t from)
{
	if (!block || from > block->children.size())
		return;
	std::vector<std::unique_ptr<AstNode>> head, suffix, held;
	for (size_t k = 0; k < block->children.size(); k++) {
		if (block->children[k].get() == self)
			held.push_back(std::move(block->children[k]));
		else if (k < from)
			head.push_back(std::move(block->children[k]));
		else
			suffix.push_back(std::move(block->children[k]));
	}
	block->children.clear();
	absorb_function_return(suffix);
	for (int pass = 0; pass < 6; pass++) {
		bool opened = unwrap_const_case(suffix);
		absorb_function_return(suffix);
		if (!opened)
			break;
	}
	strip_function_return(suffix);
	for (auto &stmt : head)
		block->children.push_back(std::move(stmt));
	for (auto &stmt : suffix)
		block->children.push_back(std::move(stmt));
	for (auto &stmt : held)
		block->children.push_back(std::move(stmt));
}

// A net driven by one constant continuous assignment has that constant value.
static AstNode *constant_cont_driver(AstNode *mod, const std::string &name)
{
	AstNode *found = nullptr;
	if (!mod)
		return nullptr;
	for (auto &ch : mod->children) {
		if (!ch || ch->children.size() < 2 || !ch->children[0] || !ch->children[1])
			continue;
		if (ch->children[0]->type != AST_IDENTIFIER || ch->children[0]->str != name || !ch->children[0]->children.empty())
			continue;
		if (ch->type == AST_ASSIGN_EQ || ch->type == AST_ASSIGN_LE)
			return nullptr;
		if (ch->type != AST_ASSIGN)
			continue;
		if (ch->children[1]->type != AST_CONSTANT || found)
			return nullptr;
		found = ch->children[1].get();
	}
	return found;
}

// a user identifier, or a local of an unnamed / for-declaration block
// (`$unnamed_block$N.x`), which is a user variable under a generated scope
static bool is_user_scope_name(const std::string &str)
{
	return !str.empty() && (str[0] == '\\' || str.rfind("$unnamed_block$", 0) == 0 || str.rfind("$fordecl_block$", 0) == 0);
}

// block names can be prefixed with an explicit scope during elaboration
static bool is_autonamed_block(const std::string &str) {
	size_t last_dot = str.rfind('.');
	// unprefixed names: autonamed if the first char is a dollar sign
	if (last_dot == std::string::npos)
		return str.at(0) == '$'; // e.g., `$fordecl_block$1`
	// prefixed names: autonamed if the final chunk begins with a dollar sign
	return str.rfind(".$") == last_dot; // e.g., `\foo.bar.$fordecl_block$1`
}

// check a procedural block for auto-nosync markings, remove them, and add
// nosync to local variables as necessary
static void check_auto_nosync(AstNode *node)
{
	std::vector<RTLIL::IdString> attrs_to_drop;
	for (const auto& elem : node->attributes) {
		// skip attributes that don't begin with the prefix
		if (elem.first.compare(0, auto_nosync_prefix.size(),
					auto_nosync_prefix.c_str()))
			continue;

		// delete and remove the attribute once we're done iterating
		attrs_to_drop.push_back(elem.first);

		// find the wire based on the attribute
		std::string wire_name = elem.first.substr(auto_nosync_prefix.size());
		auto it = current_scope.find(wire_name);
		if (it == current_scope.end())
			continue;

		// analyze the usage of the local variable in this block
		IdentUsage ident_usage = always_asgn_before_use(node, wire_name);
		if (ident_usage != IdentUsage::Assigned)
			continue;

		// mark the wire with `nosync`
		AstNode *wire = it->second;
		log_assert(wire->type == AST_WIRE);
		wire->set_attribute(ID::nosync, AstNode::mkconst_int(wire->location, 1, false));
	}

	// remove the attributes we've "consumed"
	for (RTLIL::IdString str : attrs_to_drop) {
		auto it = node->attributes.find(str);
		node->attributes.erase(it);
	}

	// check local variables in any nested blocks
	for (auto& child : node->children)
		check_auto_nosync(child.get());
}

class PackageImporter {
	std::set<std::string> import_items;
	bool is_wildcard;
	const AstNode* node;
public:
	PackageImporter(const AstNode* n, const AstNode* child) : node(n) {
		is_wildcard = child->children.empty();
		// For specific imports, collect the list of items to import
		if (!is_wildcard) {
			for (auto& item : child->children) {
				import_items.insert(item->str);
			}
		}
	}

	void import(std::map<std::string, AstNode*>& scope, AstNode* to_import, std::set<std::string> *wildcard_names = nullptr) const {
		if (!to_import)
			return;
		// Check if this is a specific import and if this item should be imported
		if (!is_wildcard && import_items.count(to_import->str) == 0)
			return;

		if (to_import->type == AST_PARAMETER || to_import->type == AST_LOCALPARAM ||
			to_import->type == AST_TYPEDEF || to_import->type == AST_FUNCTION ||
			to_import->type == AST_TASK || to_import->type == AST_ENUM) {
			// For wildcard imports, check if item already exists (from specific import)
			if (is_wildcard && scope.count(to_import->str) > 0)
				return;
			scope[to_import->str] = to_import;
			if (is_wildcard && wildcard_names)
				wildcard_names->insert(to_import->str);
		}
		if (to_import->type == AST_ENUM) {
			for (auto& enode : to_import->children) {
				log_assert(enode->type==AST_ENUM_ITEM);
				// Check if this enum item should be imported
				if (!is_wildcard && import_items.count(enode->str) == 0)
					continue;
				// For wildcard imports, check if item already exists (from specific import)
				if (is_wildcard && scope.count(enode->str) > 0)
					continue;
				if (scope.count(enode->str) == 0)
					scope[enode->str] = enode.get();
				else
					node->input_error("enum item %s already exists in current scope\n", enode->str);
				if (is_wildcard && wildcard_names)
					wildcard_names->insert(enode->str);
			}
		}
	}
};

// IEEE 1800-2009 11.4.13: inside compares with ==, or with ==? when the
// set element is an integral value that contains x or z.
static std::unique_ptr<AstNode> inside_eq(AstSrcLocType loc, AstNode *lhs, std::unique_ptr<AstNode> item, bool x_wild = true)
{
	bool wild = false;
	if (item->type == AST_CONSTANT) {
		for (auto bit : item->bits)
			if ((x_wild && bit == RTLIL::State::Sx) || bit == RTLIL::State::Sz || bit == RTLIL::State::Sa)
				wild = true;
	}
	if (!wild)
		return std::make_unique<AstNode>(loc, AST_EQ, lhs->clone(), std::move(item));

	std::vector<RTLIL::State> mask, value;
	mask.reserve(item->bits.size());
	value.reserve(item->bits.size());
	for (auto bit : item->bits) {
		bool dont_care = (x_wild && bit == RTLIL::State::Sx) || bit == RTLIL::State::Sz || bit == RTLIL::State::Sa;
		mask.push_back(dont_care ? RTLIL::State::S0 : RTLIL::State::S1);
		value.push_back(dont_care ? RTLIL::State::S0 : bit);
	}
	auto masked = std::make_unique<AstNode>(loc, AST_BIT_AND, lhs->clone(), AstNode::mkconst_bits(loc, mask, false));
	return std::make_unique<AstNode>(loc, AST_EQ, std::move(masked), AstNode::mkconst_bits(loc, value, false));
}

// convert the AST into a simpler AST that has all parameters substituted by their
// values, unrolled for-loops, expanded generate blocks, etc. when this function
// is done with an AST it can be converted into RTLIL using genRTLIL().
//
// this function also does all name resolving and sets the id2ast member of all
// nodes that link to a different node using names and lexical scoping.
// IEEE 1800-2009 10.6: force/assign holds a variable until release/deassign.
// cond is null when the variable is not held. A non-constant if makes the
// hold conditional on that branch.
struct ForceHold {
	std::unique_ptr<AstNode> cond;
	std::unique_ptr<AstNode> value;
};

static bool is_const_bool(const AstNode *n, bool &bit)
{
	if (!n || n->type != AST_CONSTANT)
		return false;
	bit = n->asBool();
	return true;
}

static dict<std::string, ForceHold> clone_holds(const dict<std::string, ForceHold> &held)
{
	dict<std::string, ForceHold> copy;
	for (auto &kv : held) {
		ForceHold h;
		if (kv.second.cond)
			h.cond = kv.second.cond->clone();
		if (kv.second.value)
			h.value = kv.second.value->clone();
		copy[kv.first] = std::move(h);
	}
	return copy;
}

static void apply_force_override(std::vector<std::unique_ptr<AstNode>> &stmts, dict<std::string, ForceHold> &held, dict<std::string, int> &known)
{
	auto note_value = [&](AstNode *stmt) {
		if ((stmt->type != AST_ASSIGN_EQ && stmt->type != AST_ASSIGN_LE && stmt->type != AST_ASSIGN) ||
				stmt->children.size() < 2 || stmt->children[0]->type != AST_IDENTIFIER || !stmt->children[0]->children.empty())
			return;
		int val = 0;
		if (eval_known_int(stmt->children[1].get(), known, val))
			known[stmt->children[0]->str] = val;
		else
			known.erase(stmt->children[0]->str);
	};
	for (size_t i = 0; i < stmts.size();) {
		AstNode *stmt = stmts[i].get();
		if (!stmt) {
			i++;
			continue;
		}
		if (stmt->type == AST_BLOCK) {
			apply_force_override(stmt->children, held, known);
			i++;
			continue;
		}
		if (stmt->type == AST_CASE && !stmt->children.empty()) {
			if (stmt->str == "\\matches") {
				i++;
				continue;
			}
			auto sel = eval_known_const(stmt->children[0].get(), known);
			if (!sel) {
				AstNode *then_blk = nullptr;
				AstNode *else_blk = nullptr;
				for (size_t c = 1; c < stmt->children.size(); c++) {
					AstNode *arm = stmt->children[c].get();
					if (!arm || arm->type != AST_COND)
						continue;
					AstNode *blk = nullptr;
					bool is_default = false;
					for (auto &ch : arm->children) {
						if (!ch)
							continue;
						if (ch->type == AST_BLOCK)
							blk = ch.get();
						else if (ch->type == AST_DEFAULT)
							is_default = true;
					}
					if (is_default)
						else_blk = blk;
					else if (!then_blk)
						then_blk = blk;
				}
				auto then_held = clone_holds(held);
				auto else_held = clone_holds(held);
				auto then_known = known;
				auto else_known = known;
				if (then_blk)
					apply_force_override(then_blk->children, then_held, then_known);
				if (else_blk)
					apply_force_override(else_blk->children, else_held, else_known);
				pool<std::string> names;
				for (auto &kv : held)
					names.insert(kv.first);
				for (auto &kv : then_held)
					names.insert(kv.first);
				for (auto &kv : else_held)
					names.insert(kv.first);
				dict<std::string, ForceHold> merged;
				for (auto &name : names) {
					auto take = [](dict<std::string, ForceHold> &src, const std::string &name) {
						ForceHold h;
						if (src.count(name) && src.at(name).cond) {
							h.cond = src.at(name).cond->clone();
							if (src.at(name).value)
								h.value = src.at(name).value->clone();
						}
						return h;
					};
					ForceHold th = take(then_held, name);
					ForceHold eh = take(else_held, name);
					if (!th.cond && !eh.cond)
						continue;
					if (!th.cond)
						th.cond = AstNode::mkconst_int(stmt->location, 0, false);
					if (!eh.cond)
						eh.cond = AstNode::mkconst_int(stmt->location, 0, false);
					if (!th.value)
						th.value = AstNode::mkconst_int(stmt->location, 0, false);
					if (!eh.value)
						eh.value = AstNode::mkconst_int(stmt->location, 0, false);
					ForceHold h;
					bool tb, eb;
					if (is_const_bool(th.cond.get(), tb) && is_const_bool(eh.cond.get(), eb) && tb == eb) {
						if (!tb)
							continue;
						h.cond = AstNode::mkconst_int(stmt->location, 1, false);
						h.value = std::make_unique<AstNode>(stmt->location, AST_TERNARY, stmt->children[0]->clone(),
								std::move(th.value), std::move(eh.value));
					} else {
						h.cond = std::make_unique<AstNode>(stmt->location, AST_TERNARY, stmt->children[0]->clone(),
								std::move(th.cond), std::move(eh.cond));
						h.value = std::make_unique<AstNode>(stmt->location, AST_TERNARY, stmt->children[0]->clone(),
								std::move(th.value), std::move(eh.value));
					}
					merged[name] = std::move(h);
				}
				held = std::move(merged);
				known.clear();
				i++;
				continue;
			}
			// The selector is a fully defined constant, so this case can be
			// resolved to a single arm. That is only sound when every non-default
			// compare is a fully defined constant too: in `case (1'b1) tag[0]: ...`
			// style reverse-case statements the compares are dynamic, and erasing
			// all arms but the default would drop live logic.
			AstNode *taken = nullptr;
			AstNode *fallback = nullptr;
			size_t taken_at = 0;
			size_t fallback_at = 0;
			bool matched = false;
			bool resolvable = true;
			for (size_t c = 1; c < stmt->children.size(); c++) {
				AstNode *arm = stmt->children[c].get();
				if (!arm || arm->type != AST_COND) {
					// casex/casez arms (AST_CONDX/AST_CONDZ) match with
					// wildcard semantics that a plain compare cannot prove.
					resolvable = false;
					break;
				}
				AstNode *blk = nullptr;
				bool is_default = false;
				bool arm_match = false;
				for (auto &ch : arm->children) {
					if (!ch)
						continue;
					if (ch->type == AST_BLOCK)
						blk = ch.get();
					else if (ch->type == AST_DEFAULT)
						is_default = true;
					else {
						auto item = eval_known_const(ch.get(), known);
						if (!item) {
							resolvable = false;
							break;
						}
						if (const_case_item_match(stmt, sel.get(), item.get()))
							arm_match = true;
					}
				}
				if (!resolvable)
					break;
				if (is_default) {
					fallback = blk;
					fallback_at = c;
				} else if (arm_match && !matched) {
					taken = blk;
					taken_at = c;
					matched = true;
				}
			}
			if (!resolvable) {
				i++;
				continue;
			}
			if (!matched) {
				taken = fallback;
				taken_at = fallback_at;
			}
			for (size_t c = stmt->children.size(); c > 1; c--)
				if (!taken || c - 1 != taken_at)
					stmt->children.erase(stmt->children.begin() + (c - 1));
			if (taken)
				apply_force_override(taken->children, held, known);
			i++;
			continue;
		}
		if (stmt->type == AST_TCALL && (stmt->str == "\\release" || stmt->str == "\\deassign")) {
			if (!stmt->children.empty() && stmt->children[0]->type == AST_IDENTIFIER && stmt->children[0]->children.empty()) {
				held.erase(stmt->children[0]->str);
				known.erase(stmt->children[0]->str);
			}
			stmts.erase(stmts.begin() + i);
			continue;
		}
		if ((stmt->type == AST_ASSIGN_EQ || stmt->type == AST_ASSIGN_LE) && stmt->children.size() >= 2 &&
				stmt->children[0]->type == AST_IDENTIFIER && stmt->children[0]->children.empty()) {
			const std::string &name = stmt->children[0]->str;
			if (stmt->get_bool_attribute(ID(sv_force))) {
				ForceHold h;
				h.cond = AstNode::mkconst_int(stmt->location, 1, false);
				h.value = stmt->children[1]->clone();
				held.erase(name);
				held[name] = std::move(h);
				stmt->attributes.erase(ID(sv_force));
				note_value(stmt);
				i++;
				continue;
			}
			if (held.count(name) && held.at(name).cond) {
				bool on = false;
				if (is_const_bool(held.at(name).cond.get(), on) && on) {
					stmts.erase(stmts.begin() + i);
					continue;
				}
				auto rhs = std::move(stmt->children[1]);
				stmt->children[1] = std::make_unique<AstNode>(stmt->location, AST_TERNARY,
						held.at(name).cond->clone(), held.at(name).value->clone(), std::move(rhs));
				known.erase(name);
				i++;
				continue;
			}
			note_value(stmt);
		}
		i++;
	}
}

bool AstNode::simplify(bool const_fold, int stage, int width_hint, bool sign_hint)
{
	{
		// Clang assumes `this` is non-null and would otherwise delete this test.
		uintptr_t raw = reinterpret_cast<uintptr_t>(this);
		asm volatile("" : "+r"(raw));
		if (raw < 4096)
			return false;
	}
	static int recursion_counter = 0;
	static bool deep_recursion_warning = false;

	if (recursion_counter >= 1000) {
		static bool warned = false;
		if (!warned) {
			log_warning("Deep recursion in AST simplifier.\nDoes this design contain overly long or deeply nested expressions, or excessive recursion?\n");
			warned = true;
		}
		return false;
	}
	recursion_counter++;


	// Class declarations are simulation-only: keep the parsed AST (for
	// completeness) but never elaborate the body.
	if (type == AST_CLASS) {
		basic_prep = true;
		recursion_counter--;
		return false;
	}

	// Out-of-block class method definitions (parsed at package scope) are
	// simulation-only as well: their bodies reference class members and
	// constructs (this/super) that only make sense in class context.
	if ((type == AST_FUNCTION || type == AST_TASK) &&
			attributes.count(ID(sv_class_method_def)) != 0) {
		basic_prep = true;
		recursion_counter--;
		return false;
	}

	static bool unevaluated_tern_branch = false;

	// IEEE 1800-2009 11.12: a let is a textual macro. Substitute before the
	// assertion-instance guard, which would otherwise leave the call intact.
	if (type == AST_FCALL && !str.empty() && (str.size() < 2 || str[1] != '$')) {
		AstNode *let = nullptr;
		auto find_let = [&](AstNode *scope) {
			if (!scope)
				return;
			for (auto &ch : scope->children) {
				if (ch && ch->type == AST_SVA && ch->str == "let_decl" && !ch->children.empty() &&
						ch->children[0]->str == str)
					let = ch.get();
				if (ch && ch->type == AST_PACKAGE) {
					for (auto &inner : ch->children) {
						if (inner->type == AST_SVA && inner->str == "let_decl" && !inner->children.empty() &&
								inner->children[0]->str == str)
							let = inner.get();
					}
				}
			}
		};
		find_let(current_ast_mod);
		find_let(current_ast);
		if (let && let->children.size() >= 2) {
			for (auto &child : children)
				while (child && child->simplify(true, stage, -1, false)) { }
			auto body = let->children[1]->clone();
			std::function<void(AstNode *)> subst = [&](AstNode *node) {
				for (size_t pi = 2; pi < let->children.size() && pi - 2 < children.size(); pi++) {
					if (node->type == AST_IDENTIFIER && node->str == let->children[pi]->str) {
						children[pi - 2]->clone()->cloneInto(*node);
						return;
					}
				}
				for (auto &ch : node->children)
					if (ch)
						subst(ch.get());
			};
			subst(body.get());
			while (body->simplify(true, stage, width_hint, sign_hint)) { }
			body->cloneInto(*this);
			fixup_hierarchy_flags();
			recursion_counter--;
			return true;
		}
	}

	// A let with no ports is an expression name, not a call: `let seven = 7; y = seven`.
	if (type == AST_IDENTIFIER && !in_lvalue && children.empty() && !str.empty()) {
		AstNode *let = nullptr;
		auto find_let_id = [&](AstNode *scope) {
			if (!scope)
				return;
			for (auto &ch : scope->children) {
				if (ch && ch->type == AST_SVA && ch->str == "let_decl" && ch->children.size() == 2 &&
						ch->children[0]->str == str)
					let = ch.get();
			}
		};
		find_let_id(current_ast_mod);
		find_let_id(current_ast);
		if (let) {
			auto body = let->children[1]->clone();
			while (body->simplify(true, stage, width_hint, sign_hint)) { }
			body->cloneInto(*this);
			fixup_hierarchy_flags();
			recursion_counter--;
			return true;
		}
	}

	// IEEE 1800-2009 17.2: a checker declaration is a template. Its body is
	// cloned and elaborated per instantiation (below), so the declaration
	// itself must not be simplified in place -- doing so would flatten the
	// body into the module and leave the template empty.
	if (type == AST_SVA && str == "checker_decl") {
		basic_prep = true;
		recursion_counter--;
		return false;
	}

	// IEEE 1800-2009 6.19.5: enum.next / enum.prev walk the declaration order and wrap.
	// first, last, and num are properties of the type, including a typedef name.
	std::string enum_method, enum_prefix;
	if (type == AST_FCALL) {
		auto ends = [&](const char *suf) {
			size_t n = strlen(suf);
			return str.size() >= n && str.compare(str.size() - n, n, suf) == 0;
		};
		if (ends(".next")) { enum_method = "next"; enum_prefix = str.substr(0, str.size() - 5); }
		else if (ends(".prev")) { enum_method = "prev"; enum_prefix = str.substr(0, str.size() - 5); }
		else if (ends(".first")) { enum_method = "first"; enum_prefix = str.substr(0, str.size() - 6); }
		else if (ends(".last")) { enum_method = "last"; enum_prefix = str.substr(0, str.size() - 5); }
		else if (ends(".num")) { enum_method = "num"; enum_prefix = str.substr(0, str.size() - 4); }
		else if (ends(".name")) { enum_method = "name"; enum_prefix = str.substr(0, str.size() - 5); }
	}
	if (!enum_method.empty()) {
		bool prev = enum_method == "prev";
		std::string prefix = enum_prefix;
		bool on_expr = !str.empty() && str[0] == '.';
		AstNode *enum_node = nullptr;
		std::unique_ptr<AstNode> base;
		auto scan_enum = [&](AstNode *scope) {
			if (!scope || enum_node)
				return;
			for (auto &ch : scope->children) {
				if (!ch)
					continue;
				if (ch->type == AST_ENUM) {
					for (auto &item : ch->children) {
						if (item && item->str == prefix) {
							enum_node = ch.get();
							if (!item->children.empty())
								base = item->children[0]->clone();
						}
					}
				}
				if (ch->type == AST_TYPEDEF && ch->str == prefix && !ch->children.empty() &&
						ch->children[0]->attributes.count(ID::enum_type)) {
					std::string ename = ch->children[0]->attributes.at(ID::enum_type)->str.c_str();
					for (auto &sib : scope->children) {
						if (sib && sib->type == AST_ENUM && sib->str == ename)
							enum_node = sib.get();
					}
				}
				if ((ch->type == AST_WIRE || ch->type == AST_MEMORY) && ch->str == prefix && ch->attributes.count(ID::enum_type)) {
					std::string ename = ch->attributes.at(ID::enum_type)->str.c_str();
					for (auto &sib : scope->children) {
						if (sib && sib->type == AST_ENUM && sib->str == ename)
							enum_node = sib.get();
					}
					if (!base) {
						base = std::make_unique<AstNode>(location, AST_IDENTIFIER);
						base->str = prefix;
					}
				}
			}
		};
		// a variable outside the module's top level (a block local, e.g.
		// an inlined function's for-init `StateEnumT t = t.first()`)
		// is found through the scope; its enum is registered there too
		auto scope_enum = [&]() {
			if (enum_node || !current_scope.count(prefix))
				return;
			AstNode *d = current_scope.at(prefix);
			if (!d || (d->type != AST_WIRE && d->type != AST_MEMORY) || !d->attributes.count(ID::enum_type))
				return;
			std::string ename = d->attributes.at(ID::enum_type)->str.c_str();
			if (current_scope.count(ename) && current_scope.at(ename)->type == AST_ENUM)
				enum_node = current_scope.at(ename);
			if (enum_node && !base) {
				base = std::make_unique<AstNode>(location, AST_IDENTIFIER);
				base->str = prefix;
			}
		};
		scan_enum(current_ast_mod);
		scan_enum(current_ast);
		scope_enum();
		// expr.method(): the receiver is the first argument, not a name prefix.
		if (prefix.empty() && !children.empty()) {
			AstNode *recv = children[0].get();
			if (recv && recv->type == AST_IDENTIFIER) {
				prefix = recv->str;
				base = recv->clone();
				enum_node = nullptr;
				scan_enum(current_ast_mod);
				scan_enum(current_ast);
				scope_enum();
			} else if (recv && recv->type == AST_CAST_SIZE && recv->children.size() >= 2) {
				std::function<void(AstNode *)> find_ty = [&](AstNode *n) {
					if (!n || !prefix.empty())
						return;
					if (n->type == AST_WIRETYPE)
						prefix = n->str;
					for (auto &ch : n->children)
						find_ty(ch.get());
				};
				find_ty(recv->children[0].get());
				base = recv->children[1]->clone();
				enum_node = nullptr;
				scan_enum(current_ast_mod);
				scan_enum(current_ast);
			}
		}
		// A block-local receiver (`begin : b st_e t; t = t.first(); ...`,
		// a for-init `st_e t`) is renamed into scope only when its named
		// block is expanded, after the block's statements were first
		// simplified: leave the call for that later pass. An unresolved
		// call that survives is still an error in genrtlil.
		if (!enum_node && !prefix.empty() && !current_scope.count(prefix) && current_top_block &&
				prefix.find('.') == std::string::npos) {
			bool declared = false;
			std::function<void(AstNode *)> find_decl = [&](AstNode *n) {
				if (!n || declared)
					return;
				if ((n->type == AST_WIRE || n->type == AST_MEMORY) && n->str == prefix)
					declared = true;
				for (auto &c : n->children)
					find_decl(c.get());
			};
			find_decl(current_top_block);
			if (declared) {
				recursion_counter--;
				return false;
			}
		}
		// The default elaboration of a module whose type parameter defaults
		// to a plain vector (`parameter type StateEnumT = logic [W-1:0]`,
		// OpenTitan prim_sparse_fsm_flop) calls enum methods on that
		// vector. Treat the unoverridden type as the enumeration of all its
		// values; a derived module has the real enum. Plain misuse on a
		// non-parameter type still falls through to the error.
		if (!enum_node && !prefix.empty() && enum_method != "name" && current_scope.count(prefix)) {
			AstNode *d = current_scope.at(prefix);
			AstNode *td = nullptr;
			if (d && (d->type == AST_WIRE || d->type == AST_MEMORY) && d->attributes.count(ID::wiretype)) {
				std::string tname = d->attributes.at(ID::wiretype)->str.c_str();
				if (current_scope.count(tname) && current_scope.at(tname)->type == AST_TYPEDEF &&
						current_scope.at(tname)->attributes.count(ID(type_param)))
					td = current_scope.at(tname);
			}
			int w = (td && d->range_valid) ? std::abs(d->range_left - d->range_right) + 1 : -1;
			if (td && w >= 1 && w <= 16) {
				log_warning("%s: enum method `%s' on `%s', whose type parameter `%s' defaults to a %d-bit vector; "
						"treating the type as the enumeration of all its values.\n",
						location.to_string().c_str(), enum_method.c_str(), prefix.c_str() + 1, td->str.c_str() + 1, w);
				int n = 1 << w;
				std::unique_ptr<AstNode> result;
				if (enum_method == "first" || enum_method == "last" || enum_method == "num") {
					result = AstNode::mkconst_int(location, enum_method == "first" ? 0 : enum_method == "last" ? n - 1 : n, true);
				} else {
					int step = 1;
					AstNode *step_node = on_expr ? (children.size() >= 2 ? children[1].get() : nullptr)
							: (!children.empty() ? children[0].get() : nullptr);
					if (step_node) {
						while (step_node->simplify(true, stage, -1, false)) { }
						if (step_node->type == AST_CONSTANT)
							step = step_node->integer;
					}
					int delta = (((prev ? -step : step) % n) + n) % n;
					auto recv = std::make_unique<AstNode>(location, AST_IDENTIFIER);
					recv->str = prefix;
					auto sum = std::make_unique<AstNode>(location, AST_ADD,
							std::make_unique<AstNode>(location, AST_TO_UNSIGNED, std::move(recv)),
							AstNode::mkconst_int(location, delta, false, 32));
					result = std::make_unique<AstNode>(location, AST_BIT_AND, std::move(sum),
							AstNode::mkconst_int(location, n - 1, false, 32));
				}
				result->location = location;
				result->cloneInto(*this);
				fixup_hierarchy_flags();
				recursion_counter--;
				return true;
			}
		}
		if (enum_node && (base || enum_method == "first" || enum_method == "last" || enum_method == "num")) {
			enum_node->allocateDefaultEnumValues();
			std::vector<int> values;
			std::vector<std::string> names;
			for (auto &item : enum_node->children) {
				if (item && item->type == AST_ENUM_ITEM && !item->children.empty() &&
						item->children[0]->type == AST_CONSTANT) {
					values.push_back(item->children[0]->integer);
					std::string nm = item->str;
					if (!nm.empty() && nm[0] == '\\')
						nm = nm.substr(1);
					names.push_back(nm);
				}
			}
			if (enum_method == "name" && base) {
				while (base->simplify(true, stage, -1, false)) { }
				if (base->type == AST_IDENTIFIER && current_ast_mod) {
					for (auto &ch : current_ast_mod->children) {
						if (!ch || ch->type != AST_ASSIGN || ch->children.size() < 2)
							continue;
						if (ch->children[0]->type != AST_IDENTIFIER || ch->children[0]->str != base->str ||
								!ch->children[0]->children.empty())
							continue;
						auto rhs = ch->children[1]->clone();
						while (rhs->simplify(true, stage, -1, false)) { }
						if (rhs->type == AST_CONSTANT) {
							rhs->cloneInto(*base);
							break;
						}
					}
				}
				if (base->type == AST_CONSTANT) {
					std::string picked;
					for (size_t i = 0; i < values.size(); i++) {
						if (values[i] == base->integer) {
							picked = names[i];
							break;
						}
					}
					auto result = AstNode::mkconst_str(location, picked);
					result->cloneInto(*this);
					fixup_hierarchy_flags();
					recursion_counter--;
					return true;
				}
				std::unique_ptr<AstNode> sel = AstNode::mkconst_str(location, "");
				for (int i = (int)values.size() - 1; i >= 0; i--) {
					auto eq = std::make_unique<AstNode>(location, AST_EQ, base->clone(),
							AstNode::mkconst_int(location, values[i], true));
					sel = std::make_unique<AstNode>(location, AST_TERNARY, std::move(eq),
							AstNode::mkconst_str(location, names[i]), std::move(sel));
				}
				sel->cloneInto(*this);
				fixup_hierarchy_flags();
				recursion_counter--;
				return true;
			}
			if (enum_method == "name") {
				recursion_counter--;
				return false;
			}
			if (!values.empty() && (enum_method == "first" || enum_method == "last" || enum_method == "num")) {
				int picked = enum_method == "first" ? values.front() : enum_method == "last" ? values.back() : (int)values.size();
				auto result = AstNode::mkconst_int(location, picked, true);
				result->cloneInto(*this);
				fixup_hierarchy_flags();
				recursion_counter--;
				return true;
			}
			int step = 1;
			AstNode *step_node = nullptr;
			if (on_expr) {
				if (children.size() >= 2)
					step_node = children[1].get();
			} else if (!children.empty()) {
				step_node = children[0].get();
			}
			if (step_node) {
				while (step_node->simplify(true, stage, -1, false)) { }
				if (step_node->type == AST_CONSTANT)
					step = step_node->integer;
			}
			while (base->simplify(true, stage, -1, false)) { }
			auto result = std::make_unique<AstNode>(location, AST_CONSTANT);
			if (!values.empty() && base->type == AST_CONSTANT) {
				int idx = 0;
				bool found = false;
				for (size_t i = 0; i < values.size(); i++) {
					if (values[i] == base->integer) {
						idx = (int)i;
						found = true;
						break;
					}
				}
				int n = (int)values.size();
				int pos = found ? idx : 0;
				if (found) {
					int delta = prev ? -step : step;
					pos = ((pos + delta) % n + n) % n;
				}
				result = AstNode::mkconst_int(location, values[pos], true);
			} else if (!values.empty()) {
				int n = (int)values.size();
				std::unique_ptr<AstNode> sel = AstNode::mkconst_int(location, values[0], true);
				for (int i = n - 1; i >= 0; i--) {
					int delta = prev ? -step : step;
					int pos = ((i + delta) % n + n) % n;
					auto eq = std::make_unique<AstNode>(location, AST_EQ, base->clone(),
							AstNode::mkconst_int(location, values[i], true));
					sel = std::make_unique<AstNode>(location, AST_TERNARY, std::move(eq),
							AstNode::mkconst_int(location, values[pos], true), std::move(sel));
				}
				result = std::move(sel);
			}
			if (!values.empty()) {
				result->cloneInto(*this);
				fixup_hierarchy_flags();
				recursion_counter--;
				return true;
			}
		}
	}

	// Assertion trees keep calls intact: sampled-value functions and
	// sequence/property instances are not user functions.
	bool sva_instance = false;
	if (type == AST_FCALL) {
		auto scan_scope = [&](AstNode *scope) {
			if (!scope)
				return;
			for (auto &decl : scope->children) {
				AstNode *look = decl.get();
				if (look->type == AST_PACKAGE) {
					for (auto &inner : look->children) {
						if (inner->type == AST_SVA && (inner->str == "property_decl" || inner->str == "sequence_decl") &&
								!inner->children.empty() && inner->children[0]->str == str)
							sva_instance = true;
					}
				}
				if (look->type == AST_SVA && (look->str == "property_decl" || look->str == "sequence_decl") &&
						!look->children.empty() && look->children[0]->str == str)
					sva_instance = true;
			}
		};
		scan_scope(current_ast_mod);
		scan_scope(current_ast);
	}
	if (type == AST_FCALL && (current_sva_context || sva_instance)) {
		// Assertion trees keep calls intact: sampled-value functions and
		// sequence/property instances are not user functions.  Genuine user
		// functions (e.g. in local-variable assignment expressions) still
		// inline normally.
		std::string fname = str;
		bool is_user_function = false;
		auto scan_fns = [&](AstNode *scope) {
			if (!scope || is_user_function)
				return;
			for (auto &decl : scope->children)
				if (decl && decl->type == AST_FUNCTION && decl->str == fname)
					is_user_function = true;
		};
		scan_fns(current_scope.count(fname) ? current_scope[fname] : nullptr);
		scan_fns(current_ast_mod);
		scan_fns(current_ast);
		if (!is_user_function && current_scope.count(fname) && current_scope[fname]->type == AST_FUNCTION)
			is_user_function = true;
		if (!is_user_function) {
			for (auto &child : children)
				while (child && child->simplify(const_fold, stage, -1, false)) { }
			recursion_counter--;
			return false;
		}
	}
	std::unique_ptr<AstNode> newNode = nullptr;
	bool did_something = false;

	// IEEE 1800-2009 11.4.6: x and z in the right operand are wildcards.
	if (type == AST_SVA && (str == "weq" || str == "wne" || str == "meq") && children.size() >= 2) {
		auto lhs = children[0]->clone();
		auto rhs = children[1]->clone();
		while (lhs->simplify(true, stage, -1, false)) { }
		while (rhs->simplify(true, stage, -1, false)) { }
		if (lhs->type == AST_IDENTIFIER && lhs->children.empty()) {
			AstNode *drv = constant_cont_driver(current_ast_mod, lhs->str);
			if (drv)
				lhs = drv->clone();
		}
		// case matches: ? and z are don't-cares. ==? also treats x as one.
		auto eq = inside_eq(location, lhs.get(), std::move(rhs), str != "meq");
		if (str == "wne")
			eq = std::make_unique<AstNode>(location, AST_LOGIC_NOT, std::move(eq));
		eq->location = location;
		eq->cloneInto(*this);
		fixup_hierarchy_flags();
		recursion_counter--;
		return true;
	}

	// IEEE 1800-2009 12.7 / 18.5.8: foreach walks declared dimensions left to right.
	if (type == AST_SVA && str == "foreach" && children.size() >= 3 && children.back() && children.back()->type == AST_BLOCK) {
		auto arr = children[0]->clone();
		while (arr->simplify(true, stage, -1, false)) { }
		AstNode *mem = arr->id2ast;
		int nvars = GetSize(children) - 2;
		if (mem && (mem->type == AST_WIRE || mem->type == AST_MEMORY) && nvars <= GetSize(mem->dimensions) && nvars > 0) {
			// An empty slot does not iterate that dimension.
			struct Slot { int child; int left, right, step, cur; };
			std::vector<Slot> slots;
			bool ready = true;
			for (int d = 0; d < nvars; d++) {
				AstNode *var = children[d + 1].get();
				if (!var || var->type != AST_IDENTIFIER || var->str.empty())
					continue;
				auto &dim = mem->dimensions[d];
				if (dim.range_width <= 0) {
					ready = false;
					break;
				}
				int lo = dim.range_right;
				int hi = lo + dim.range_width - 1;
				Slot s;
				s.child = d + 1;
				s.left = dim.range_swapped ? lo : hi;
				s.right = dim.range_swapped ? hi : lo;
				s.step = s.left <= s.right ? 1 : -1;
				s.cur = s.left;
				slots.push_back(s);
			}
			if (ready && !slots.empty()) {
				auto block = std::make_unique<AstNode>(location, AST_BLOCK);
				auto done = [&]() {
					for (auto &s : slots)
						if (s.cur != s.right)
							return false;
					return true;
				};
				while (true) {
					dict<std::string, int> known;
					for (auto &s : slots)
						known[children[s.child]->str] = s.cur;
					auto body = children.back()->clone();
					subst_known_reads(body.get(), known, false);
					block->children.push_back(std::move(body));
					if (done())
						break;
					for (int d = GetSize(slots) - 1; d >= 0; d--) {
						slots[d].cur += slots[d].step;
						if (slots[d].cur != slots[d].right + slots[d].step)
							break;
						slots[d].cur = slots[d].left;
					}
				}
				block->cloneInto(*this);
				fixup_hierarchy_flags();
				recursion_counter--;
				return true;
			}
		}
	}

	// IEEE 1800-2009 10.11: alias joins nets, including a constant part-select.
	// A bare port name stays the canonical net.
	if (type == AST_SVA && str == "alias") {
		auto select_bounds = [](AstNode *id, bool &whole, int &hi, int &lo) {
			whole = false;
			if (!id || id->type != AST_IDENTIFIER)
				return false;
			if (id->children.empty()) {
				whole = true;
				return true;
			}
			if (id->children.size() != 1 || id->children[0]->type != AST_RANGE)
				return false;
			auto &bounds = id->children[0]->children;
			if (bounds.size() == 1 && bounds[0]->type == AST_CONSTANT) {
				hi = lo = bounds[0]->asInt(true);
				return true;
			}
			if (bounds.size() == 2 && bounds[0]->type == AST_CONSTANT && bounds[1]->type == AST_CONSTANT) {
				hi = bounds[0]->asInt(true);
				lo = bounds[1]->asInt(true);
				return true;
			}
			return false;
		};
		auto same_net = [&](AstNode *a, AstNode *b) {
			bool aw, bw;
			int ah, al, bh, bl;
			if (!select_bounds(a, aw, ah, al) || !select_bounds(b, bw, bh, bl))
				return false;
			if (a->str != b->str || aw != bw)
				return false;
			return aw || (ah == bh && al == bl);
		};
		bool shaped = children.size() >= 2;
		for (auto &ch : children) {
			if (ch && ch->type == AST_IDENTIFIER) {
				for (auto &sel : ch->children)
					while (sel && sel->simplify(true, stage, -1, false)) { }
			}
			bool whole;
			int hi, lo;
			if (!select_bounds(ch.get(), whole, hi, lo))
				shaped = false;
		}
		if (shaped && current_ast_mod) {
			auto is_port = [&](AstNode *id) {
				if (!id->children.empty())
					return false;
				for (auto &ch : current_ast_mod->children)
					if (ch && ch->str == id->str && ch->port_id != 0 &&
							(ch->type == AST_WIRE || ch->type == AST_MEMORY))
						return true;
				return false;
			};
			size_t canon_i = 0;
			for (size_t i = 1; i < children.size(); i++)
				if (is_port(children[i].get()) && !is_port(children[canon_i].get()))
					canon_i = i;
			AstNode *canon = children[canon_i].get();
			std::function<void(AstNode *)> repl = [&](AstNode *node) {
				if (!node || node == this)
					return;
				if (node->type == AST_IDENTIFIER && !same_net(node, canon)) {
					for (size_t i = 0; i < children.size(); i++) {
						if (i == canon_i || !same_net(node, children[i].get()))
							continue;
						bool lvalue = node->in_lvalue;
						canon->clone()->cloneInto(*node);
						node->in_lvalue = lvalue;
						return;
					}
				}
				for (auto &ch : node->children)
					repl(ch.get());
			};
			repl(current_ast_mod);
		} else if (children.size() >= 2) {
			degrade(location.to_loc(), "Alias of a non-constant select is ignored.\n");
		}
		type = AST_BLOCK;
		str.clear();
		children.clear();
		fixup_hierarchy_flags();
		recursion_counter--;
		return true;
	}

	// A disable that simplify did not use as a block exit must not become an $sva cell.
	if (type == AST_SVA && str == "disable") {
		degrade(location.to_loc(), "disable that is not an unconditional exit of its named block is ignored.\n");
		type = AST_BLOCK;
		str.clear();
		children.clear();
		fixup_hierarchy_flags();
		recursion_counter--;
		return true;
	}

	// IEEE 1800-2009 16.9.3: $sampled(e) is the current sampled value, not a delay.
	if (type == AST_FCALL && str == "\\$sampled" && !children.empty()) {
		children[0]->clone()->cloneInto(*this);
		fixup_hierarchy_flags();
		recursion_counter--;
		return true;
	}

	// IEEE 1800-2009 11.4.14: {<<slice{expr}} reverses slice-sized blocks.
	if (type == AST_SVA && str == "stream_l" && children.size() >= 2) {
		while (children[0]->simplify(true, stage, -1, false)) { }
		while (children[1]->simplify(true, stage, -1, false)) { }
		int slice = 1;
		if (children[0]->type == AST_CONSTANT && children[0]->asInt(false) > 0)
			slice = children[0]->asInt(false);
		AstNode *payload = children[1].get();
		int width = -1;
		bool sgn = false;
		payload->detectSignWidth(width, sgn);
		if (width > 0 && current_ast_mod) {
			std::vector<std::pair<int, int>> blocks;
			for (int pos = width; pos > 0; ) {
				int sz = std::min(slice, pos);
				blocks.push_back({pos - 1, pos - sz});
				pos -= sz;
			}
			std::reverse(blocks.begin(), blocks.end());
			// blocks of a bare identifier are part-selects of it; any other
			// payload is sliced in place as sz'($unsigned(payload) >> lo).  (It
			// used to be hoisted into a module-level `assign $stream$ = payload`,
			// which reads a function's or always block's local -- `return
			// {<<6{col}}` in OpenTitan prim_lfsr revcol -- outside its process,
			// i.e. as x.)
			bool bare = payload->type == AST_IDENTIFIER && payload->children.empty();
			auto cat = std::make_unique<AstNode>(location, AST_CONCAT);
			for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
				if (bare) {
					auto bit = std::make_unique<AstNode>(location, AST_IDENTIFIER);
					bit->str = payload->str;
					auto range = std::make_unique<AstNode>(location, AST_RANGE);
					range->children.push_back(mkconst_int(location, it->first, true));
					range->children.push_back(mkconst_int(location, it->second, true));
					bit->children.push_back(std::move(range));
					cat->children.push_back(std::move(bit));
				} else {
					auto shifted = std::make_unique<AstNode>(location, AST_SHIFT_RIGHT,
							std::make_unique<AstNode>(location, AST_TO_UNSIGNED, payload->clone()),
							mkconst_int(location, it->second, false));
					cat->children.push_back(std::make_unique<AstNode>(location, AST_CAST_SIZE,
							mkconst_int(location, it->first - it->second + 1, false), std::move(shifted)));
				}
			}
			if (cat->children.size() == 1)
				cat->children[0]->cloneInto(*this);
			else
				cat->cloneInto(*this);
			fixup_hierarchy_flags();
			recursion_counter--;
			return true;
		}
	}

	// a bare identifier naming a 1-D unpacked array (parameter or memory),
	// whose elements are the `inside` set members (IEEE 1800-2017 11.4.13)
	auto inside_unpacked_array = [&](AstNode *member) -> AstNode * {
		if (member->type != AST_IDENTIFIER || !member->children.empty())
			return nullptr;
		AstNode *decl = member->id2ast;
		if (!decl && current_scope.count(member->str))
			decl = current_scope.at(member->str);
		if (!decl)
			return nullptr;
		bool is_param = decl->type == AST_PARAMETER || decl->type == AST_LOCALPARAM;
		if (!(is_param || decl->type == AST_MEMORY) || decl->unpacked_dimensions != 1 ||
				decl->dimensions.empty() || decl->dimensions[0].range_width < 1 ||
				decl->dimensions[0].range_width > 4096)
			return nullptr;
		return decl;
	};

	// IEEE 1800-2009 11.4.13 set membership. The result is the OR of the
	// comparisons; a range whose left bound is greater than its right bound is empty.
	if (type == AST_SVA && str == "inside" && children.size() >= 2 && children[1] && children[1]->str == "set") {
		auto lhs = children[0]->clone();
		while (lhs->simplify(true, stage, -1, false)) { }
		std::unique_ptr<AstNode> acc = mkconst_int(location, 0, false);
		for (auto &member : children[1]->children) {
			if (!member)
				continue;
			std::unique_ptr<AstNode> term;
			if (member->type == AST_RANGE && member->children.size() >= 2) {
				auto lo = member->children[0]->clone();
				auto hi = member->children[1]->clone();
				while (lo->simplify(true, stage, -1, false)) { }
				while (hi->simplify(true, stage, -1, false)) { }
				if (lo->type == AST_CONSTANT && hi->type == AST_CONSTANT &&
						lo->bits_only_01() && hi->bits_only_01() && lo->asInt(false) > hi->asInt(false))
					term = mkconst_int(location, 0, false);
				else
					term = std::make_unique<AstNode>(location, AST_LOGIC_AND,
							std::make_unique<AstNode>(location, AST_GE, lhs->clone(), std::move(lo)),
							std::make_unique<AstNode>(location, AST_LE, lhs->clone(), std::move(hi)));
			} else if (AstNode *arr = inside_unpacked_array(member.get())) {
				// an unpacked array operand stands for its elements
				// (`x inside {VALID}` with `localparam t VALID [N]`)
				const auto &dim = arr->dimensions[0];
				term = mkconst_int(location, 0, false);
				for (int k = 0; k < dim.range_width; k++) {
					auto elem = member->clone();
					elem->children.push_back(std::make_unique<AstNode>(location, AST_RANGE,
							mkconst_int(location, dim.range_right + k, true)));
					while (elem->simplify(true, stage, -1, false)) { }
					term = std::make_unique<AstNode>(location, AST_LOGIC_OR, std::move(term),
							inside_eq(location, lhs.get(), std::move(elem)));
				}
			} else {
				auto item = member->clone();
				while (item->simplify(true, stage, -1, false)) { }
				term = inside_eq(location, lhs.get(), std::move(item));
			}
			acc = std::make_unique<AstNode>(location, AST_LOGIC_OR, std::move(acc), std::move(term));
		}
		acc->location = location;
		acc->cloneInto(*this);
		fixup_hierarchy_flags();
		recursion_counter--;
		return true;
	}

#if 0
	log("-------------\n");
	log("AST simplify[%d] depth %d at %s on %s %p:\n", stage, recursion_counter, location.to_string(), type2str(type), this);
	log("const_fold=%d, stage=%d, width_hint=%d, sign_hint=%d\n",
			int(const_fold), int(stage), int(width_hint), int(sign_hint));
	// dumpAst(nullptr, "> ");
#endif

		auto flatten_unpacked_ports = [&](bool include_resolved) {
		// Flatten unpacked array ports (logic [w-1:0] p [n]) into wide packed
		// wires before child simplification: ports cannot be memories in
		// RTLIL, and indexed accesses become part-selects on the wide wire.
		// include_resolved additionally catches ports whose typedef'd
		// element type has been resolved to an AST_MEMORY [packed, unpacked]
		// shape by the stage-1 simplify (typedef'd array ports).
		for (auto &child : children) {
			if (!child || child->port_id == 0 ||
					(child->type != AST_WIRE && !(include_resolved && child->type == AST_MEMORY))) {
				if (!include_resolved)
					break; // legacy semantics: ports lead the child list
				continue;
			}
			AstNode *port = child.get();
			if (port->children.size() < 2 || !port->children[1] || port->children[1]->type != AST_RANGE)
				continue;
			AstNode *packed = port->children[0].get();
			AstNode *unpacked = port->children[1].get();
			if (!packed || packed->type != AST_RANGE)
				continue;
			// C-style single-size unpacked dim `[n]` (literal or parameter)
			// means `[0:n-1]`; it never becomes a valid two-bound range on
			// its own, which used to make this rewrite silently skip (leaving
			// e.g. [33:0] o[2] as a 34-bit port instead of 68).
			// C-style single-size unpacked dim `[expr]` (literal or parameter).
			// Probe non-constant sizes on a clone: simplifying an unresolved
			// size expression (e.g. a struct-member of a parameter) mutates it
			// in place and breaks the legacy memory-dim path downstream.
			if (unpacked->type == AST_RANGE && unpacked->children.size() == 1) {
				int n = -1;
				if (unpacked->children[0]->type == AST_CONSTANT) {
					n = unpacked->children[0]->asInt(true);
				} else {
					auto probe = unpacked->children[0]->clone();
					while (probe->simplify(true, 1, -1, false)) { }
					if (probe->type == AST_CONSTANT)
						n = probe->asInt(true);
				}
				if (n < 1 || n > (1 << 20))
					continue;
				port->children[1] = make_range(unpacked->location, n - 1, 0);
				unpacked = port->children[1].get();
			}
			while (!packed->range_valid && packed->simplify(true, 1, -1, false)) { }
			while (!unpacked->range_valid && unpacked->simplify(true, 1, -1, false)) { }
			if (!packed->range_valid || !unpacked->range_valid)
				continue;
			int w = std::abs(packed->range_left - packed->range_right) + 1;
			int n = std::abs(unpacked->range_left - unpacked->range_right) + 1;
			// a one-element array port (`logic [7:0] p [1]`, OpenTitan's
			// `[NumShares]` with NumShares = 1) is flattened too: left as a
			// wire with its unpacked range, `p[0]` read bit 0 (silently)
			if (w < 1 || n < 1)
				continue;
			auto loc = packed->location;
			port->children.erase(port->children.begin() + 1);
			port->children[0] = make_range(loc, w * n - 1, 0);
			port->range_left = w * n - 1;
			port->range_right = 0;
			port->range_valid = true;
			port->range_swapped = false;
			port->dimensions.clear();
			port->unpacked_dimensions = 0;
			if (port->type == AST_MEMORY)
				port->type = AST_WIRE;
			rewrite_unpacked_port_refs(this, port->str, w, port);
		}
		// a port that stays a memory (zero-size or non-constant dimension,
		// e.g. a default configuration with zero requesters) is left a wire
		// with its ranges, as a plain unpacked port is; a memory port
		// crashed mem2reg
		if (include_resolved)
			for (auto &child : children)
				if (child && child->port_id != 0 && child->type == AST_MEMORY)
					child->type = AST_WIRE;
		};

	if (stage == 0)
	{
		log_assert(type == AST_MODULE || type == AST_INTERFACE);

		deep_recursion_warning = true;


		flatten_unpacked_ports(false);



		while (simplify(const_fold, 1, width_hint, sign_hint)) { }

		// typedef'd unpacked-array ports resolve to AST_MEMORY shapes during
		// the stage-1 simplify; flatten them now, before body elaboration.
		flatten_unpacked_ports(true);

		// Rewrite whole-array accesses to 1-D unpacked memories into indexed
		// element form (concatenations for reads/connections, split assigns
		// for writes) so downstream passes only ever see indexed accesses;
		// touched memories are forced into mem2reg. Runs after stage-1
		// simplification so identifiers are scope-bound and generate loops
		// are unrolled, but before the mem2reg scan below.
		if (!flag_nomem2reg && !get_bool_attribute(ID::nomem2reg))
			rewrite_whole_array_refs(this);

		if (!flag_nomem2reg && !get_bool_attribute(ID::nomem2reg))
		{
			dict<AstNode*, pool<std::string>> mem2reg_places;
			dict<AstNode*, uint32_t> mem2reg_candidates, dummy_proc_flags;
			uint32_t flags = flag_mem2reg ? AstNode::MEM2REG_FL_ALL : 0;
			mem2reg_as_needed_pass1(mem2reg_places, mem2reg_candidates, dummy_proc_flags, flags);

			pool<AstNode*> mem2reg_set;
			for (auto &it : mem2reg_candidates)
			{
				AstNode *mem = it.first;
				uint32_t memflags = it.second;
				bool this_nomeminit = flag_nomeminit;
				log_assert((memflags & ~0x00ffff00) == 0);

				if (mem->get_bool_attribute(ID::nomem2reg))
					continue;

				if (mem->children.size() >= 2 && mem->children[1] && mem->children[1]->type == AST_RANGE && mem->children[1]->range_valid) {
					int mem_size = mem->children[1]->range_left - mem->children[1]->range_right;
					if (mem_size < 0)
						mem_size = -mem_size;
					mem_size += 1;
					if (mem_size > 1024) {
						static bool warned = false;
						if (!warned) {
							warned = true;
							log_file_warning(mem->location.to_loc(), "Leaving memory `%s' (%d entries) as a memory.\n", mem->str.c_str(), mem_size);
						}
						continue;
					}
				}

				if (mem->get_bool_attribute(ID::nomeminit) || get_bool_attribute(ID::nomeminit))
					this_nomeminit = true;

				if (memflags & AstNode::MEM2REG_FL_FORCED)
					goto silent_activate;

				if (memflags & AstNode::MEM2REG_FL_EQ2)
					goto verbose_activate;

				if (memflags & AstNode::MEM2REG_FL_SET_ASYNC)
					goto verbose_activate;

				if ((memflags & AstNode::MEM2REG_FL_SET_INIT) && (memflags & AstNode::MEM2REG_FL_SET_ELSE) && this_nomeminit)
					goto verbose_activate;

				if (memflags & AstNode::MEM2REG_FL_CMPLX_LHS)
					goto verbose_activate;

				if ((memflags & AstNode::MEM2REG_FL_CONST_LHS) && !(memflags & AstNode::MEM2REG_FL_VAR_LHS))
					goto verbose_activate;

				// log("Note: Not replacing memory %s with list of registers (flags=0x%08lx).\n", mem->str, long(memflags));
				continue;

			verbose_activate:
				if (mem2reg_set.count(mem) == 0) {
					std::string message = stringf("Replacing memory %s with list of registers.", mem->str);
					bool first_element = true;
					for (auto &place : mem2reg_places[it.first]) {
						message += stringf("%s%s", first_element ? " See " : ", ", place);
						first_element = false;
					}
					log_warning("%s\n", message);
				}

			silent_activate:
				// log("Note: Replacing memory %s with list of registers (flags=0x%08lx).\n", mem->str, long(memflags));
				mem2reg_set.insert(mem);
			}

			for (auto node : mem2reg_set)
			{
				auto is_range = [](const std::unique_ptr<AstNode> &child) {
					return child && child->type == AST_RANGE;
				};
				if (node->children.empty() || !is_range(node->children[0]))
					node->children.insert(node->children.begin(), make_range(node->location, 0, 0));
				if (node->children.size() < 2 || !is_range(node->children[1]))
					node->children.insert(node->children.begin() + 1, make_range(node->location, 0, 0));
				if (!node->children[0]->range_valid) {
					node->children[0]->range_left = 0;
					node->children[0]->range_right = 0;
					node->children[0]->range_valid = true;
				}
				if (!node->children[1]->range_valid) {
					node->children[1]->range_left = 0;
					node->children[1]->range_right = 0;
					node->children[1]->range_valid = true;
				}

				int mem_width, mem_size, addr_bits;
				node->meminfo(mem_width, mem_size, addr_bits);

				int data_range_left = node->children[0]->range_left;
				int data_range_right = node->children[0]->range_right;

				if (node->children[0]->range_swapped)
					std::swap(data_range_left, data_range_right);

				auto loc = node->location;
				for (int i = 0; i < mem_size; i++) {
					auto reg = std::make_unique<AstNode>(loc, AST_WIRE, std::make_unique<AstNode>(loc, AST_RANGE,
							mkconst_int(loc, data_range_left, true), mkconst_int(loc, data_range_right, true)));
					reg->str = stringf("%s[%d]", node->str, i);
					reg->is_reg = true;
					reg->is_signed = node->is_signed;
					for (auto &it : node->attributes)
						if (it.first != ID::mem2reg)
							reg->set_attribute(it.first, it.second->clone());
					reg->location.begin.filename = node->location.begin.filename;
					reg->location = node->location;
					while (reg->simplify(true, 1, -1, false)) { }
					children.push_back(std::move(reg));
				}
			}

			AstNode* async_block = nullptr;
			while (mem2reg_as_needed_pass2(mem2reg_set, this, nullptr, async_block)) { }

			mem2reg_remove(mem2reg_set);
		}

		while (simplify(const_fold, 2, width_hint, sign_hint)) { }
		recursion_counter--;
		return false;
	}

	// we do not look inside a task or function
	// (but as soon as a task or function is instantiated we process the generated AST as usual)
	if (type == AST_FUNCTION || type == AST_TASK) {
		recursion_counter--;
		return false;
	}

	// deactivate all calls to non-synthesis system tasks
	// note that $display, $finish, and $stop are used for synthesis-time DRC so they're not in this list
	if ((type == AST_FCALL || type == AST_TCALL) && (str == "$strobe" || str == "$monitor" || str == "$time" ||
			str == "$dumpfile" || str == "$dumpvars" || str == "$dumpon" || str == "$dumpoff" || str == "$dumpall")) {
		log_file_warning(location.to_loc(), "Ignoring call to system %s %s.\n", type == AST_FCALL ? "function" : "task", str);
		delete_children();
		str = std::string();
	}

	if ((type == AST_TCALL) &&
		(str == "$display" || str == "$displayb" || str == "$displayh" || str == "$displayo" ||
		 str == "$write"   || str == "$writeb"   || str == "$writeh"   || str == "$writeo"))
	{
		if (!current_always) {
			log_file_warning(location.to_loc(), "System task `%s' outside initial or always block is unsupported.\n", str);
			delete_children();
			str = std::string();
		} else {
			// simplify the expressions and convert them to a special cell later in genrtlil
			for (auto& node : children)
				while (node->simplify(true, stage, -1, false)) {}

			if (current_always->type == AST_INITIAL && !flag_nodisplay && stage == 2) {
				int default_base = 10;
				if (str.back() == 'b')
					default_base = 2;
				else if (str.back() == 'o')
					default_base = 8;
				else if (str.back() == 'h')
					default_base = 16;

				// when $display()/$write() functions are used in an initial block, print them during synthesis
				Fmt fmt = processFormat(stage, /*sformat_like=*/false, default_base, /*first_arg_at=*/0, /*may_fail=*/true);
				if (str.substr(0, 8) == "$display")
					fmt.append_literal("\n");
				log("%s", fmt.render());
			}

			return false;
		}
	}


	if ((type == AST_TCALL) && (str == "$finish" || str == "$stop"))
	{
		if (!current_always) {
			log_file_warning(location.to_loc(), "System task `%s' outside initial or always block is unsupported.\n", str.c_str());
			delete_children();
			str = std::string();
		} else {
			// kept for genrtlil, which turns it into a $finish/$stop cell
			// consumed by the sim pass (ends the run when it fires)
			return false;
		}
	}

	// activate const folding if this is anything that must be evaluated statically (ranges, parameters, attributes, etc.)
	if (type == AST_WIRE || type == AST_PARAMETER || type == AST_LOCALPARAM || type == AST_ENUM_ITEM || type == AST_DEFPARAM || type == AST_PARASET || type == AST_RANGE || type == AST_PREFIX || type == AST_TYPEDEF)
		const_fold = true;
	if (type == AST_IDENTIFIER && current_scope.count(str) > 0 && (current_scope[str]->type == AST_PARAMETER || current_scope[str]->type == AST_LOCALPARAM || current_scope[str]->type == AST_ENUM_ITEM))
		const_fold = true;

	std::map<std::string, AstNode*> backup_scope;

	// create name resolution entries for all objects with names
	// also merge multiple declarations for the same wire (e.g. "output foobar; reg foobar;")
	if (type == AST_MODULE || type == AST_INTERFACE) {
		current_scope.clear();
		// names brought in by wildcard imports; local declarations shadow
		// these (IEEE 1800-2009 26.3), so population below may override them
		std::set<std::string> wildcard_scope_names;
		std::set<std::string> existing;
		int counter = 0;
		label_genblks(existing, counter);
		std::map<std::string, AstNode*> this_wire_scope;

		// Process package imports after clearing the scope but before processing module declarations
		for (size_t i = 0; i < children.size(); i++) {
			AstNode *child = children[i].get();
			if (!child)
				continue;
			if (child->type == AST_IMPORT) {
				// Find the package in the design
				AstNode *package_node = nullptr;

				// an import inside a package is prefixed with that package's
				// name like its other declarations (`\\devp::regp`); package
				// names have no `::`, so the imported package is the last part
				{
					size_t sep = child->str.rfind("::");
					if (sep != std::string::npos)
						child->str = "\\" + child->str.substr(sep + 2);
				}

				// First look in current_ast->children (for packages in same file)
				if (current_ast != nullptr) {
					for (auto &design_child : current_ast->children) {
						if (design_child->type == AST_PACKAGE) {
							if (design_child->str == child->str) {
								package_node = design_child.get();
								break;
							}
						}
					}
				}

				// If not found, look in design->verilog_packages (for packages from other files)
				if (!package_node && simplify_design_context != nullptr) {
					for (auto &design_package : simplify_design_context->verilog_packages) {
						// Handle both with and without leading backslash
						std::string package_name = design_package->str;
						if (package_name[0] == '\\') {
							package_name = package_name.substr(1);
						}
						if (package_name == child->str || design_package->str == child->str) {
							package_node = design_package.get();
							break;
						}
					}
				}

				if (package_node) {
					PackageImporter importer(this, child);
					// Import names from the package into current scope
					for (auto& pkg_child : package_node->children) {
						if (pkg_child)
							importer.import(current_scope, pkg_child.get(), &wildcard_scope_names);
					}
					// Keep the import node: the scope is cleared and rebuilt
					// on every simplify pass of this module, and later
					// passes (cell parameter overrides, generate bodies)
					// still need the imported names. Re-importing is
					// idempotent for wildcard imports (existing names win).
				} else {
					// If we can't find the package, just remove the import node to avoid errors later
					log_warning("Package `%s' not found for import, removing import statement\n", child->str);
					children.erase(children.begin() + i);
					i--; // Adjust index since we removed an element
				}
			}
		}


		flatten_unpacked_ports(false);

		for (size_t i = 0; i < children.size(); i++) {
			AstNode* node = children[i].get();
			if (!node)
				continue;

			if (node->type == AST_WIRE) {
				if (node->children.size() == 1 && node->children[0]->type == AST_RANGE) {
					for (auto& c : node->children[0]->children) {
						if (!c->is_simple_const_expr())
							set_attribute(ID::dynports, AstNode::mkconst_int(c->location, 1, true));
					}
				}
				if (this_wire_scope.count(node->str) > 0) {
					AstNode *first_node = this_wire_scope[node->str];
					if (first_node->is_input && node->is_reg)
						goto wires_are_incompatible;
					if (!node->is_input && !node->is_output && node->is_reg && node->children.size() == 0)
						goto wires_are_compatible;
					if (first_node->children.size() == 0 && node->children.size() == 1 && node->children[0]->type == AST_RANGE) {
						AstNode* r = node->children[0].get();
						if (r->range_valid && r->range_left == 0 && r->range_right == 0) {
							node->children.pop_back();
						}
					}
					if (first_node->children.size() != node->children.size())
						goto wires_are_incompatible;
					for (size_t j = 0; j < node->children.size(); j++) {
						auto &n1 = first_node->children[j], &n2 = node->children[j];
						if (n1->type == AST_RANGE && n2->type == AST_RANGE && n1->range_valid && n2->range_valid) {
							if (n1->range_left != n2->range_left)
								goto wires_are_incompatible;
							if (n1->range_right != n2->range_right)
								goto wires_are_incompatible;
						} else if (*n1 != *n2)
							goto wires_are_incompatible;
					}
					if (first_node->range_left != node->range_left)
						goto wires_are_incompatible;
					if (first_node->range_right != node->range_right)
						goto wires_are_incompatible;
					if (first_node->port_id == 0 && (node->is_input || node->is_output))
						goto wires_are_incompatible;
				wires_are_compatible:
					if (node->is_input)
						first_node->is_input = true;
					if (node->is_output)
						first_node->is_output = true;
					if (node->is_reg)
						first_node->is_reg = true;
					if (node->is_logic)
						first_node->is_logic = true;
					if (node->is_signed)
						first_node->is_signed = true;
					for (auto &it : node->attributes) {
						first_node->set_attribute(it.first, it.second->clone());
					}
					children.erase(children.begin()+(i--));
					did_something = true;
					continue;
				wires_are_incompatible:
					if (stage > 1)
						input_error("Incompatible re-declaration of wire %s.\n", node->str);
					continue;
				}
				this_wire_scope[node->str] = node;
			}
			// these nodes appear at the top level in a module and can define names
			if (node->type == AST_PARAMETER || node->type == AST_LOCALPARAM || node->type == AST_WIRE || node->type == AST_AUTOWIRE || node->type == AST_GENVAR ||
					node->type == AST_MEMORY || node->type == AST_FUNCTION || node->type == AST_TASK || node->type == AST_DPI_FUNCTION || node->type == AST_CELL ||
					node->type == AST_TYPEDEF) {
				backup_scope[node->str] = current_scope[node->str];
				current_scope[node->str] = node;
			}
			if (node->type == AST_ENUM) {
				current_scope[node->str] = node;
				for (auto& enode : node->children) {
					if (!enode)
						continue;
					log_assert(enode->type==AST_ENUM_ITEM);
					// Tag the item with its owner enum so that a repeated
					// registration of the SAME enum (the compilation-unit
					// globals are clones of the $sv_unit module children,
					// see AstNode::process) stays idempotent instead of
					// looking like a duplicate declaration.
					enode->attributes[ID::enum_reg_owner] = AstNode::mkconst_str(enode->location, node->str);
					if (current_scope.count(enode->str) == 0)
						current_scope[enode->str] = enode.get();
					else if (current_scope[enode->str]->type == AST_ENUM_ITEM &&
							current_scope[enode->str]->attributes.count(ID::enum_reg_owner) &&
							current_scope[enode->str]->attributes[ID::enum_reg_owner]->str == node->str)
						; // same enum, already registered
					else if (wildcard_scope_names.count(enode->str))
						// local declaration shadows a wildcard-imported name
						current_scope[enode->str] = enode.get();
					else
						input_error("enum item %s already exists\n", enode->str);
				}
			}
		}
		for (size_t i = 0; i < children.size(); i++) {
			AstNode *node = children[i].get();
			if (!node)
				continue;
			if (node->type == AST_PARAMETER || node->type == AST_LOCALPARAM || node->type == AST_WIRE || node->type == AST_AUTOWIRE || node->type == AST_MEMORY || node->type == AST_TYPEDEF)
				while (i < children.size() && children[i] && children[i]->simplify(true, 1, -1, false))
					did_something = true;
			node = i < children.size() ? children[i].get() : nullptr;
			if (node && node->type == AST_ENUM) {
				for (auto& enode : node->children){
					if (!enode)
						continue;
					log_assert(enode->type==AST_ENUM_ITEM);
					while (i < children.size() && children[i] && children[i]->simplify(true, 1, -1, false))
						did_something = true;
				}
			}
		}

		for (auto& child : children)
			if (child && child->type == AST_ALWAYS &&
					child->attributes.count(ID::always_comb))
				check_auto_nosync(child.get());
	}

	// create name resolution entries for all objects with names
	if (type == AST_PACKAGE) {
		//add names to package scope
		for (size_t i = 0; i < children.size(); i++) {
			auto& node = children[i];
			if (!node)
				continue;
			// these nodes appear at the top level in a package and can define names
			if (node->type == AST_PARAMETER || node->type == AST_LOCALPARAM || node->type == AST_TYPEDEF || node->type == AST_FUNCTION || node->type == AST_TASK) {
				current_scope[node->str] = node.get();
			}
			if (node->type == AST_ENUM) {
				current_scope[node->str] = node.get();
				for (auto& enode : node->children) {
					log_assert(enode->type==AST_ENUM_ITEM);
					enode->attributes[ID::enum_reg_owner] = AstNode::mkconst_str(enode->location, node->str);
					if (current_scope.count(enode->str) == 0)
						current_scope[enode->str] = enode.get();
					else if (current_scope[enode->str]->type == AST_ENUM_ITEM &&
							current_scope[enode->str]->attributes.count(ID::enum_reg_owner) &&
							current_scope[enode->str]->attributes[ID::enum_reg_owner]->str == node->str)
						; // same enum, already registered
					else
						input_error("enum item %s already exists in package\n", enode->str);
				}
			}
		}
	}


	auto backup_current_block = current_block;
	auto backup_current_block_child = current_block_child;
	auto backup_current_top_block = current_top_block;
	auto backup_current_always = current_always;
	auto backup_current_always_clocked = current_always_clocked;

	if (type == AST_ALWAYS || type == AST_INITIAL)
	{
		if (current_always != nullptr) {
			// A function-local initializer inlined into an always is left in place.
			if (type != AST_INITIAL)
				input_error("Invalid nesting of always blocks and/or initializations.\n");
		} else {
			current_always = this;
			current_always_clocked = false;

			if (type == AST_ALWAYS)
				for (auto& child : children) {
					if ((child->type == AST_POSEDGE || child->type == AST_NEGEDGE) && child->children.size() == 2) {
						// @(posedge clk iff en): the body runs only when en is true.
						auto guard = std::move(child->children[1]);
						child->children.pop_back();
						for (auto &body : children) {
							if (body->type != AST_BLOCK)
								continue;
							auto then_blk = std::make_unique<AstNode>(location, AST_BLOCK);
							then_blk->children.swap(body->children);
							auto cas = std::make_unique<AstNode>(location, AST_CASE);
							cas->children.push_back(std::make_unique<AstNode>(location, AST_REDUCE_BOOL, std::move(guard)));
							cas->children.push_back(std::make_unique<AstNode>(location, AST_COND,
									AstNode::mkconst_int(location, 1, false, 1), std::move(then_blk)));
							body->children.push_back(std::move(cas));
							break;
						}
					}
					if (child->type == AST_POSEDGE || child->type == AST_NEGEDGE)
						current_always_clocked = true;
					if (child->type == AST_EDGE && GetSize(child->children) == 1 &&
							child->children[0]->type == AST_IDENTIFIER && child->children[0]->str == "\\$global_clock")
						current_always_clocked = true;
				}
		}
	}

	// IEEE 1800-2009 17.3: a checker instantiation is expanded inline at the
	// AST level, the way sva2check's inline_decl inlines named sequence/
	// property instances at the RTLIL level: the actual argument expression
	// textually replaces the formal everywhere in the cloned body. AST-level
	// inlining is used (instead of an RTLIL route) so checker-local
	// declarations can be renamed per instance and the body composes with
	// generate blocks like any other module items.
	if (type == AST_CELL && GetSize(children) >= 1 && children[0]->type == AST_CELLTYPE && !children[0]->str.empty()) {
		AstNode *decl = nullptr;
		auto find_checker = [&](AstNode *scope) {
			if (!scope || decl)
				return;
			for (auto &ch : scope->children) {
				if (!ch)
					continue;
				if (ch->type == AST_SVA && ch->str == "checker_decl" && GetSize(ch->children) >= 2 &&
						ch->children[0]->type == AST_CELLTYPE && ch->children[0]->str == children[0]->str)
					decl = ch.get();
				if (ch->type == AST_PACKAGE) {
					for (auto &inner : ch->children)
						if (inner->type == AST_SVA && inner->str == "checker_decl" && GetSize(inner->children) >= 2 &&
								inner->children[0]->type == AST_CELLTYPE && inner->children[0]->str == children[0]->str)
							decl = inner.get();
				}
			}
		};
		find_checker(current_ast_mod);
		find_checker(current_ast);

		if (!decl) {
			// A cell that came out of a checker body may only instantiate
			// another checker; anything else is a module instance, which
			// IEEE 1800-2009 17.2 does not allow there.
			if (get_bool_attribute(ID(sv_in_checker)))
				input_error("Checker instantiation of `%s' not found (module instantiations are not allowed inside checker bodies).\n",
						children[0]->str.c_str());
		} else {
			int inline_depth = attributes.count(ID(checker_inline_depth)) ? attributes[ID(checker_inline_depth)]->integer : 0;
			if (inline_depth >= 64)
				log_error("Recursive checker instantiation involving `%s' (depth limit 64).\n", children[0]->str.c_str());
			AstNode *decl_body = decl->children[1].get();
			log_assert(decl_body->type == AST_GENBLOCK);

			// checker formals are the AST_IDENTIFIER children after the
			// body; an optional default expression is their first child.
			// The trailing AST_SVA markers carry the checker's effective
			// default clocking / disable iff.
			std::vector<AstNode *> formals;
			AstNode *defclock = nullptr, *defdisable = nullptr;
			for (size_t i = 2; i < decl->children.size(); i++) {
				AstNode *c = decl->children[i].get();
				if (c->type == AST_IDENTIFIER)
					formals.push_back(c);
				else if (c->type == AST_SVA && c->str == "checker_default_clock" && GetSize(c->children) == 1)
					defclock = c->children[0].get();
				else if (c->type == AST_SVA && c->str == "checker_default_disable" && GetSize(c->children) == 1)
					defdisable = c->children[0].get();
			}

			// Bind actuals: positional arguments fill formals in order,
			// named arguments by name, `.*` picks up the same-named signal
			// from the instantiation scope, and remaining formals fall back
			// to their default expression.
			dict<std::string, AstNode *> actuals;
			std::vector<AstNode *> positional;
			for (size_t i = 1; i < children.size(); i++) {
				AstNode *c = children[i].get();
				if (c->type == AST_PARASET)
					input_error("Checker instance `%s' of `%s' takes no parameters (checkers are not parametric).\n",
							str.c_str(), children[0]->str.c_str());
				if (c->type != AST_ARGUMENT)
					continue;
				if (c->str.empty()) {
					if (c->children.empty())
						input_error("Checker instance `%s' has an empty positional port connection.\n", str.c_str());
					positional.push_back(c->children[0].get());
				} else {
					AstNode *match = nullptr;
					for (auto f : formals)
						if (f->str == c->str)
							match = f;
					if (!match)
						input_error("Checker instance `%s' connects unknown port `%s'.\n", str.c_str(), c->str.c_str());
					// an explicitly empty named connection `.x()' leaves
					// the port to its default, like an omitted port
					if (!c->children.empty())
						actuals[match->str] = c->children[0].get();
				}
			}
			std::vector<std::unique_ptr<AstNode>> owned_actuals;
			size_t posi = 0;
			for (auto f : formals) {
				if (actuals.count(f->str))
					continue;
				if (posi < positional.size()) {
					actuals[f->str] = positional[posi++];
					continue;
				}
				if (get_bool_attribute(ID::wildcard_port_conns) && current_scope.count(f->str)) {
					auto id = std::make_unique<AstNode>(location, AST_IDENTIFIER);
					id->str = f->str;
					owned_actuals.push_back(std::move(id));
					actuals[f->str] = owned_actuals.back().get();
					continue;
				}
				if (!f->children.empty()) {
					actuals[f->str] = f->children[0].get();
					continue;
				}
				input_error("Checker port `%s' of `%s' is not connected and has no default.\n",
						f->str.c_str() + 1, children[0]->str.c_str() + 1);
			}

			auto body = decl_body->clone();
			std::string pfx = str + "$";

			// Pass 1: collect the checker's local declaration names (data
			// objects, functions, sequence/property/let/nested checker
			// declarations, named genblocks) so the clone can be renamed
			// without colliding across instances of the same checker.
			dict<std::string, std::string> rename;
			std::function<void(AstNode *)> collect_names = [&](AstNode *node) {
				for (auto &ch : node->children) {
					AstNode *c = ch.get();
					if (!c)
						continue;
					switch (c->type) {
					case AST_WIRE:
					case AST_MEMORY:
					case AST_PARAMETER:
					case AST_LOCALPARAM:
					case AST_TYPEDEF:
					case AST_FUNCTION:
					case AST_TASK:
					case AST_GENVAR:
					case AST_GENBLOCK:
						if (!c->str.empty())
							rename[c->str] = pfx + (c->str.front() == '\\' ? c->str.substr(1) : c->str);
						break;
					case AST_ENUM:
						if (!c->str.empty())
							rename[c->str] = pfx + (c->str.front() == '\\' ? c->str.substr(1) : c->str);
						for (auto &item : c->children)
							if (item->type == AST_ENUM_ITEM && !item->str.empty())
								rename[item->str] = pfx + (item->str.front() == '\\' ? item->str.substr(1) : item->str);
						break;
					case AST_SVA:
						if ((c->str == "sequence_decl" || c->str == "property_decl" || c->str == "let_decl" ||
									c->str == "checker_decl") && GetSize(c->children) >= 1 && !c->children[0]->str.empty())
							rename[c->children[0]->str] = pfx + (c->children[0]->str.front() == '\\' ? c->children[0]->str.substr(1) : c->children[0]->str);
						break;
					default:
						break;
					}
					collect_names(c);
				}
			};
			collect_names(body.get());
			// assertion labels of a checker instance are copied per
			// instance: a label clash with another instance (or with the
			// first copy) is instance expansion, not a degradation, so
			// genrtlil renames these silently (the battery's tchk* files
			// read them as <label>$N)
			std::function<void(AstNode *)> mark_asserts = [&](AstNode *node) {
				for (auto &ch : node->children) {
					if (!ch)
						continue;
					if (ch->type == AST_ASSERT || ch->type == AST_ASSUME || ch->type == AST_LIVE ||
							ch->type == AST_FAIR || ch->type == AST_COVER)
						ch->attributes[IdString("\\checker_instance")] = mkconst_int(ch->location, 1, false);
					mark_asserts(ch.get());
				}
			};
			mark_asserts(body.get());

			// Pass 2: rename declarations and their references in the
			// clone. Actual argument expressions are spliced in pass 3, so
			// they are never touched by this renaming and keep resolving in
			// the instantiation scope (the inline_decl semantics).
			std::function<void(AstNode *)> rename_node = [&](AstNode *node) {
				for (auto &ch : node->children) {
					AstNode *c = ch.get();
					if (!c)
						continue;
					switch (c->type) {
					case AST_WIRE:
					case AST_MEMORY:
					case AST_PARAMETER:
					case AST_LOCALPARAM:
					case AST_TYPEDEF:
					case AST_TASK:
					case AST_GENVAR:
					case AST_GENBLOCK:
						if (rename.count(c->str))
							c->str = rename.at(c->str);
						break;
					case AST_FUNCTION:
						if (rename.count(c->str)) {
							std::string old = c->str;
							c->str = rename.at(c->str);
							c->replace_result_wire_name_in_function(old, c->str);
						}
						break;
					case AST_ENUM:
						if (rename.count(c->str))
							c->str = rename.at(c->str);
						for (auto &item : c->children)
							if (item->type == AST_ENUM_ITEM && rename.count(item->str))
								item->str = rename.at(item->str);
						break;
					case AST_IDENTIFIER:
					case AST_FCALL:
					case AST_TCALL:
					case AST_WIRETYPE:
						if (rename.count(c->str))
							c->str = rename.at(c->str);
						break;
					case AST_SVA:
						if ((c->str == "sequence_decl" || c->str == "property_decl" || c->str == "let_decl" ||
									c->str == "checker_decl") && GetSize(c->children) >= 1 && rename.count(c->children[0]->str))
							c->children[0]->str = rename.at(c->children[0]->str);
						break;
					case AST_CELL:
						// nested checker instance: unique instance name for
						// the next expansion round, and its type must follow
						// a renamed nested checker declaration. Instance names
						// are not in the rename map, so prefix them directly.
						if (!c->str.empty()) {
							if (rename.count(c->str))
								c->str = rename.at(c->str);
							else
								c->str = pfx + (c->str.front() == '\\' ? c->str.substr(1) : c->str);
						}
						if (GetSize(c->children) >= 1 && c->children[0]->type == AST_CELLTYPE && rename.count(c->children[0]->str))
							c->children[0]->str = rename.at(c->children[0]->str);
						c->set_attribute(ID(checker_inline_depth), AstNode::mkconst_int(c->location, inline_depth + 1, false));
						break;
					case AST_ASSERT:
					case AST_ASSUME:
					case AST_LIVE:
					case AST_FAIR:
					case AST_COVER:
						// Labels keep their source name so property verdicts
						// stay keyed by the label the user wrote; genrtlil's
						// duplicate-name fallback uniquifies later instances.
						break;
					default:
						break;
					}
					rename_node(c);
				}
			};
			rename_node(body.get());

			// Pass 3: substitute the formals with the actual expressions.
			// Identifiers renamed in pass 2 are checker-local references
			// and never formal references, so plain name matching is enough.
			std::function<void(AstNode *)> subst_formals = [&](AstNode *node) {
				for (auto &ch : node->children) {
					AstNode *c = ch.get();
					if (!c)
						continue;
					if (c->type == AST_IDENTIFIER && actuals.count(c->str)) {
						actuals.at(c->str)->clone()->cloneInto(*c);
						continue;
					}
					subst_formals(c);
				}
			};
			subst_formals(body.get());
			fixup_hierarchy_flags();

			// Pass 4: the checker's own `default clocking` / `default
			// disable iff` applies to concurrent assertions without an
			// explicit clocking event (assertions parsed before the
			// declaration were not covered by the parser state).
			if (defclock || defdisable) {
				std::function<void(AstNode *)> apply_clock = [&](AstNode *node) {
					for (auto &ch : node->children) {
						AstNode *c = ch.get();
						if (!c)
							continue;
						if ((c->type == AST_ASSERT || c->type == AST_ASSUME || c->type == AST_COVER ||
								c->type == AST_LIVE || c->type == AST_FAIR) &&
								GetSize(c->children) >= 1 && c->get_bool_attribute(ID(sv_concurrent))) {
							AstNode *spec = c->children[0].get();
							if (spec->type == AST_SVA && spec->str == "spec") {
								if (defclock && GetSize(spec->children) >= 1 && spec->children[0]->type == AST_SVA &&
										spec->children[0]->str == "no_clock")
									defclock->clone()->cloneInto(*(spec->children[0]));
								if (defdisable && GetSize(spec->children) >= 2 && spec->children[1]->type == AST_SVA &&
										spec->children[1]->str == "no_disable")
									defdisable->clone()->cloneInto(*(spec->children[1]));
							} else {
								// no spec node at all: wrap the body in one
								auto wrapped = std::make_unique<AstNode>(location, AST_SVA);
								wrapped->str = "spec";
								wrapped->children.push_back(defclock ? defclock->clone() : std::make_unique<AstNode>(location, AST_SVA));
								if (!defclock)
									wrapped->children[0]->str = "no_clock";
								wrapped->children.push_back(defdisable ? defdisable->clone() : std::make_unique<AstNode>(location, AST_SVA));
								if (!defdisable)
									wrapped->children[1]->str = "no_disable";
								wrapped->children.push_back(c->children[0]->clone());
								wrapped->cloneInto(*(c->children[0]));
							}
						}
						apply_clock(c);
					}
				};
				apply_clock(body.get());
			}

			// Flatten the elaborated body into the enclosing module, like
			// the unconditional-generate-block case does, and consume the
			// instance cell. The body's own declarations are registered in
			// the scope as they are flattened: the module's registration
			// pass only runs at the start of a simplify call, so a sibling
			// (e.g. an assertion calling a checker-local function) would
			// otherwise miss them within this round.
			for (auto &ch : body->children) {
				if (!ch)
					continue;
				switch (ch->type) {
				case AST_WIRE:
				case AST_MEMORY:
				case AST_PARAMETER:
				case AST_LOCALPARAM:
				case AST_FUNCTION:
				case AST_TASK:
				case AST_TYPEDEF:
				case AST_GENVAR:
					current_scope[ch->str] = ch.get();
					break;
				case AST_ENUM:
					current_scope[ch->str] = ch.get();
					for (auto &item : ch->children)
						if (item->type == AST_ENUM_ITEM)
							current_scope[item->str] = item.get();
					break;
				default:
					break;
				}
				ch->simplify(const_fold, stage, -1, false);
				current_ast_mod->children.push_back(std::move(ch));
			}
			body->children.clear();

			auto none = std::make_unique<AstNode>(location, AST_NONE);
			none->cloneInto(*this);
			fixup_hierarchy_flags();
			recursion_counter--;
			return true;
		}
	}

	if (type == AST_CELL) {
		bool lookup_suggested = false;

		// an assignment-pattern override takes the overridden parameter's
		// struct type: materialize it as a typed localparam would be (it
		// used to fold to nothing and the derived module got cfg=32'0)
		{
			std::string ctype;
			for (auto &child : children)
				if (child && child->type == AST_CELLTYPE)
					ctype = child->str;
			int pos = 0;
			for (auto &child : children) {
				if (!child || child->type != AST_PARASET)
					continue;
				int my_pos = child->str.empty() ? pos++ : -1;
				// an unpacked-array parameter passed through by name
				// (`.PMPRstCfg(PMPRstCfg)`, ibex): carry its value as one
				// flat constant, element 0 most significant; struct
				// elements are materialized with the array's element type
				// (it used to be overridden with 0)
				if (!child->children.empty() && child->children[0]->type == AST_IDENTIFIER &&
						child->children[0]->children.empty()) {
					AstNode *src = child->children[0]->id2ast ? child->children[0]->id2ast :
							current_scope.count(child->children[0]->str) ? current_scope.at(child->children[0]->str) : nullptr;
					if (src && (src->type == AST_PARAMETER || src->type == AST_LOCALPARAM) &&
							src->unpacked_dimensions > 0 && !src->children.empty()) {
						// a default naming another array parameter: take its value
						if (src->children[0]->type == AST_IDENTIFIER && src->children[0]->id2ast &&
								(src->children[0]->id2ast->type == AST_PARAMETER || src->children[0]->id2ast->type == AST_LOCALPARAM) &&
								!src->children[0]->id2ast->children.empty())
							src->children[0] = src->children[0]->id2ast->children[0]->clone();
						if (src->children[0]->type == AST_CONSTANT) {
							child->children[0] = src->children[0]->clone();
							did_something = true;
							continue;
						}
						if (src->children[0]->type == AST_ASSIGN_PATTERN) {
							AstNode *etype = src->attributes.count(ID::wiretype) ? src->attributes.at(ID::wiretype).get() : nullptr;
							// an array's wiretype attribute is the element type's NAME
							if (etype && etype->type != AST_STRUCT && etype->type != AST_UNION) {
								std::string tn = etype->str;
								etype = nullptr;
								AstNode *td = current_scope.count(tn) ? current_scope.at(tn) : nullptr;
								if (!td && current_ast_mod)
									for (auto &c : current_ast_mod->children)
										if (c && c->type == AST_TYPEDEF && c->str == tn)
											td = c.get();
								if (td && td->type == AST_TYPEDEF && !td->children.empty())
									td = td->children[0].get();
								if (td && (td->type == AST_STRUCT || td->type == AST_UNION))
									etype = td;
							}
							int ew = etype ? etype->range_left - etype->range_right + 1
									: (src->range_valid ? std::abs(src->range_left - src->range_right) + 1 : -1);
							std::vector<RTLIL::State> flat;   // LSB first; built back to front
							std::vector<std::vector<RTLIL::State>> elems;
							bool ok = ew > 0;
							std::function<void(AstNode*, int)> walk = [&](AstNode *n, int d) {
								if (!ok)
									return;
								if (d < src->unpacked_dimensions) {
									if (n->type != AST_ASSIGN_PATTERN) { ok = false; return; }
									for (auto &c : n->children)
										walk(c.get(), d + 1);
									return;
								}
								std::unique_ptr<AstNode> leaf = n->clone();
								if (leaf->type == AST_ASSIGN_PATTERN && etype) {
									auto tmp = std::make_unique<AstNode>(n->location, AST_LOCALPARAM, std::move(leaf));
									tmp->str = "\\$override_element";
									auto st = etype->clone();
									size_packed_struct(st.get(), 0);
									tmp->set_attribute(ID::wiretype, std::move(st));
									while (tmp->simplify(true, 1, -1, false)) { }
									leaf = tmp->children[0]->clone();
								} else {
									while (leaf->simplify(true, 1, -1, false)) { }
								}
								if (leaf->type != AST_CONSTANT) { ok = false; return; }
								elems.push_back(leaf->bitsAsConst(ew, leaf->is_signed).to_bits());
							};
							walk(src->children[0].get(), 0);
							if (!ok || elems.empty())
								input_error("Unpacked-array parameter %s passed to `%s' did not fold to constant elements.\n",
										log_id(src->str), log_id(ctype));
							for (int i = GetSize(elems) - 1; i >= 0; i--)
								flat.insert(flat.end(), elems[i].begin(), elems[i].end());
							child->children[0] = mkconst_bits(child->location, flat, false);
							did_something = true;
							continue;
						}
					}
				}
				if (child->children.empty() || child->children[0]->type != AST_ASSIGN_PATTERN)
					continue;
				auto stype = override_struct_type(ctype, child->str, my_pos);
				if (!stype)
					input_error("Assignment pattern as the value of parameter %s of `%s' needs the parameter's struct type, "
							"which could not be resolved.\n",
							child->str.empty() ? stringf("#%d", my_pos + 1).c_str() : log_id(child->str), log_id(ctype));
				size_packed_struct(stype.get(), 0);
				auto tmp = std::make_unique<AstNode>(child->location, AST_LOCALPARAM, child->children[0]->clone());
				tmp->str = "\\$override_pattern";
				tmp->set_attribute(ID::wiretype, std::move(stype));
				while (tmp->simplify(true, 1, -1, false)) { }
				if (tmp->children.empty() || tmp->children[0]->type != AST_CONSTANT)
					input_error("Assignment pattern as the value of parameter %s of `%s' did not fold to a constant.\n",
							child->str.empty() ? stringf("#%d", my_pos + 1).c_str() : log_id(child->str), log_id(ctype));
				child->children[0] = tmp->children[0]->clone();
				did_something = true;
			}
		}

		for (auto& child : children) {
			// simplify any parameters to constants
			if (child->type == AST_PARASET)
				while (child->simplify(true, 1, -1, false)) { }

			// look for patterns which _may_ indicate ambiguity requiring
			// resolution of the underlying module
			if (child->type == AST_ARGUMENT) {
				if (child->children.size() != 1)
					continue;
				const auto& value = child->children[0];
				if (value->type == AST_IDENTIFIER) {
					const AstNode *elem = value->id2ast;
					if (elem == nullptr) {
						if (current_scope.count(value->str))
							elem = current_scope.at(value->str);
						else
							continue;
					}
					if (elem->type == AST_MEMORY)
						// need to determine is the is a read or wire
						lookup_suggested = true;
					else if (elem->type == AST_WIRE && elem->is_signed && !value->children.empty())
						// this may be a fully sliced signed wire which needs
						// to be indirected to produce an unsigned connection
						lookup_suggested = true;
				}
				else if (contains_unbased_unsized(value.get()))
					// unbased unsized literals extend to width of the context
					lookup_suggested = true;
			}
		}

		const RTLIL::Module *module = nullptr;
		if (lookup_suggested)
			module = lookup_cell_module();
		if (module) {
			size_t port_counter = 0;
			for (auto& child : children) {
				if (child->type != AST_ARGUMENT)
					continue;

				// determine the full name of port this argument is connected to
				RTLIL::IdString port_name;
				if (child->str.size())
					port_name = child->str;
				else {
					if (port_counter >= module->ports.size())
						input_error("Cell instance has more ports than the module!\n");
					port_name = module->ports[port_counter++];
				}

				// find the port's wire in the underlying module
				const RTLIL::Wire *ref = module->wire(port_name);
				if (ref == nullptr)
					input_error("Cell instance refers to port %s which does not exist in module %s!.\n",
							port_name.unescape(), module->name.unescape());

				// select the argument, if present
				log_assert(child->children.size() <= 1);
				if (child->children.empty())
					continue;

				{
					auto arg_check = child->children[0].get();

					// plain identifiers never need indirection; this also prevents
					// adding infinite levels of indirection
					if (arg_check->type == AST_IDENTIFIER && arg_check->children.empty())
						continue;

					// only add indirection for standard inputs or outputs
					if (ref->port_input == ref->port_output)
						continue;
				}

				auto arg = std::move(child->children[0]);
				did_something = true;

				// create the indirection wire
				std::stringstream sstr;
				sstr << "$indirect$" << ref->name.c_str() << "$" << RTLIL::encode_filename(*location.begin.filename) << ":" << location.begin.line << "$" << (autoidx++);
				std::string tmp_str = sstr.str();
				add_wire_for_ref(location, ref, tmp_str);

				auto asgn_owned = std::make_unique<AstNode>(child->location, AST_ASSIGN);
				auto* asgn = asgn_owned.get();
				current_ast_mod->children.push_back(std::move(asgn_owned));

				auto ident = std::make_unique<AstNode>(child->location, AST_IDENTIFIER);
				ident->str = tmp_str;
				child->children[0] = ident->clone();

				if (ref->port_input && !ref->port_output) {
					asgn->children.push_back(std::move(ident));
					asgn->children.push_back(std::move(arg));
				} else {
					log_assert(!ref->port_input && ref->port_output);
					asgn->children.push_back(std::move(arg));
					asgn->children.push_back(std::move(ident));
				}
				asgn->fixup_hierarchy_flags();
			}


		}
	}

	int backup_width_hint = width_hint;
	bool backup_sign_hint = sign_hint;

	bool detect_width_simple = false;
	bool child_0_is_self_determined = false;
	bool child_1_is_self_determined = false;
	bool child_2_is_self_determined = false;
	bool children_are_self_determined = false;
	bool reset_width_after_children = false;

	switch (type)
	{
	case AST_ASSIGN_EQ:
	case AST_ASSIGN_LE:
	case AST_ASSIGN:
		if (!children[0] || !children[1])
			break;
		while (children[0] && !children[0]->basic_prep && children[0]->simplify(false, stage, -1, false) == true)
			did_something = true;
		while (children[1] && !children[1]->basic_prep && children[1]->simplify(false, stage, -1, false) == true)
			did_something = true;
		children[0]->detectSignWidth(backup_width_hint, backup_sign_hint);
		children[1]->detectSignWidth(width_hint, sign_hint);
		width_hint = max(width_hint, backup_width_hint);
		child_0_is_self_determined = true;
		// test only once, before optimizations and memory mappings but after assignment LHS was mapped to an identifier
		if (children[0]->id2ast && !children[0]->was_checked) {
			if ((type == AST_ASSIGN_LE || type == AST_ASSIGN_EQ) && children[0]->id2ast->is_logic)
				children[0]->id2ast->is_reg = true; // if logic type is used in a block asignment
			if ((type == AST_ASSIGN_LE || type == AST_ASSIGN_EQ) && !children[0]->id2ast->is_reg)
				log_warning("wire '%s' is assigned in a block at %s.\n", children[0]->str, loc_string());
			if (type == AST_ASSIGN && children[0]->id2ast->is_reg) {
				bool is_rand_reg = false;
				if (children[1]->type == AST_FCALL) {
					if (children[1]->str == "\\$anyconst")
						is_rand_reg = true;
					if (children[1]->str == "\\$anyseq")
						is_rand_reg = true;
					if (children[1]->str == "\\$allconst")
						is_rand_reg = true;
					if (children[1]->str == "\\$allseq")
						is_rand_reg = true;
				}
				if (!is_rand_reg)
					log_warning("reg '%s' is assigned in a continuous assignment at %s.\n", children[0]->str, loc_string());
			}
			children[0]->was_checked = true;
		}
		break;

	case AST_STRUCT:
	case AST_UNION:
		if (!basic_prep) {
			for (auto& node : children) {
				// resolve any ranges
				while (!node->basic_prep && node->simplify(true, stage, -1, false)) {
					did_something = true;
				}
			}
			// determine member offsets and widths
			size_packed_struct(this, 0);

			// instance rather than just a type in a typedef or outer struct?
			if (!str.empty() && str[0] == '\\') {
				// instance so add a wire for the packed structure
				auto wnode = make_packed_struct(this, str, attributes);
				log_assert(current_ast_mod);
				current_ast_mod->children.push_back(std::move(wnode));
			}
			basic_prep = true;
			is_custom_type = false;
		}
		break;

	case AST_STRUCT_ITEM:
		if (is_custom_type) {
			log_assert(children.size() >= 1);
			log_assert(children[0]->type == AST_WIRETYPE);

			// Pretend it's just a wire in order to resolve the type.
			type = AST_WIRE;
			while (is_custom_type && simplify(const_fold, stage, width_hint, sign_hint)) {};
			if (type == AST_WIRE)
				type = AST_STRUCT_ITEM;

			did_something = true;
		}
		log_assert(!is_custom_type);
		break;

	case AST_ENUM:
		//log("\nENUM %s: %d child %d\n", str, basic_prep, children[0]->basic_prep);
		if (!basic_prep) {
			for (auto& item_node : children) {
				while (!item_node->basic_prep && item_node->simplify(false, stage, -1, false))
					did_something = true;
			}
			// allocate values (called more than once)
			allocateDefaultEnumValues();
		}
		break;

	case AST_PARAMETER:
	case AST_LOCALPARAM:
		// if parameter is implicit type which is the typename of a struct or union,
		// save information about struct in wiretype attribute
		if (children[0]->type == AST_IDENTIFIER && current_scope.count(children[0]->str) > 0) {
			auto item_node = current_scope[children[0]->str];
			if (item_node->type == AST_STRUCT || item_node->type == AST_UNION) {
				set_attribute(ID::wiretype, item_node->clone());
				size_packed_struct(attributes[ID::wiretype].get(), 0);
				add_members_to_scope(attributes[ID::wiretype].get(), str);
			}
		}
		{
		// custom-typed form (parameter cfg_t x = '{...}): addWiretypeNode
			// records the type name in an AST_WIRETYPE child; register the
			// struct members the same way so member selects (cfg.ID_WIDTH)
			// resolve and fold against the parameter's constant value
			// (a wiretype attribute that is only the type NAME -- set for a
			// compilation-unit typedef -- is not a member map: resolve it too)
			if (attributes.count(ID::wiretype) == 0 ||
					(attributes.at(ID::wiretype)->type != AST_STRUCT && attributes.at(ID::wiretype)->type != AST_UNION))
				for (auto &child : children)
					if (child->type == AST_WIRETYPE) {
						std::string tname = child->str;
						// a compilation-unit typedef is appended to the
						// module's children AFTER its declarations, so it is
						// not in scope yet when an early parameter is
						// simplified: a named pattern then had no member map
						// and folded to 0 (`typedef struct ... c_t;` at file
						// scope, `localparam c_t P = '{A:5, B:9}`)
						if (current_scope.count(tname) == 0 && current_ast_mod)
							for (auto &node : current_ast_mod->children)
								if (node && node->type == AST_TYPEDEF && node->str == tname)
									current_scope[tname] = node.get();
						if (current_scope.count(tname) == 0 && current_ast)
							for (auto &node : current_ast->children) {
								if (!node)
									continue;
								if (node->type == AST_TYPEDEF && node->str == tname)
									current_scope[tname] = node.get();
								if (node->type == AST_PACKAGE)
									for (auto &inner : node->children)
										if (inner && inner->type == AST_TYPEDEF && inner->str == tname)
											current_scope[tname] = inner.get();
							}
						if (current_scope.count(tname) > 0) {
							AstNode *resolved = current_scope[tname];
							if (resolved->type == AST_TYPEDEF && !resolved->children.empty())
								resolved = resolved->children[0].get();
							if (resolved->type == AST_STRUCT || resolved->type == AST_UNION) {
								// size BEFORE cloning: a typedef not simplified
								// yet (compilation-unit typedefs come after the
								// module's own declarations) cloned unsized, and
								// the pattern materialized into zero bits
								size_packed_struct(resolved, 0);
								set_attribute(ID::wiretype, resolved->clone());
								// members registered from the typedef's own
								// struct node (stable); see the note above
								add_members_to_scope(resolved, str);
							}
						}
						break;
					}
			// materialize an assignment-pattern value into a packed constant
			// (named members per sv_member, otherwise positional) so member
			// selects and range declarations can fold against the parameter
			// (not for an unpacked ARRAY of structs: its pattern lists
			// elements, not members -- `'{default:'0}` used to fold into one
			// struct-wide constant)
			if (attributes.count(ID::wiretype) &&
					attributes.at(ID::wiretype)->type == AST_STRUCT &&
					unpacked_dimensions == 0 && !attributes.count(ID(sv_unpacked_dims)) &&
					children[0]->type == AST_ASSIGN_PATTERN) {
				AstNode *stype = attributes.at(ID::wiretype).get();
				// the struct's layout with typedef'd struct members resolved:
				// such a member (`sub_t sel;`) can still carry its WIRETYPE
				// here and sizes as one bit, which shifted every member and
				// folded the parameter to garbage. Unresolvable members leave
				// the pattern unfolded (a loud "did not fold" on reads).
				std::unique_ptr<AstNode> stype_resolved;
				bool layout_ok = true;
				if (struct_has_unresolved_members(stype)) {
					stype_resolved = resolve_struct_layout(stype);
					if (stype_resolved)
						stype = stype_resolved.get();
					else
						layout_ok = false;
				}
				while (!children[0]->basic_prep && children[0]->simplify(false, stage, -1, false))
					did_something = true;
				AstNode *pat = children[0].get();
				// a parameter value is a constant expression: fold each
				// element (`unsigned'(W)`, `bit'(0)`, `1024'({...})`), keeping
				// its member key -- folding replaces the node (register v3
				// D13: one cast left the whole cva6 user config unmaterialized
				// and every member read garbage)
				for (auto &el : pat->children) {
					if (!el || el->type == AST_CONSTANT)
						continue;
					std::vector<std::pair<RTLIL::IdString, std::unique_ptr<AstNode>>> keys;
					for (auto id : {ID(sv_member), ID(sv_typekey)})
						if (el->attributes.count(id))
							keys.emplace_back(id, el->attributes.at(id)->clone());
					while (el->simplify(true, stage, -1, false)) { }
					for (auto &k : keys)
						if (!el->attributes.count(k.first))
							el->set_attribute(k.first, std::move(k.second));
					did_something = true;
				}
				// a nested struct pattern (`sel: '{hi: .., lo: ..}`) folds
				// through its member's struct type first; left unfolded, the
				// parameter kept the pattern and member reads were garbage
				if (layout_ok) {
					auto bare_name = [](const std::string &s) {
						return s.empty() || s[0] != '\\' ? s : s.substr(1);
					};
					std::vector<AstNode *> items;
					for (auto &cand : stype->children)
						if (cand && (cand->type == AST_STRUCT_ITEM || cand->type == AST_STRUCT || cand->type == AST_UNION))
							items.push_back(cand.get());
					int pos = 0;
					for (auto &el : pat->children) {
						AstNode *item = nullptr;
						if (el->attributes.count(ID(sv_member))) {
							std::string mname = bare_name(el->attributes.at(ID(sv_member))->str);
							for (auto *cand : items)
								if (bare_name(cand->str) == mname)
									item = cand;
						} else if (!el->attributes.count(ID(sv_typekey)) && pos < GetSize(items)) {
							item = items[pos];
						}
						pos++;
						if (!item || el->type != AST_ASSIGN_PATTERN)
							continue;
						AstNode *nested = (item->type == AST_STRUCT || item->type == AST_UNION) ? item : nullptr;
						if (!nested)
							for (auto &ch : item->children)
								if (ch && ch->type == AST_WIRETYPE && current_scope.count(ch->str) &&
										current_scope.at(ch->str)->type == AST_TYPEDEF &&
										!current_scope.at(ch->str)->children.empty())
									nested = current_scope.at(ch->str)->children[0].get();
						if (!nested || (nested->type != AST_STRUCT && nested->type != AST_UNION))
							continue;
						auto st = nested->clone();
						size_packed_struct(st.get(), 0);
						auto inner = el->clone();
						inner->attributes.erase(ID(sv_member));
						auto tmp = std::make_unique<AstNode>(el->location, AST_LOCALPARAM, std::move(inner));
						tmp->str = "\\$nested_member";
						tmp->set_attribute(ID::wiretype, std::move(st));
						while (tmp->simplify(true, 1, -1, false)) { }
						if (tmp->children.empty() || tmp->children[0]->type != AST_CONSTANT)
							continue;
						auto folded = tmp->children[0]->clone();
						for (auto id : {ID(sv_member), ID(sv_typekey)})
							if (el->attributes.count(id))
								folded->set_attribute(id, el->attributes.at(id)->clone());
						el = std::move(folded);
						did_something = true;
					}
				}
				bool all_const = true, named = false;
				for (auto &el : pat->children) {
					all_const = all_const && el->type == AST_CONSTANT;
					named = named || el->attributes.count(ID(sv_member)) != 0 ||
						el->attributes.count(ID(sv_typekey)) != 0;
				}
				if (all_const && layout_ok) {
					int width = stype->range_left - stype->range_right + 1;
					std::vector<RTLIL::State> bits(width, RTLIL::State::S0);
					auto bare = [](const std::string &s) {
						return s.empty() || s[0] != '\\' ? s : s.substr(1);
					};
					auto place = [&](AstNode *el, AstNode *item) {
						int w = item->range_left - item->range_right + 1;
						std::vector<RTLIL::State> v = el->bitsAsConst(w, el->is_signed).to_bits();
						for (int i = 0; i < w && item->range_right + i < width; i++)
							bits[item->range_right + i] = v[i];
					};
					if (named) {
						int pos = 0;
						AstNode *dflt = nullptr;
						pool<AstNode *> placed;
						for (auto &el : pat->children) {
							AstNode *item = nullptr;
							if (el->attributes.count(ID(sv_typekey))) {
								// a simple-type key fills every member;
								// later keys overwrite earlier ones
								for (auto &cand : stype->children)
									if (cand->type == AST_STRUCT_ITEM) {
										place(el.get(), cand.get());
										placed.insert(cand.get());
									}
								continue;
							}
							if (el->attributes.count(ID(sv_member))) {
								std::string mname = bare(el->attributes.at(ID(sv_member))->str.c_str());
								if (mname == "default") {
									dflt = el.get();
								} else {
									for (auto &cand : stype->children)
										if (cand->str == mname || bare(cand->str) == mname)
											item = cand.get();
									if (!item)
										input_error("Assignment pattern names unknown member `%s' of %s.\n",
												mname.c_str(), log_id(stype->str));
								}
							} else {
								// positional element within a named pattern
								if (pos < GetSize(stype->children))
									item = stype->children[pos].get();
							}
							if (item) {
								place(el.get(), item);
								placed.insert(item);
							}
							pos++;
						}
						// '{default: v} fills every member not otherwise assigned
						if (dflt)
							for (auto &item : stype->children)
								if (!placed.count(item.get()))
									place(dflt, item.get());
					} else {
						if (GetSize(pat->children) != GetSize(stype->children))
							input_error("Assignment pattern element count mismatch for %s: got %d, expected %d.\n",
									log_id(str), GetSize(pat->children), GetSize(stype->children));
						for (int i = 0; i < GetSize(pat->children); i++)
							place(pat->children[i].get(), stype->children[i].get());
					}
					children[0] = mkconst_bits(location, bits, false);
					did_something = true;
				}
			}
		}
		while (!children[0]->basic_prep && children[0]->simplify(false, stage, -1, false) == true)
			did_something = true;
		children[0]->detectSignWidth(width_hint, sign_hint);
		if (children.size() > 1 && children[1]->type == AST_RANGE) {
			while (!children[1]->basic_prep && children[1]->simplify(false, stage, -1, false) == true)
				did_something = true;
			if (!children[1]->range_valid)
				input_error("Non-constant width range on parameter decl.\n");
			width_hint = max(width_hint, children[1]->range_left - children[1]->range_right + 1);
		}
		break;
	case AST_ENUM_ITEM:
		while (!children[0]->basic_prep && children[0]->simplify(false, stage, -1, false))
			did_something = true;
		children[0]->detectSignWidth(width_hint, sign_hint);
		if (children.size() > 1 && children[1]->type == AST_RANGE) {
			while (!children[1]->basic_prep && children[1]->simplify(false, stage, -1, false))
				did_something = true;
			if (!children[1]->range_valid)
				input_error("Non-constant width range on enum item decl.\n");
			width_hint = max(width_hint, children[1]->range_left - children[1]->range_right + 1);
		}
		break;

	case AST_CAST_SIZE: {
		// A typedef name imported from a package can reach here as a plain
		// resolved identifier (the surrounding custom-type wire was already
		// simplified away), where it would silently lower to a 1-bit cast.
		// Normalize it to the same width constant the AST_WIRETYPE path below
		// produces for local types.
		if (children[0]->type == AST_IDENTIFIER && children[0]->id2ast &&
				children[0]->id2ast->type == AST_TYPEDEF &&
				children[0]->id2ast->children.size() == 1) {
			AstNode *td = children[0]->id2ast;
			AstNode *tmpl = td->children[0].get();
			while (tmpl->simplify(const_fold, stage, width_hint, sign_hint)) {};
			int width = -1;
			if (tmpl->type == AST_WIRE || tmpl->type == AST_ENUM) {
				if (tmpl->children.size() > 0 && tmpl->children[0]->type == AST_RANGE)
					width = range_width(this, tmpl->children[0].get());
				else if (tmpl->range_valid)
					width = std::abs(tmpl->range_left - tmpl->range_right) + 1;
				else if (tmpl->children.empty() || tmpl->type == AST_ENUM)
					// `typedef enum logic { ... } e;` / `typedef logic t;`: one bit
					width = 1;
			} else if (tmpl->type == AST_STRUCT || tmpl->type == AST_UNION) {
				width = size_packed_struct(tmpl, 0);
			}
			if (width >= 1) {
				// the cast size is the type's width (this was
				// mkconst_int(loc, 32, width): every such cast was 32 bits)
				children[0] = mkconst_int(children[0]->location, width, true);
				did_something = true;
			}
		}
		if (children[0]->type == AST_WIRE) {
			int width = 1;
			std::unique_ptr<AstNode> node;
			auto* child = children[0].get();
			if (child->children.size() == 0) {
				// Base type (e.g., int).  An unsized one (`logic'(x)`,
				// `bit'(x)`) has no range (range_left stays -1): a 1-bit
				// cast (IEEE 1800-2017 6.24.1), not a 0-bit one (register
				// v3: the last "Static cast with zero or negative size"
				// class in OpenTitan, prim_max_tree / usb_fs_nb_in_pe)
				width = child->range_left - child->range_right + 1;
				if (width <= 0 && child->range_left == -1 && child->range_right == 0)
					width = 1;   // the parser's "no range" marker (`int` carries [31:0] without range_valid)
				node = mkconst_int(child->location, width, child->is_signed);
			} else {
				// User defined type
				log_assert(child->children[0]->type == AST_WIRETYPE);

				const std::string &type_name = child->children[0]->str;
				if (!current_scope.count(type_name))
					input_error("Unknown identifier `%s' used as type name\n", type_name);
				AstNode *resolved_type_node = current_scope.at(type_name);
				if (resolved_type_node->type != AST_TYPEDEF)
					input_error("`%s' does not name a type\n", type_name);
				log_assert(resolved_type_node->children.size() == 1);
				auto* template_node = resolved_type_node->children[0].get();

				// Ensure typedef itself is fully simplified
				while (template_node->simplify(const_fold, stage, width_hint, sign_hint)) {};

				switch (template_node->type)
				{
				case AST_WIRE: {
					if (template_node->children.size() > 0 && template_node->children[0]->type == AST_RANGE)
						width = range_width(this, template_node->children[0].get());
					child->delete_children();
					node = mkconst_int(child->location, width, true);
					break;
				}

				case AST_STRUCT:
				case AST_UNION: {
					child->delete_children();
					width = size_packed_struct(template_node, 0);
					node = mkconst_int(child->location, width, false);
					break;
				}

				default:
					log_error("Don't know how to translate static cast of type %s\n", type2str(template_node->type));
				}
			}

			children.erase(children.begin());
			children.insert(children.begin(), std::move(node));
		}

		detect_width_simple = true;
		children_are_self_determined = true;
		break;
	}

	case AST_TO_BITS:
	case AST_TO_SIGNED:
	case AST_TO_UNSIGNED:
	case AST_SELFSZ:
	case AST_CONCAT:
	case AST_REPLICATE:
	case AST_REDUCE_AND:
	case AST_REDUCE_OR:
	case AST_REDUCE_XOR:
	case AST_REDUCE_XNOR:
	case AST_REDUCE_BOOL:
		detect_width_simple = true;
		children_are_self_determined = true;
		break;

	case AST_ASSIGN_PATTERN:
		// Assignment pattern elements are context-determined by the target element type.
		// Keep child width context intact until whole-array assignment expansion creates scalar assignments.
		detect_width_simple = true;
		break;

	case AST_NEG:
	case AST_BIT_NOT:
	case AST_POS:
	case AST_BIT_AND:
	case AST_BIT_OR:
	case AST_BIT_XOR:
	case AST_BIT_XNOR:
	case AST_ADD:
	case AST_SUB:
	case AST_MUL:
	case AST_DIV:
	case AST_MOD:
		detect_width_simple = true;
		break;

	case AST_SHIFT_LEFT:
	case AST_SHIFT_RIGHT:
	case AST_SHIFT_SLEFT:
	case AST_SHIFT_SRIGHT:
	case AST_POW:
		detect_width_simple = true;
		child_1_is_self_determined = true;
		break;

	case AST_LT:
	case AST_LE:
	case AST_EQ:
	case AST_NE:
	case AST_EQX:
	case AST_NEX:
	case AST_GE:
	case AST_GT:
		width_hint = -1;
		sign_hint = true;
		for (auto& child : children) {
			while (!child->basic_prep && child->simplify(false, stage, -1, false) == true)
				did_something = true;
			child->detectSignWidthWorker(width_hint, sign_hint);
		}
		reset_width_after_children = true;
		break;

	case AST_LOGIC_AND:
	case AST_LOGIC_OR:
	case AST_LOGIC_NOT:
		detect_width_simple = true;
		children_are_self_determined = true;
		break;

	case AST_TERNARY:
		child_0_is_self_determined = true;
		break;

	case AST_MEMRD:
		detect_width_simple = true;
		children_are_self_determined = true;
		break;

	case AST_FCALL:
	case AST_TCALL:
	case AST_SVA:
		children_are_self_determined = true;
		break;

	default:
		width_hint = -1;
		sign_hint = false;
	}

	if (detect_width_simple && width_hint < 0) {
		if (type == AST_REPLICATE)
			while (children[0]->simplify(true, stage, -1, false) == true)
				did_something = true;
		for (auto& child : children)
			while (!child->basic_prep && child->simplify(false, stage, -1, false) == true)
				did_something = true;
		detectSignWidth(width_hint, sign_hint);
	}

	if (type == AST_FCALL && str == "\\$past")
		detectSignWidth(width_hint, sign_hint);

	if (type == AST_TERNARY) {
		if (width_hint < 0) {
			while (!children[0]->basic_prep && children[0]->simplify(true, stage, -1, false))
				did_something = true;

			bool backup_unevaluated_tern_branch = unevaluated_tern_branch;
			AstNode *chosen = get_tern_choice().first;

			unevaluated_tern_branch = backup_unevaluated_tern_branch || chosen == children[2].get();
			while (!children[1]->basic_prep && children[1]->simplify(false, stage, -1, false))
				did_something = true;

			unevaluated_tern_branch = backup_unevaluated_tern_branch || chosen == children[1].get();
			while (!children[2]->basic_prep && children[2]->simplify(false, stage, -1, false))
				did_something = true;

			unevaluated_tern_branch = backup_unevaluated_tern_branch;
			detectSignWidth(width_hint, sign_hint);
		}
		int width_hint_left, width_hint_right;
		bool sign_hint_left, sign_hint_right;
		bool found_real_left, found_real_right;
		children[1]->detectSignWidth(width_hint_left, sign_hint_left, &found_real_left);
		children[2]->detectSignWidth(width_hint_right, sign_hint_right, &found_real_right);
		if (found_real_left || found_real_right) {
			child_1_is_self_determined = true;
			child_2_is_self_determined = true;
		}
	}

	if (type == AST_CONDX && children.size() > 0 && children.at(0)->type == AST_CONSTANT) {
		for (auto &bit : children.at(0)->bits)
			if (bit == State::Sz || bit == State::Sx)
				bit = State::Sa;
	}

	if (type == AST_CONDZ && children.size() > 0 && children.at(0)->type == AST_CONSTANT) {
		for (auto &bit : children.at(0)->bits)
			if (bit == State::Sz)
				bit = State::Sa;
	}

	if (const_fold && type == AST_CASE)
	{
		detectSignWidth(width_hint, sign_hint);
		while (children[0]->simplify(const_fold, stage, width_hint, sign_hint)) { }
		// casex treats x and z in the selector and in the item as don't-cares.
		auto casex_match = [](const RTLIL::Const &expr, const RTLIL::Const &item) {
			int n = std::max(GetSize(expr), GetSize(item));
			for (int bit = 0; bit < n; bit++) {
				RTLIL::State a = bit < GetSize(expr) ? expr[bit] : RTLIL::State::S0;
				RTLIL::State b = bit < GetSize(item) ? item[bit] : RTLIL::State::S0;
				auto wild = [](RTLIL::State s) {
					return s == RTLIL::State::Sx || s == RTLIL::State::Sz || s == RTLIL::State::Sa;
				};
				if (wild(a) || wild(b))
					continue;
				if (a != b)
					return false;
			}
			return true;
		};
		if (children[0]->type == AST_IDENTIFIER && children[0]->children.empty()) {
			AstNode *drv = constant_cont_driver(current_ast_mod, children[0]->str);
			if (drv)
				children[0] = drv->clone();
		}
		if (children[0]->type == AST_CONSTANT) {
			children[0]->is_signed = sign_hint;
			RTLIL::Const case_expr = children[0]->bitsAsConst(width_hint, sign_hint);
			std::vector<std::unique_ptr<AstNode>> new_children;
			new_children.push_back(std::move(children[0]));
			for (int i = 1; i < GetSize(children); i++) {
				auto& child = children[i];
				log_assert(child->type == AST_COND || child->type == AST_CONDX || child->type == AST_CONDZ);
				for (auto& v : child->children) {
					if (v->type == AST_DEFAULT)
						goto keep_const_cond;
					if (v->type == AST_BLOCK)
						continue;
					while (v->simplify(const_fold, stage, width_hint, sign_hint)) { }
					// case (expr) matches lowers each item to a boolean compared with 1.
					if (str == "\\matches" && v->type == AST_CONSTANT) {
						if (v->asBool()) {
							i = GetSize(children);
							goto keep_const_cond;
						}
						continue;
					}
					if (v->type == AST_CONSTANT && child->type == AST_CONDX) {
						if (casex_match(case_expr, v->bitsAsConst())) {
							i = GetSize(children);
							goto keep_const_cond;
						}
						continue;
					}
					if (v->type == AST_CONSTANT && child->type == AST_CONDZ) {
						// casez: z and ? are don't-cares. x is not.
						auto casez_match = [](const RTLIL::Const &expr, const RTLIL::Const &item) {
							int n = std::max(GetSize(expr), GetSize(item));
							for (int bit = 0; bit < n; bit++) {
								RTLIL::State a = bit < GetSize(expr) ? expr[bit] : RTLIL::State::S0;
								RTLIL::State b = bit < GetSize(item) ? item[bit] : RTLIL::State::S0;
								auto wild = [](RTLIL::State s) {
									return s == RTLIL::State::Sz || s == RTLIL::State::Sa;
								};
								if (wild(a) || wild(b))
									continue;
								if (a == RTLIL::State::Sx || b == RTLIL::State::Sx || a != b)
									return false;
							}
							return true;
						};
						if (casez_match(case_expr, v->bitsAsConst())) {
							i = GetSize(children);
							goto keep_const_cond;
						}
						continue;
					}
					if (v->type == AST_CONSTANT && v->bits_only_01() && case_expr.is_fully_def()) {
						RTLIL::Const case_item_expr = v->bitsAsConst(width_hint, sign_hint);
						RTLIL::Const match = const_eq(case_expr, case_item_expr, sign_hint, sign_hint, 1);
						log_assert(match.size() == 1);
						if (match.front() == RTLIL::State::S1) {
							// This is the only reachable case. Skip to the end
							i = GetSize(children);
							goto keep_const_cond;
						}
						continue;
					}
					goto keep_const_cond;
				}
				if (0)
			keep_const_cond:
					new_children.push_back(std::move(child));
			}
			new_children.swap(children);
		}
	}

	dict<std::string, pool<int>> backup_memwr_visible;
	dict<std::string, pool<int>> final_memwr_visible;

	if (type == AST_CASE && stage == 2) {
		backup_memwr_visible = current_memwr_visible;
		final_memwr_visible = current_memwr_visible;
	}

	// simplify all children first
	// (iterate by index as e.g. auto wires can add new children in the process)
	bool backup_sva_context = current_sva_context;
	if (type == AST_SVA)
		current_sva_context = true;
	for (size_t i = 0; i < children.size(); i++) {
		if (!children[i])
			continue;
		bool did_something_here = true;
		bool backup_flag_autowire = flag_autowire;
		bool backup_unevaluated_tern_branch = unevaluated_tern_branch;
		if ((type == AST_GENFOR || type == AST_FOR) && i >= 3)
			break;
		if ((type == AST_GENIF || type == AST_GENCASE) && i >= 1)
			break;
		if (type == AST_GENBLOCK)
			break;
		if (type == AST_CELLARRAY && (children[i]->type == AST_CELL || children[i]->type == AST_PRIMITIVE))
			continue;
		if (type == AST_BLOCK && !str.empty())
			break;
		if (type == AST_PREFIX && i >= 1)
			break;
		if (type == AST_DEFPARAM && i == 0)
			flag_autowire = true;
		if (type == AST_TERNARY && i > 0 && !unevaluated_tern_branch) {
			AstNode *chosen = get_tern_choice().first;
			unevaluated_tern_branch = chosen && chosen != children[i].get();
		}
		while (did_something_here && i < children.size() && children[i]) {
			bool const_fold_here = const_fold;
			int width_hint_here = width_hint;
			bool sign_hint_here = sign_hint;
			if (i == 0 && (type == AST_REPLICATE || type == AST_WIRE))
				const_fold_here = true;
			if (type == AST_PARAMETER || type == AST_LOCALPARAM)
				const_fold_here = true;
			if (type == AST_BLOCK) {
				current_block = this;
				current_block_child = children[i].get();
			}
			if ((type == AST_ALWAYS || type == AST_INITIAL) && children[i]->type == AST_BLOCK)
				current_top_block = children[i].get();
			if (i == 0 && child_0_is_self_determined)
				width_hint_here = -1, sign_hint_here = false;
			if (i == 1 && child_1_is_self_determined)
				width_hint_here = -1, sign_hint_here = false;
			if (i == 2 && child_2_is_self_determined)
				width_hint_here = -1, sign_hint_here = false;
			if (children_are_self_determined)
				width_hint_here = -1, sign_hint_here = false;
			did_something_here = children[i]->simplify(const_fold_here, stage, width_hint_here, sign_hint_here);
			if (did_something_here)
				did_something = true;
		}
		if (stage == 2 && i < children.size() && children[i] && children[i]->type == AST_INITIAL && current_ast_mod != this) {
			current_ast_mod->children.push_back(std::move(children[i]));
			children.erase(children.begin() + (i--));
			did_something = true;
		}
		flag_autowire = backup_flag_autowire;
		unevaluated_tern_branch = backup_unevaluated_tern_branch;
		if (stage == 2 && type == AST_CASE) {
			for (auto &x : current_memwr_visible) {
				for (int y : x.second)
					final_memwr_visible[x.first].insert(y);
			}
			current_memwr_visible = backup_memwr_visible;
		}
	}
	current_sva_context = backup_sva_context;
	for (auto &attr : attributes) {
		if (!attr.second)
			continue;
		while (attr.second->simplify(true, stage, -1, false))
			did_something = true;
	}
	if (type == AST_CASE && stage == 2) {
		current_memwr_visible = final_memwr_visible;
	}
	if (type == AST_ALWAYS && stage == 2) {
		current_memwr_visible.clear();
		current_memwr_count.clear();
	}

	if (reset_width_after_children) {
		width_hint = backup_width_hint;
		sign_hint = backup_sign_hint;
		if (width_hint < 0)
			detectSignWidth(width_hint, sign_hint);
	}

	current_block = backup_current_block;
	current_block_child = backup_current_block_child;
	current_top_block = backup_current_top_block;
	current_always = backup_current_always;
	current_always_clocked = backup_current_always_clocked;

	for (auto it = backup_scope.begin(); it != backup_scope.end(); it++) {
		if (it->second == nullptr)
			current_scope.erase(it->first);
		else
			current_scope[it->first] = it->second;
	}

	if (type == AST_MODULE || type == AST_INTERFACE)
		current_scope.clear();

	// convert defparam nodes to cell parameters
	if (type == AST_DEFPARAM && !children.empty())
	{
		if (children[0]->type != AST_IDENTIFIER)
			input_error("Module name in defparam contains non-constant expressions!\n");

		string modname, paramname = children[0]->str;

		size_t pos = paramname.rfind('.');

		while (pos != 0 && pos != std::string::npos)
		{
			modname = paramname.substr(0, pos);

			if (current_scope.count(modname))
				break;

			pos = paramname.rfind('.', pos - 1);
		}

		if (pos == std::string::npos)
			input_error("Can't find object for defparam `%s`!\n", RTLIL::unescape_id(paramname));

		paramname = "\\" + paramname.substr(pos+1);

		if (current_scope.at(modname)->type != AST_CELL)
			input_error("Defparam argument `%s . %s` does not match a cell!\n",
					RTLIL::unescape_id(modname), RTLIL::unescape_id(paramname));

		auto paraset = std::make_unique<AstNode>(location, AST_PARASET, children[1]->clone(), GetSize(children) > 2 ? children[2]->clone() : nullptr);
		paraset->str = paramname;

		AstNode *cell = current_scope.at(modname);
		cell->children.insert(cell->children.begin() + 1, std::move(paraset));
		delete_children();
	}

	// resolve typedefs
	if (type == AST_TYPEDEF) {
		log_assert(children.size() == 1);
		auto& type_node = children[0];
		log_assert(type_node->type == AST_WIRE || type_node->type == AST_MEMORY || type_node->type == AST_STRUCT || type_node->type == AST_UNION);
		while (type_node->simplify(const_fold, stage, width_hint, sign_hint)) {
			did_something = true;
		}
		log_assert(!type_node->is_custom_type);
	}

	// resolve types of wires
	if (type == AST_WIRE || type == AST_MEMORY) {
		// a typedef'd unpacked-array port (marked by the parser) resolves
		// like an unpacked declaration; flatten_unpacked_ports(true) turns
		// the memory back into a wide port wire after stage 1
		if (type == AST_WIRE && is_custom_type && port_id != 0 && attributes.count(ID(sv_port_unpacked))) {
			attributes.erase(ID(sv_port_unpacked));
			type = AST_MEMORY;
		}
		if (is_custom_type) {
			int wiretype_idx = -1;
			for (int i = 0; i < GetSize(children); i++) {
				if (children[i] && children[i]->type == AST_WIRETYPE) {
					wiretype_idx = i;
					break;
				}
			}
			if (wiretype_idx < 0) {
				// A struct or union used as the declaration itself has no separate type name.
				is_custom_type = false;
			} else if (wiretype_idx != 0) {
				auto wiretype = std::move(children[wiretype_idx]);
				children.erase(children.begin() + wiretype_idx);
				children.insert(children.begin(), std::move(wiretype));
			}
		}
		if (is_custom_type) {
			log_assert(children.size() >= 1);
			log_assert(children[0]->type == AST_WIRETYPE);
			auto type_name = children[0]->str;
			bool known_typedef = current_scope.count(type_name) && current_scope.at(type_name)->type == AST_TYPEDEF
				&& !current_scope.at(type_name)->children.empty() && current_scope.at(type_name)->children[0];
			// A `string` variable (IEEE 1800-2017 6.16) that is only ever
			// assigned string literals (typically a message tag) holds one
			// of those values: model it as a vector as wide as the longest.
			// Shorter literals zero-extend on the left, as literal operands
			// do, so equality against them is exact. Any other assignment
			// keeps the degradation below.
			int string_width = -1;
			if (!known_typedef && type_name == "\\string" && type == AST_WIRE && current_ast_mod && !str.empty()) {
				bool only_literals = true;
				int widest = 8;
				std::function<void(AstNode *)> scan = [&](AstNode *n) {
					if (!n || !only_literals)
						return;
					if ((n->type == AST_ASSIGN_EQ || n->type == AST_ASSIGN_LE || n->type == AST_ASSIGN) &&
							n->children.size() >= 2 && n->children[0] && n->children[0]->type == AST_IDENTIFIER &&
							n->children[0]->str == str) {
						AstNode *rhs = n->children[1].get();
						if (!n->children[0]->children.empty() || !rhs || rhs->type != AST_CONSTANT || !rhs->is_string)
							only_literals = false;
						else
							widest = std::max(widest, GetSize(rhs->bits));
					}
					for (auto &c : n->children)
						scan(c.get());
				};
				scan(current_ast_mod);
				if (only_literals)
					string_width = widest;
			}
			if (string_width > 0) {
				is_custom_type = false;
				children.erase(children.begin());
				children.insert(children.begin(), make_range(location, string_width - 1, 0));
				is_logic = true;
				set_attribute(ID(sv_string), AstNode::mkconst_int(location, 1, false));
				did_something = true;
			} else if (!known_typedef) {
				// The typedef often lives in a package this file did not elaborate.
				// Keep any declared range and otherwise use a single logic bit.
				degrade(location.to_loc(), "Unknown identifier `%s' used as type name; treating unresolved types as logic.\n", type_name.c_str());
				is_custom_type = false;
				children.erase(children.begin());
				if (children.empty() || !children[0] || children[0]->type != AST_RANGE)
					children.insert(children.begin(), make_range(location, 0, 0));
				did_something = true;
			} else {
			AstNode *resolved_type_node = current_scope.at(type_name);
			log_assert(resolved_type_node->children.size() == 1);
			auto& template_node = resolved_type_node->children[0];

			// Resolve the typedef from the bottom up, recursing within the current
			// block of code. Defer further simplification until the complete type is
			// resolved.
			while (template_node->is_custom_type && template_node->simplify(const_fold, stage, width_hint, sign_hint)) {};

			// Unpacked array of a packed struct/union type (e.g.
			// `st_t w [15:0]` or `typedef st_t arr_t [15:0]`): the generic
			// path below would graft the unpacked range onto a struct
			// member. Materialize a real memory whose packed element is the
			// struct width and keep the struct template reachable through
			// the wiretype attribute so element member selects can be
			// resolved. Runs before the packed-struct-wire path: a direct
			// `st_t w [15:0]` declaration is AST_MEMORY with the unpacked
			// range attached, not a plain struct instance.
			if ((template_node->type == AST_STRUCT || template_node->type == AST_UNION) &&
					type == AST_MEMORY && GetSize(children) > 1) {
				bool any_unpacked = false;
				for (int i = 1; i < GetSize(children); i++)
					if (children[i] && (children[i]->type == AST_RANGE || children[i]->type == AST_MULTIRANGE))
						any_unpacked = true;
				if (any_unpacked) {
					if (!template_node->range_valid)
						size_packed_struct(template_node.get(), 0);
					if (!template_node->range_valid)
						input_error("Unsized struct/union type `%s' in unpacked array declaration.\n", type_name.c_str());
					// [WIRETYPE, packed, unpacked...]: with more than one range
					// the first is the packed dimension (`st_t [1:0] w [2]`),
					// as in the generic path below
					int first_unpacked = 1;
					if (GetSize(children) > 2 && children[1] && children[1]->type == AST_RANGE) {
						first_unpacked = 2;
					} else if (GetSize(children) > 2 && children[1] && children[1]->type == AST_MULTIRANGE) {
						input_error("Multiple packed dimensions on struct/union type `%s' in unpacked array declaration are not supported.\n", type_name.c_str());
					}
					int elem_w = std::abs(template_node->range_left - template_node->range_right) + 1;
					newNode = std::make_unique<AstNode>(location, AST_MEMORY);
					newNode->str = str;
					newNode->is_logic = true;
					newNode->port_id = port_id;
					if (first_unpacked == 1)
						newNode->children.push_back(make_range(location, template_node->range_left, template_node->range_right));
					else {
						// packed [n] x struct, shaped like `logic [n][w-1:0] m [..]`
						// so `m[i][j]` selects a whole struct
						auto multi = std::make_unique<AstNode>(location, AST_MULTIRANGE);
						multi->children.push_back(children[1]->clone());
						multi->children.push_back(make_range(location, elem_w - 1, 0));
						newNode->children.push_back(std::move(multi));
					}
					for (int i = first_unpacked; i < GetSize(children); i++)
						if (children[i] && (children[i]->type == AST_RANGE || children[i]->type == AST_MULTIRANGE))
							newNode->children.push_back(children[i]->clone());
					newNode->set_attribute(ID::wiretype, mkconst_str(newNode->location, resolved_type_node->str));
					newNode->is_input = is_input;
					newNode->is_output = is_output;
					newNode->is_wand = is_wand;
					newNode->is_wor = is_wor;
					for (auto &pair : attributes)
						newNode->set_attribute(pair.first, pair.second->clone());
					if (!str.empty())
						current_scope[str] = this;
					goto apply_newNode;
				}
			}

			// a local of an unnamed block is named `$unnamed_block$N.x`: it
			// took the generic path below, became a bare struct node, and its
			// member selects (`x.m`) were implicitly declared 1-bit wires
			if (is_user_scope_name(str) && (template_node->type == AST_STRUCT || template_node->type == AST_UNION)) {
				// replace instance with wire representing the packed structure
				AstNode *packed_dims = nullptr;
				if (type == AST_WIRE && GetSize(children) > 1 && children[1] &&
						(children[1]->type == AST_RANGE || children[1]->type == AST_MULTIRANGE))
					packed_dims = children[1].get();
				// a typedef'd packed array of a struct (`typedef st_t [7:0]
				// arr_t; arr_t x;`) is declared like `st_t [7:0] x`, so
				// `x[i]` is an element and `x[i].m` its member
				AstNode *struct_tmpl = template_node.get();
				std::unique_ptr<AstNode> elem_dims;
				if (struct_tmpl->attributes.count(ID(sv_elem_type)) && struct_tmpl->attributes.count(ID(sv_elem_range)) &&
						struct_tmpl->attributes.at(ID(sv_elem_type))->range_valid) {
					elem_dims = struct_tmpl->attributes.at(ID(sv_elem_range))->clone();
					struct_tmpl = struct_tmpl->attributes.at(ID(sv_elem_type)).get();
					if (packed_dims) {
						auto merged = packed_dims->clone();
						prepend_ranges(elem_dims, merged.get());
					}
					packed_dims = elem_dims.get();
				}
				newNode = make_packed_struct(struct_tmpl, str, attributes, packed_dims);
				newNode->set_attribute(ID::wiretype, mkconst_str(newNode->location, resolved_type_node->str));
				// add original input/output attribute to resolved wire
				newNode->is_input = this->is_input;
				newNode->is_output = this->is_output;
				current_scope[str] = this;
				goto apply_newNode;
			}

			// Prepare replacement node.
			newNode = template_node->clone();
			newNode->str = str;
			newNode->port_id = port_id;
			if (!newNode->attributes.count(ID::wiretype))
				newNode->set_attribute(ID::wiretype, mkconst_str(newNode->location, resolved_type_node->str));
			newNode->is_input = is_input;
			newNode->is_output = is_output;
			newNode->is_wand = is_wand;
			newNode->is_wor = is_wor;
			for (auto &pair : attributes)
				newNode->set_attribute(pair.first, pair.second->clone());

			// if an enum then add attributes to support simulator tracing
			newNode->annotateTypedEnums(template_node.get());

			bool add_packed_dimensions = (type == AST_WIRE && GetSize(children) > 1) || (type == AST_MEMORY && GetSize(children) > 2);

			// Cannot add packed dimensions if unpacked dimensions are already specified.
			if (add_packed_dimensions && newNode->type == AST_MEMORY)
				input_error("Cannot extend unpacked type `%s' with packed dimensions\n", type_name);

			// Add packed dimensions.
			if (add_packed_dimensions) {
				if (newNode->type == AST_STRUCT || newNode->type == AST_UNION) {
					// Packed array of a struct/union type (e.g. the
					// OpenTitan reg-model `mreg_t [7:0] x;` members):
					// materialize N inline copies of the members so the
					// packed layout is correct. Copies are stored
					// declaration order with index N-1 first, matching
					// packed-array MSB-first order. The copies stay
					// anonymous: indexed member selects `x[i].member` are
					// not supported, but plain member offsets before/after
					// the array are now computed correctly (a single copy
					// silently shifted every following member's offset).
					auto& packed = children[1];
					while (!packed->basic_prep && packed->simplify(const_fold, stage, -1, false)) { }
					int n = 1;
					if (packed->type == AST_MULTIRANGE) {
						for (auto &sub : packed->children) {
							while (!sub->basic_prep && sub->simplify(const_fold, stage, -1, false)) { }
							if (!sub->range_valid)
								input_error("Non-constant packed dimension on struct/union member %s.\n", str.c_str());
							n *= std::abs(sub->range_left - sub->range_right) + 1;
						}
					} else {
						if (!packed->range_valid)
							input_error("Non-constant packed dimension on struct/union member %s.\n", str.c_str());
						n = std::abs(packed->range_left - packed->range_right) + 1;
					}
					std::vector<std::unique_ptr<AstNode>> member_copies;
					for (auto &m : newNode->children)
						member_copies.push_back(m->clone());
					newNode->children.clear();
					for (int i = 0; i < n; i++)
						for (auto &m : member_copies)
							newNode->children.push_back(m->clone());
					if (newNode->type == AST_UNION && n > 1)
						// the copies of a union's members all sit at
						// offset 0, so the array would be one element
						// wide: refuse rather than lay it out wrong
						input_error("Packed array of a union type as a struct member (`%s') is not supported.\n", str.c_str());
					if (packed->type == AST_RANGE) {
						// single outer dimension: size_packed_struct
						// records [N][E] from sv_elem_range, and the
						// AST_PREFIX rewrite of `s.x[i].m` reads the
						// element type's member offsets from sv_elem_type
						newNode->set_attribute(ID(sv_elem_range), packed->clone());
						newNode->set_attribute(ID(sv_elem_type), template_node->clone());
					}
					// a struct member is re-sized with its enclosing struct;
					// a typedef (`typedef st_t [7:0] arr_t`) or a wire is
					// not, and the clone still carries the one-element range
					if (str.empty() || str[0] == '\\') {
						newNode->range_valid = false;
						newNode->dimensions.clear();
						size_packed_struct(newNode.get(), 0);
					}
				} else {
					auto& packed = children[1];
					if (newNode->children.empty())
						newNode->children.insert(newNode->children.begin(), packed->clone());
					else
						prepend_ranges(newNode->children[0], packed.get());
				}
			}

			// Add unpacked dimensions.
			if (type == AST_MEMORY) {
				auto& unpacked = children.back();
				// a one-bit element type (`logic`) has no range child: give
				// it [0:0] so the unpacked range does not land in the
				// packed slot (a malformed memory that crashed mem2reg)
				if (newNode->children.empty() || !newNode->children[0] ||
						(newNode->children[0]->type != AST_RANGE && newNode->children[0]->type != AST_MULTIRANGE))
					newNode->children.insert(newNode->children.begin(), make_range(location, 0, 0));
				if (GetSize(newNode->children) < 2)
					newNode->children.push_back(unpacked->clone());
				else
					prepend_ranges(newNode->children[1], unpacked.get());
				newNode->type = type;
			}

			// Prepare to generate dimensions metadata for the resolved type.
			// If the template wire was already simplified its packed
			// multirange has been compacted to one flat AST_RANGE and only
			// `dimensions` still records the structure (`typedef logic
			// [4:0][7:0] plane_t`); regenerating from the flat child would
			// turn every `s[x]` lane select into a bit select.  Keep the
			// template's metadata whenever no extra dimensions are added.
			if (!add_packed_dimensions && type != AST_MEMORY && !template_node->dimensions.empty()) {
				newNode->dimensions = template_node->dimensions;
				newNode->unpacked_dimensions = template_node->unpacked_dimensions;
			} else {
				newNode->dimensions.clear();
				newNode->unpacked_dimensions = 0;
			}

			goto apply_newNode;
			}
		}
	}

	// resolve types of parameters
	if (type == AST_LOCALPARAM || type == AST_PARAMETER) {
		if (is_custom_type) {
			log_assert(children.size() >= 2);
			log_assert(children[1]->type == AST_WIRETYPE);

			// Pretend it's just a wire in order to resolve the type in the code block above.
			AstNodeType param_type = type;
			type = AST_WIRE;
			auto expr = std::move(children.front());
			children.erase(children.begin());
			while (is_custom_type && simplify(const_fold, stage, width_hint, sign_hint)) {};
			type = param_type;
			children.insert(children.begin(), std::move(expr));
			is_custom_type = false;

			if (children.size() > 1 && children[1] && children[1]->type == AST_MEMORY)
				input_error("unpacked array type `%s' cannot be used for a parameter\n", children[1]->str);
			fixup_hierarchy_flags();
			did_something = true;
		}
		log_assert(!is_custom_type);
	}

	// element member select on an array of a packed struct/union, unpacked
	// (`st_t W [3:0]`, a memory) or packed (`st_t [3:0] W`, a multirange
	// wire): W[idx].member -> W[idx][member_range]; the indexed-word paths
	// for reads (multidimensional range) and writes (memory write port,
	// packed part-select) already handle the rewritten form. A nested
	// member path (`W[idx].a.b`) descends the struct; a select on the
	// member (`W[idx].data[k]`, `[hi:lo]`, `[k +: n]`) is offset into the
	// member's bits. Must run before the constant-prefix flattening below,
	// which would otherwise turn this into a hierarchical-looking
	// `W[0].member` identifier that silently autowires.
	if (type == AST_PREFIX && GetSize(children) == 2 && children[1]->type == AST_IDENTIFIER &&
			current_scope.count(str) &&
			(current_scope.at(str)->type == AST_MEMORY ||
			 (current_scope.at(str)->type == AST_WIRE && GetSize(current_scope.at(str)->dimensions) > 1) ||
			 ((current_scope.at(str)->type == AST_STRUCT || current_scope.at(str)->type == AST_UNION) &&
			  current_scope.at(str)->attributes.count(ID(sv_elem_type))))) {
		AstNode *decl = current_scope.at(str);
		// a struct member array (`s.x[i].m`) carries its element type; the
		// identifier keeps the member path and the struct-member
		// resolution below turns the element-relative range into bits of
		// the struct wire
		AstNode *stype = decl->attributes.count(ID(sv_elem_type)) ? decl->attributes.at(ID(sv_elem_type)).get()
				: resolve_wiretype_struct(decl);
		if (stype && stype->range_valid) {
			std::string mname = children[1]->str;
			if (!mname.empty() && mname[0] == '\\')
				mname = mname.substr(1);
			AstNode *item = stype;
			size_t pos = 0;
			while (item && pos <= mname.size()) {
				size_t dot = mname.find('.', pos);
				std::string seg = mname.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
				AstNode *next = nullptr;
				for (auto &cand : item->children) {
					std::string cname = cand->str;
					if (!cname.empty() && cname[0] == '\\')
						cname = cname.substr(1);
					if (cname == seg) {
						next = cand.get();
						break;
					}
				}
				item = next;
				if (dot == std::string::npos)
					break;
				pos = dot + 1;
			}
			if (item && item->range_valid) {
				AstNode *msel = children[1]->children.empty() ? nullptr : children[1]->children[0].get();
				int ndims = GetSize(item->dimensions);
				int sel_dims = msel ? (msel->type == AST_MULTIRANGE ? GetSize(msel->children) : 1) : 0;
				if (msel && (children[1]->children.size() != 1 || (msel->type != AST_RANGE && msel->type != AST_MULTIRANGE) ||
						ndims < 1 || sel_dims > ndims))
					input_error("Unsupported select on member `%s' of array element `%s'.\n", mname.c_str(), str.c_str());
				newNode = std::make_unique<AstNode>(location, AST_IDENTIFIER);
				newNode->str = str;
				// the expression has the member's type: $bits/$size and
				// friends read it from here (the range below is bits of W)
				newNode->set_attribute(ID(sv_elem_member), item->clone());
				newNode->set_attribute(ID(sv_elem_member_sel), mkconst_int(location, sel_dims, true));
				// shape mirrors a parsed `W[idx][member_bits]` access:
				// one MULTIRANGE child, split downstream into unpacked
				// index + packed member range (memory read/write paths)
				auto mr = std::make_unique<AstNode>(location, AST_MULTIRANGE);
				mr->children.push_back(std::make_unique<AstNode>(location, AST_RANGE, children[0]->clone()));
				if (msel) {
					// a select on the member (`.key[j]`, `.data[hi:lo]`,
					// `.key[j][c +: w]`): the member is indexed like a
					// packed array of its own dimensions, then shifted
					// to the element's bit space
					auto tmp = std::make_unique<AstNode>(location, AST_IDENTIFIER, msel->clone());
					tmp->str = str;
					auto rel = tmp->make_index_range(item, false);
					int off = item->range_right;
					for (auto &b : rel->children) {
						if (b->type == AST_CONSTANT)
							b = mkconst_int(location, b->integer + off, true);
						else if (off != 0)
							b = std::make_unique<AstNode>(location, AST_ADD, std::move(b), mkconst_int(location, off, true));
					}
					mr->children.push_back(std::move(rel));
				} else {
					mr->children.push_back(std::make_unique<AstNode>(location, AST_RANGE,
							mkconst_int(location, item->range_left, true),
							mkconst_int(location, item->range_right, true)));
				}
				newNode->children.push_back(std::move(mr));
				goto apply_newNode;
			}
		}
	}

	// a member of an element of a PACKED array-of-struct parameter
	// (`R[1].sel.hi` with `localparam range_t [1:0] R = '{...}`, OpenTitan
	// RACL ranges): fold it from the parameter's constant value. It used to
	// become an implicitly declared undriven wire.
	if (type == AST_PREFIX && GetSize(children) == 2 && children[1]->type == AST_IDENTIFIER &&
			children[1]->children.empty() && current_scope.count(str)) {
		AstNode *decl = current_scope.at(str);
		if ((decl->type == AST_PARAMETER || decl->type == AST_LOCALPARAM) && decl->unpacked_dimensions == 0 &&
				GetSize(decl->dimensions) >= 2 && !decl->children.empty() && decl->children[0]->type == AST_CONSTANT &&
				decl->attributes.count(ID::wiretype) && decl->attributes.at(ID::wiretype)->type == AST_CONSTANT) {
			std::string tn = decl->attributes.at(ID::wiretype)->str;
			AstNode *td = current_scope.count(tn) ? current_scope.at(tn) : nullptr;
			AstNode *etype = (td && td->type == AST_TYPEDEF && !td->children.empty() &&
					(td->children[0]->type == AST_STRUCT || td->children[0]->type == AST_UNION)) ? td->children[0].get() : nullptr;
			std::unique_ptr<AstNode> st;
			if (etype)
				st = struct_has_unresolved_members(etype) ? resolve_struct_layout(etype) : etype->clone();
			if (st) {
				size_packed_struct(st.get(), 0);
				int ew = st->range_left - st->range_right + 1;
				auto &dm = decl->dimensions[0];
				int n_inner = 1;
				for (int i = 1; i < GetSize(decl->dimensions); i++)
					n_inner *= decl->dimensions[i].range_width;
				while (children[0]->simplify(true, stage, -1, false)) { }
				AstNode *idx = children[0].get();
				if (idx->type == AST_RANGE && idx->children.size() == 1)
					idx = idx->children[0].get();
				if (n_inner == ew && idx->type == AST_CONSTANT) {
					int index = idx->asInt(true);
					int pos = dm.range_swapped ? dm.range_right + dm.range_width - 1 - index : index - dm.range_right;
					if (pos < 0 || pos >= dm.range_width)
						input_error("Index %d of parameter array %s is out of range.\n", index, log_id(str));
					// the member path (`sel.hi`) through nested structs;
					// nested member ranges are offsets in the element
					std::string mname = children[1]->str;
					if (!mname.empty() && mname[0] == '\\')
						mname = mname.substr(1);
					AstNode *item = st.get();
					size_t p0 = 0;
					while (item) {
						size_t dot = mname.find('.', p0);
						std::string seg = mname.substr(p0, dot == std::string::npos ? std::string::npos : dot - p0);
						AstNode *next = nullptr;
						for (auto &cand : item->children) {
							std::string cn = cand->str;
							if (!cn.empty() && cn[0] == '\\')
								cn = cn.substr(1);
							if (cn == seg)
								next = cand.get();
						}
						item = next;
						if (dot == std::string::npos)
							break;
						p0 = dot + 1;
					}
					if (!item || !item->range_valid)
						input_error("Parameter array %s has no member `%s'.\n", log_id(str), mname.c_str());
					std::vector<RTLIL::State> bits = decl->children[0]->bitsAsConst(dm.range_width * ew, false).to_bits();
					int lo = pos * ew + std::min(item->range_left, item->range_right) - st->range_right;
					int hi = pos * ew + std::max(item->range_left, item->range_right) - st->range_right;
					std::vector<RTLIL::State> v(bits.begin() + lo, bits.begin() + hi + 1);
					newNode = mkconst_bits(location, v, false);
					goto apply_newNode;
				}
			}
		}
	}

	// a member of an element of an unpacked-array PARAMETER (`P[1].m` with
	// `parameter c_t P [4] = ...`): fold it from the parameter's value.  The
	// constant-prefix rule below would turn it into an identifier `P[1].m`
	// read as an undriven implicit wire.
	if (type == AST_PREFIX && GetSize(children) == 2 && children[1]->type == AST_IDENTIFIER &&
			children[1]->children.empty() && current_scope.count(str)) {
		AstNode *decl = current_scope.at(str);
		if ((decl->type == AST_PARAMETER || decl->type == AST_LOCALPARAM) && decl->unpacked_dimensions == 1 &&
				!decl->children.empty() && decl->children[0]->type == AST_ASSIGN_PATTERN) {
			while (children[0]->simplify(true, stage, -1, false)) { }
			AstNode *idx = children[0].get();
			if (idx->type == AST_RANGE && idx->children.size() == 1)
				idx = idx->children[0].get();
			AstNode *pat = decl->children[0].get();
			// the element struct type (the wiretype attribute holds its NAME)
			AstNode *etype = decl->attributes.count(ID::wiretype) ? decl->attributes.at(ID::wiretype).get() : nullptr;
			if (etype && etype->type != AST_STRUCT && etype->type != AST_UNION) {
				std::string tn = etype->str;
				etype = nullptr;
				AstNode *td = current_scope.count(tn) ? current_scope.at(tn) : nullptr;
				if (!td && current_ast_mod)
					for (auto &c : current_ast_mod->children)
						if (c && c->type == AST_TYPEDEF && c->str == tn)
							td = c.get();
				if (td && td->type == AST_TYPEDEF && !td->children.empty())
					td = td->children[0].get();
				if (td && (td->type == AST_STRUCT || td->type == AST_UNION))
					etype = td;
			}
			if (etype) {
				auto &dm = decl->dimensions[0];
				bool ascending = dm.range_swapped;
				int base = ascending ? dm.range_right : dm.range_right + dm.range_width - 1;
				// `'{default: v}` (alone or after explicit elements): v for
				// every element the pattern does not list
				AstNode *dflt = nullptr;
				int listed = 0;
				for (auto &c : pat->children) {
					auto it = c->attributes.find(ID(sv_member));
					std::string key = it != c->attributes.end() && it->second ? it->second->str : "";
					if (key == "default" || key == "\\default")
						dflt = c.get();
					else
						listed++;
				}
				// the element layout, typedef'd struct members resolved
				std::unique_ptr<AstNode> st = struct_has_unresolved_members(etype) ? resolve_struct_layout(etype) : etype->clone();
				if (!st)
					input_error("Struct type of parameter array %s could not be laid out.\n", log_id(str));
				size_packed_struct(st.get(), 0);
				// the member path (`session_cfg.mode`, kmac AppCfg) through
				// nested structs; nested member ranges are element offsets
				std::string mname = children[1]->str;
				if (!mname.empty() && mname[0] == '\\')
					mname = mname.substr(1);
				AstNode *item = st.get();
				{
					size_t p0 = 0;
					while (item) {
						size_t dot = mname.find('.', p0);
						std::string seg = mname.substr(p0, dot == std::string::npos ? std::string::npos : dot - p0);
						AstNode *next = nullptr;
						for (auto &cand : item->children) {
							std::string cn = cand->str;
							if (!cn.empty() && cn[0] == '\\')
								cn = cn.substr(1);
							if (cn == seg)
								next = cand.get();
						}
						item = next;
						if (dot == std::string::npos)
							break;
						p0 = dot + 1;
					}
				}
				if (!item)
					input_error("Struct type of parameter array %s has no member `%s'.\n", log_id(str), mname.c_str());
				// the member of element `pos` as a constant (nullptr if the
				// element does not fold)
				auto elem_member = [&](int pos) -> std::unique_ptr<AstNode> {
					AstNode *src_elem = pos < listed ? pat->children[pos].get() : dflt;
					if (!src_elem)
						return nullptr;
					std::unique_ptr<AstNode> elem = src_elem->clone();
					elem->attributes.erase(ID(sv_member));
					if (elem->type == AST_ASSIGN_PATTERN) {
						auto tmp = std::make_unique<AstNode>(location, AST_LOCALPARAM, std::move(elem));
						tmp->str = "\\$member_element";
						tmp->set_attribute(ID::wiretype, etype->clone());
						while (tmp->simplify(true, 1, -1, false)) { }
						elem = tmp->children[0]->clone();
					} else {
						while (elem->simplify(true, 1, -1, false)) { }
					}
					if (elem->type != AST_CONSTANT || !item->range_valid)
						return nullptr;
					int sw = st->range_left - st->range_right + 1;
					std::vector<RTLIL::State> bits = elem->bitsAsConst(sw, false).to_bits();
					int lo = std::min(item->range_left, item->range_right) - st->range_right;
					int hi = std::max(item->range_left, item->range_right) - st->range_right;
					std::vector<RTLIL::State> v(bits.begin() + lo, bits.begin() + hi + 1);
					return mkconst_bits(location, v, false);
				};
				if (idx->type == AST_CONSTANT) {
					int index = idx->asInt(true);
					int pos = ascending ? index - base : base - index;
					if (pos < 0 || pos >= dm.range_width)
						input_error("Index %d of parameter array %s is out of range.\n", index, log_id(str));
					if (pos >= listed && !dflt)
						input_error("Index %d of parameter array %s has no element in its value.\n", index, log_id(str));
					if (auto v = elem_member(pos)) {
						newNode = std::move(v);
						goto apply_newNode;
					}
				} else {
					// a data-dependent index (`AppCfg[arb_idx].session_cfg.mode`):
					// select among the elements' constant members; an index
					// outside the array reads x (IEEE 1800-2017 7.4.6)
					int mw = std::abs(item->range_left - item->range_right) + 1;
					std::unique_ptr<AstNode> sel = mkconst_bits(location, std::vector<RTLIL::State>(mw, RTLIL::State::Sx), false);
					bool ok = true;
					for (int pos = dm.range_width - 1; pos >= 0 && ok; pos--) {
						auto v = elem_member(pos);
						if (!v) {
							ok = false;
							break;
						}
						int index = ascending ? base + pos : base - pos;
						auto eq = std::make_unique<AstNode>(location, AST_EQ, idx->clone(), mkconst_int(location, index, true));
						sel = std::make_unique<AstNode>(location, AST_TERNARY, std::move(eq), std::move(v), std::move(sel));
					}
					if (ok) {
						newNode = std::move(sel);
						goto apply_newNode;
					}
				}
			}
		}
	}

	// resolve constant prefixes (IEEE hierarchical names use constant indices)
	if (type == AST_PREFIX && children[0]->type == AST_CONSTANT) {
		if (children[1]->type == AST_PREFIX)
			children[1]->simplify(const_fold, stage, width_hint, sign_hint);
		if (children[1]->type == AST_IDENTIFIER) {
			newNode = children[1]->clone();
			const char *second_part = children[1]->str.c_str();
			if (second_part[0] == '\\')
				second_part++;
			newNode->str = stringf("%s[%d].%s", str, children[0]->integer, second_part);
			goto apply_newNode;
		}
	}

	// evaluate TO_BITS nodes
	if (type == AST_TO_BITS) {
		if (children[0]->type != AST_CONSTANT)
			input_error("Left operand of to_bits expression is not constant!\n");
		if (children[1]->type != AST_CONSTANT)
			input_error("Right operand of to_bits expression is not constant!\n");
		RTLIL::Const new_value = children[1]->bitsAsConst(children[0]->bitsAsConst().as_int(), children[1]->is_signed);
		newNode = mkconst_bits(location, new_value.to_bits(), children[1]->is_signed);
		goto apply_newNode;
	}

	// annotate constant ranges
	if (type == AST_RANGE) {
		for (auto &child : children) {
			if (child->type == AST_CONSTANT)
				continue;
			AstNode *missing = find_missing_autowire(child.get());
			if (!missing)
				continue;
			if (missing->str.find("::") == std::string::npos && current_ast_mod &&
					name_declared_in(current_ast_mod, missing->str))
				continue;
			degrade(child->location.to_loc(), "Range bound uses implicitly declared identifier `%s' from a missing package; using constant 0.\n", missing->str.c_str());
			child = mkconst_int(child->location, 0, true);
			fixup_hierarchy_flags();
			did_something = true;
		}
		bool old_range_valid = range_valid;
		range_valid = false;
		range_swapped = false;
		range_left = -1;
		range_right = 0;
		log_assert(children.size() >= 1);
		if (children[0]->type == AST_CONSTANT) {
			range_valid = true;
			range_left = children[0]->integer;
			if (children.size() == 1)
				range_right = range_left;
		}
		if (children.size() >= 2) {
			if (children[1]->type == AST_CONSTANT)
				range_right = children[1]->integer;
			else
				range_valid = false;
		}
		if (old_range_valid != range_valid)
			did_something = true;
		if (range_valid && range_right > range_left) {
			std::swap(range_left, range_right);
			range_swapped = true;
		}
	}

	// annotate wires with their ranges
	if (type == AST_WIRE) {
		if (children.size() > 0) {
			if (children[0]->range_valid) {
				if (!range_valid)
					did_something = true;
				range_valid = true;
				range_swapped = children[0]->range_swapped;
				range_left = children[0]->range_left;
				range_right = children[0]->range_right;
				bool force_upto = false, force_downto = false;
				if (attributes.count(ID::force_upto)) {
					auto* val = attributes[ID::force_upto].get();
					if (val->type != AST_CONSTANT)
						input_error("Attribute `force_upto' with non-constant value!\n");
					force_upto = val->asAttrConst().as_bool();
				}
				if (attributes.count(ID::force_downto)) {
					auto* val = attributes[ID::force_downto].get();
					if (val->type != AST_CONSTANT)
						input_error("Attribute `force_downto' with non-constant value!\n");
					force_downto = val->asAttrConst().as_bool();
				}
				if (force_upto && force_downto)
					input_error("Attributes `force_downto' and `force_upto' cannot be both set!\n");
				if ((force_upto && !range_swapped) || (force_downto && range_swapped)) {
					std::swap(range_left, range_right);
					range_swapped = force_upto;
				}
				if (range_left == range_right && !attributes.count(ID::single_bit_vector))
					set_attribute(ID::single_bit_vector, mkconst_int(location, 1, false));
			} else if (children[0]->type == AST_REALVALUE) {
				// real-typed variable: the REALVALUE child only marks the
				// declaration as real, it carries no range.  Back the signal
				// with a 32-bit wire; real values assigned to it are
				// converted via realAsConst like any other real expression
				// (simulation-only degradation, IEEE 6.12).
				if (!range_valid)
					did_something = true;
				range_valid = true;
				range_swapped = false;
				range_left = 31;
				range_right = 0;
			}
		} else {
			if (!range_valid)
				did_something = true;
			range_valid = true;
			range_swapped = false;
			range_left = 0;
			range_right = 0;
			attributes.erase(ID::single_bit_vector);
		}
	}

	// Resolve packed and unpacked ranges in declarations.
	if ((type == AST_WIRE || type == AST_MEMORY) && dimensions.empty()) {
		if (!children.empty()) {
			// Unpacked ranges first, then packed ranges.
			for (int i = std::min(GetSize(children), 2) - 1; i >= 0; i--) {
				if (children[i]->type == AST_MULTIRANGE) {
					int width = 1;
					for (auto& range : children[i]->children) {
						while (range->simplify(true, stage, -1, false)) { }
						width *= add_dimension(this, range.get());
						if (i) unpacked_dimensions++;
					}
					int left = width - 1, right = 0;
					if (i)
						std::swap(left, right);
					auto loc = children[i]->location;
					children[i] = std::make_unique<AstNode>(loc, AST_RANGE, mkconst_int(loc, left, true), mkconst_int(loc, right, true));
					fixup_hierarchy_flags();
					did_something = true;
				} 				else if (children[i]->type == AST_RANGE) {
					// Fold genvar and parameter bounds before recording the width.
					while (children[i]->simplify(true, stage, -1, false)) { }
					add_dimension(this, children[i].get());
					if (i) unpacked_dimensions++;
				}
				else if (children[i]->type == AST_REALVALUE) {
					// real-typed declaration: the REALVALUE child marks the
					// packed type as real; record the fixed 32-bit backing
					// width (matches the width detectSignWidth uses for
					// real expressions).
					if (i == 0) {
						dimensions.push_back({ 0, 32, false });
						did_something = true;
					}
				}
			}
		} else {
			// 1 bit signal: bit, logic or reg
			dimensions.push_back({ 0, 1, false });
		}
	}

	// Resolve multidimensional array access.
	// a packed multi-dimensional parameter is also reached through
	// current_scope before id2ast is set (constant selects fold early)
	{
	AstNode *mdim_param = nullptr;
	if (type == AST_IDENTIFIER && !basic_prep) {
		AstNode *decl = id2ast ? id2ast : (current_scope.count(str) ? current_scope.at(str) : nullptr);
		if (decl && (decl->type == AST_PARAMETER || decl->type == AST_LOCALPARAM) && GetSize(decl->dimensions) > 1 &&
				decl->unpacked_dimensions == 0)
			mdim_param = decl;
	}
	if (type == AST_IDENTIFIER && !basic_prep && ((id2ast && (id2ast->type == AST_WIRE || id2ast->type == AST_MEMORY)) || mdim_param) &&
		children.size() > 0 && (children[0]->type == AST_RANGE || children[0]->type == AST_MULTIRANGE))
	{
		AstNode *decl_node = mdim_param ? mdim_param : id2ast;
		int dims_sel = children[0]->type == AST_MULTIRANGE ? children[0]->children.size() : 1;
		// Save original number of dimensions for $size() etc.
		integer = dims_sel;
		// a row of a 2-D unpacked memory (`m[k]` of `m [A][B]`): keep the
		// row index; the flattening below loses it, and a cell connection
		// of the row is expanded from it (rewrite_whole_array_refs)
		if (decl_node->type == AST_MEMORY && decl_node->unpacked_dimensions == 2 && dims_sel == 1 &&
				children[0]->type == AST_RANGE && children[0]->children.size() == 1)
			set_attribute(ID(sv_subarray_row), children[0]->children[0]->clone());

		// Split access into unpacked and packed parts.
		std::unique_ptr<AstNode> unpacked_range = nullptr;
		std::unique_ptr<AstNode> packed_range = nullptr;

		if (decl_node->unpacked_dimensions) {
			if (decl_node->unpacked_dimensions > 1) {
				// Flattened range for access to unpacked dimensions.
				unpacked_range = make_index_range(decl_node, true);
			} else {
				// Index into one-dimensional unpacked part; unlink simple range node.
				auto& range = children[0]->type == AST_MULTIRANGE ? children[0]->children[0] : children[0];
				unpacked_range = std::move(range);
				range = nullptr;
			}
		}

		if (dims_sel > decl_node->unpacked_dimensions) {
			if (GetSize(decl_node->dimensions) - decl_node->unpacked_dimensions > 1) {
				// Flattened range for access to packed dimensions.
				packed_range = make_index_range(decl_node, false);
			} else {
				// Index into one-dimensional packed part; unlink simple range node.
				auto& range = children[0]->type == AST_MULTIRANGE ? children[0]->children[dims_sel - 1] : children[0];
				packed_range = std::move(range);
				range = nullptr;
			}
		}

		children.clear();

		if (unpacked_range)
			children.push_back(std::move(unpacked_range));

		if (packed_range)
			children.push_back(std::move(packed_range));

		fixup_hierarchy_flags();
		basic_prep = true;
		did_something = true;
	}
	}

	// trim/extend parameters
	if (type == AST_PARAMETER || type == AST_LOCALPARAM || type == AST_ENUM_ITEM) {
		// unpacked dimensions of a parameter array, recorded first (the
		// wire convention): `[n]` is [0:n-1], `[l:r]` keeps its direction
		// (a struct-typed array already carries its packed dimension here:
		// the unpacked ones go IN FRONT of it, as for wires -- they used to be
		// skipped, so `P[1]` on `parameter c_t P [4]` read a bit)
		if (attributes.count(ID(sv_unpacked_dims)) && unpacked_dimensions == 0) {
			AstNode *un = attributes.at(ID(sv_unpacked_dims)).get();
			std::vector<AstNode*> ranges;
			if (un->type == AST_MULTIRANGE)
				for (auto &r : un->children)
					ranges.push_back(r.get());
			else
				ranges.push_back(un);
			bool all_valid = true;
			std::vector<dimension_t> dims;
			for (auto r : ranges) {
				for (auto &c : r->children)
					while (c->simplify(true, stage, -1, false)) { }
				if (r->children.size() == 1 && r->children[0]->type == AST_CONSTANT) {
					int n = r->children[0]->asInt(true);
					if (n < 1)
						input_error("Unpacked dimension [%d] of parameter %s is empty.\n", n, str.c_str());
					dims.push_back({ 0, n, true });
				} else if (r->children.size() == 2 && r->children[0]->type == AST_CONSTANT && r->children[1]->type == AST_CONSTANT) {
					int l = r->children[0]->asInt(true), rr = r->children[1]->asInt(true);
					dims.push_back({ std::min(l, rr), std::abs(l - rr) + 1, l < rr });
				} else
					all_valid = false;
			}
			if (all_valid) {
				dims.insert(dims.end(), dimensions.begin(), dimensions.end());
				dimensions = dims;
				unpacked_dimensions = GetSize(ranges);
				attributes.erase(ID(sv_unpacked_dims));
				did_something = true;
			}
		}
		// unpacked-array parameter values that are not a literal pattern:
		//  - a default naming another array parameter (`= ibex_pkg::PmpCfgRst`)
		//    takes that parameter's pattern, so element reads fold;
		//  - an instance override arrives (derive) as ONE flat constant,
		//    element 0 in the most significant bits (see the AST_CELL
		//    flattening): split it back into a pattern of elements.
		// a replication pattern `'{N{v}}` (OpenTitan's RaclPolicySelVec
		// default `'{NumRegs{0}}`): expand it to N elements so element reads
		// and pass-through flattening see one child per element
		if (unpacked_dimensions > 0 && !children.empty() && children[0]->type == AST_ASSIGN_PATTERN &&
				children[0]->children.size() == 1 && children[0]->children[0]->type == AST_REPLICATE &&
				children[0]->children[0]->children.size() == 2) {
			AstNode *rep = children[0]->children[0].get();
			auto cnt = rep->children[0]->clone();
			while (cnt->simplify(true, stage, -1, false)) { }
			if (cnt->type == AST_CONSTANT) {
				int n = cnt->asInt(false);
				AstNode *inner = rep->children[1].get();
				std::vector<AstNode *> elems;
				if (inner->type == AST_CONCAT)
					for (auto &c : inner->children)
						elems.push_back(c.get());
				else
					elems.push_back(inner);
				if (n >= 1 && n <= (1 << 16) && !elems.empty()) {
					auto pat = std::make_unique<AstNode>(children[0]->location, AST_ASSIGN_PATTERN);
					for (int i = 0; i < n; i++)
						for (auto e : elems)
							pat->children.push_back(e->clone());
					children[0] = std::move(pat);
					did_something = true;
				}
			}
		}
		// a PACKED multi-dim parameter (`parameter logic [15:0][3:0] P =
		// '{4'hF, 4'hA, ...}`, OpenTitan prim_cipher_pkg PRINCE_SHIFT_ROWS64)
		// with a positional pattern: for a packed array that is the
		// concatenation of the elements, leftmost element in the MSBs, each
		// sized to the element width.  The pattern never folded to a
		// constant (every read degraded: prim_prince, sram_ctrl FAIL@parse).
		// Keyed / default patterns keep the loud path.
		// (a whole-struct parameter, whose wiretype attribute is the struct
		// itself, is folded member by member above; its `dimensions` may
		// describe nested members and must not be read as array dimensions)
		bool whole_struct = attributes.count(ID::wiretype) &&
				(attributes.at(ID::wiretype)->type == AST_STRUCT || attributes.at(ID::wiretype)->type == AST_UNION);
		if (unpacked_dimensions == 0 && GetSize(dimensions) >= 2 && !children.empty() && !whole_struct &&
				children[0]->type == AST_ASSIGN_PATTERN) {
			auto bare = [](std::string s) {
				if (!s.empty() && s[0] == '\\')
					s = s.substr(1);
				return s;
			};
			// an element that is itself a struct pattern (`localparam
			// range_t [0:0] R = '{ '{base: .., limit: ..} }`, OpenTitan
			// RACL ranges): its members in declaration order, MSB first,
			// each sized to the member; named or positional, no default
			std::function<std::unique_ptr<AstNode>(AstNode *, AstNode *)> struct_val = [&](AstNode *pat, AstNode *st) -> std::unique_ptr<AstNode> {
				if (!st || st->type != AST_STRUCT || pat->type != AST_ASSIGN_PATTERN)
					return nullptr;
				std::vector<AstNode *> items;
				for (auto &it : st->children)
					if (it && (it->type == AST_STRUCT_ITEM || it->type == AST_STRUCT))
						items.push_back(it.get());
				bool named = false;
				dict<std::string, AstNode *> by_name;
				for (auto &c : pat->children) {
					if (c->attributes.count(ID(sv_typekey)) || c->attributes.count(ID(sv_index)))
						return nullptr;
					if (c->attributes.count(ID(sv_member))) {
						named = true;
						std::string key = bare(c->attributes.at(ID(sv_member))->str);
						if (key == "default")
							return nullptr;
						by_name[key] = c.get();
					}
				}
				if (!named && GetSize(pat->children) != GetSize(items))
					return nullptr;
				auto cat = std::make_unique<AstNode>(pat->location, AST_CONCAT);
				for (int k = GetSize(items) - 1; k >= 0; k--) {
					AstNode *item = items[k];
					AstNode *src = named ? (by_name.count(bare(item->str)) ? by_name.at(bare(item->str)) : nullptr)
							: pat->children[k].get();
					if (!src || !item->range_valid)
						return nullptr;
					int w = std::abs(item->range_left - item->range_right) + 1;
					std::unique_ptr<AstNode> v;
					if (src->type == AST_ASSIGN_PATTERN) {
						AstNode *nested = item->type == AST_STRUCT ? item : nullptr;
						if (!nested)
							for (auto &ch : item->children)
								if (ch && ch->type == AST_WIRETYPE && current_scope.count(ch->str) &&
										current_scope.at(ch->str)->type == AST_TYPEDEF &&
										!current_scope.at(ch->str)->children.empty())
									nested = current_scope.at(ch->str)->children[0].get();
						v = struct_val(src, nested);
						if (!v)
							return nullptr;
					} else {
						v = src->clone();
						v->attributes.erase(ID(sv_member));
					}
					cat->children.push_back(std::make_unique<AstNode>(src->location, AST_CAST_SIZE,
							mkconst_int(src->location, w, true), std::move(v)));
				}
				return cat;
			};
			AstNode *elem_struct = resolve_wiretype_struct(this);
			std::function<std::unique_ptr<AstNode>(AstNode *, int)> flat = [&](AstNode *pat, int d) -> std::unique_ptr<AstNode> {
				if (d >= GetSize(dimensions) - 1 || pat->type != AST_ASSIGN_PATTERN)
					return nullptr;
				if (GetSize(pat->children) != dimensions[d].range_width)
					return nullptr;
				int ew = 1;
				for (int i = d + 1; i < GetSize(dimensions); i++)
					ew *= dimensions[i].range_width;
				// AST_CONCAT keeps its children LSB first (reverse source
				// order); a pattern lists them in source order
				auto cat = std::make_unique<AstNode>(pat->location, AST_CONCAT);
				for (auto it = pat->children.rbegin(); it != pat->children.rend(); ++it) {
					auto &e = *it;
					if (e->attributes.count(ID(sv_member)))
						return nullptr;
					std::unique_ptr<AstNode> v;
					if (e->type == AST_ASSIGN_PATTERN) {
						v = d + 1 < GetSize(dimensions) - 1 ? flat(e.get(), d + 1) : struct_val(e.get(), elem_struct);
						if (!v)
							return nullptr;
					} else
						v = e->clone();
					cat->children.push_back(std::make_unique<AstNode>(e->location, AST_CAST_SIZE,
							mkconst_int(e->location, ew, true), std::move(v)));
				}
				return cat;
			};
			if (auto cat = flat(children[0].get(), 0)) {
				children[0] = std::move(cat);
				did_something = true;
			}
		}
		// `'{default: v}` (possibly after explicit elements) for a 1-D
		// unpacked array: list every element, so element reads and
		// pass-through flattening see one child per element
		if (unpacked_dimensions == 1 && !children.empty() && children[0]->type == AST_ASSIGN_PATTERN) {
			AstNode *pat = children[0].get();
			AstNode *dflt = nullptr;
			std::vector<AstNode *> listed;
			bool keyed = false;
			for (auto &c : pat->children) {
				auto it = c->attributes.find(ID(sv_member));
				std::string key = it != c->attributes.end() && it->second ? it->second->str : "";
				if (key == "default" || key == "\\default")
					dflt = c.get();
				else if (!key.empty())
					keyed = true;
				else
					listed.push_back(c.get());
			}
			int n = dimensions[0].range_width;
			if (dflt && !keyed && GetSize(listed) <= n) {
				auto npat = std::make_unique<AstNode>(pat->location, AST_ASSIGN_PATTERN);
				for (int i = 0; i < n; i++) {
					auto e = (i < GetSize(listed) ? listed[i] : dflt)->clone();
					e->attributes.erase(ID(sv_member));
					npat->children.push_back(std::move(e));
				}
				children[0] = std::move(npat);
				did_something = true;
			}
		}
		if (unpacked_dimensions > 0 && !children.empty() && children[0]->type == AST_IDENTIFIER &&
				children[0]->children.empty()) {
			AstNode *src = children[0]->id2ast ? children[0]->id2ast :
					current_scope.count(children[0]->str) ? current_scope.at(children[0]->str) : nullptr;
			if (src && src != this && (src->type == AST_PARAMETER || src->type == AST_LOCALPARAM) &&
					!src->children.empty() && src->children[0]->type == AST_ASSIGN_PATTERN) {
				children[0] = src->children[0]->clone();
				did_something = true;
			}
		}
		if (unpacked_dimensions > 0 && !children.empty() && children[0]->type == AST_CONSTANT &&
				!children[0]->is_string) {
			int count = 1;
			for (int d = 0; d < unpacked_dimensions; d++)
				count *= dimensions[d].range_width;
			int total = GetSize(children[0]->bits);
			if (count > 1 && total > 0 && total % count == 0) {
				int ew = total / count;
				std::vector<RTLIL::State> bits = children[0]->bits;
				std::function<std::unique_ptr<AstNode>(int, int)> build = [&](int d, int first) -> std::unique_ptr<AstNode> {
					// elements [first, first + span) in source order
					if (d == unpacked_dimensions) {
						int off = (count - 1 - first) * ew;
						std::vector<RTLIL::State> v(bits.begin() + off, bits.begin() + off + ew);
						return mkconst_bits(location, v, false);
					}
					int span = 1;
					for (int k = d + 1; k < unpacked_dimensions; k++)
						span *= dimensions[k].range_width;
					auto pat = std::make_unique<AstNode>(location, AST_ASSIGN_PATTERN);
					for (int i = 0; i < dimensions[d].range_width; i++)
						pat->children.push_back(build(d + 1, first + i * span));
					return pat;
				};
				children[0] = build(0, 0);
				did_something = true;
			}
		}
		// packed multi-dimensional parameter (`parameter logic [3:0][1:0] P`):
		// record its dimensions like a wire's and flatten the range, so that
		// `P[k]` selects an element, not bit k
		if (children.size() > 1 && children[1]->type == AST_MULTIRANGE && unpacked_dimensions == 0 &&
				!attributes.count(ID(sv_unpacked_dims)) && GetSize(dimensions) == 0) {
			bool all_valid = true;
			for (auto &range : children[1]->children) {
				while (range->simplify(true, stage, -1, false)) { }
				if (!range->range_valid)
					all_valid = false;
			}
			if (all_valid) {
				int width = 1;
				for (auto &range : children[1]->children)
					width *= add_dimension(this, range.get());
				children[1] = make_range(children[1]->location, width - 1, 0);
				fixup_hierarchy_flags();
				did_something = true;
			}
		}
		if (children.size() > 1 && children[1]->type == AST_RANGE) {
			if (!children[1]->range_valid)
				input_error("Non-constant width range on parameter decl.\n");
			int width = std::abs(children[1]->range_left - children[1]->range_right) + 1;
			if (children[0]->type == AST_REALVALUE) {
				RTLIL::Const constvalue = children[0]->realAsConst(width);
				log_file_warning(location.to_loc(), "converting real value %e to binary %s.\n",
						children[0]->realvalue, log_signal(constvalue));
				children[0] = mkconst_bits(location, constvalue.to_bits(), sign_hint);
				fixup_hierarchy_flags();
				did_something = true;
			}
			if (children[0]->type == AST_CONSTANT) {
				if (width != int(children[0]->bits.size())) {
					RTLIL::Const val;
					if (children[0]->is_unsized) {
						val = children[0]->bitsAsUnsizedConst(width);
					} else {
						val = children[0]->bitsAsConst(width);
					}
					children[0] = mkconst_bits(location, val.to_bits(), is_signed);
					fixup_hierarchy_flags();
				}
				children[0]->is_signed = is_signed;
			}
			range_valid = true;
			range_swapped = children[1]->range_swapped;
			range_left = children[1]->range_left;
			range_right = children[1]->range_right;
		} else
		if (children.size() > 1 && children[1]->type == AST_REALVALUE && children[0]->type == AST_CONSTANT) {
			double as_realvalue = children[0]->asReal(sign_hint);
			children[0] = std::make_unique<AstNode>(location, AST_REALVALUE);
			children[0]->realvalue = as_realvalue;
			fixup_hierarchy_flags();
			did_something = true;
		}
	}

	if (type == AST_IDENTIFIER && !basic_prep) {
		// `pkg::P.member` on a struct-typed package parameter: the members
		// were registered in the package under `P.member` only; register
		// them under the qualified name the module sees (register v3 D13:
		// cva6 `cva6_config_pkg::cva6_cfg.XLEN` autowired, a silent 0).
		// The same for an imported `P.member` (`import tlul_pkg::*;
		// TL_A_USER_DEFAULT.data_intg`): the import brings in `P` alone
		if (!str.empty() && str[0] == '\\' && !current_scope.count(str) && str.find('.') != std::string::npos) {
			size_t q = str.rfind("::");
			size_t dot = str.find('.', q == std::string::npos ? 0 : q);
			if (dot != std::string::npos) {
				std::string pname = str.substr(0, dot);
				if (current_scope.count(pname)) {
					AstNode *pn = current_scope.at(pname);
					if ((pn->type == AST_PARAMETER || pn->type == AST_LOCALPARAM) && pn->attributes.count(ID::wiretype)) {
						// the type is the struct node or its (qualified) name
						AstNode *st = resolve_wiretype_struct(pn);
						if (st && (st->type == AST_STRUCT || st->type == AST_UNION)) {
							if (!st->range_valid)
								size_packed_struct(st, 0);
							add_members_to_scope(st, pname);
						}
					}
				}
			}
		}
		// check if a plausible struct member sss.mmmm
		if (is_user_scope_name(str) && current_scope.count(str)) {
			auto item_node = current_scope[str];
			if (item_node->type == AST_STRUCT_ITEM || item_node->type == AST_STRUCT || item_node->type == AST_UNION) {
				// Traverse any hierarchical path until the full name for the referenced struct/union is found.
				std::string sname;
				bool found_sname = false;
				for (std::string::size_type pos = 0; (pos = str.find('.', pos)) != std::string::npos; pos++) {
					sname = str.substr(0, pos);
					if (current_scope.count(sname)) {
						auto stype = current_scope[sname]->type;
						if (stype == AST_WIRE || stype == AST_PARAMETER || stype == AST_LOCALPARAM) {
							found_sname = true;
							break;
						}
					}
				}

				if (found_sname) {
					// structure member, rewrite this node to reference the packed struct wire
					auto range = make_index_range(item_node);
					newNode = std::make_unique<AstNode>(location, AST_IDENTIFIER, std::move(range));
					newNode->str = sname;
					// save type and original number of dimensions for $size() etc.
					newNode->set_attribute(ID::wiretype, item_node->clone());
					// an element member select `s.x[i].m` keeps the member's type
					for (auto id : {ID(sv_elem_member), ID(sv_elem_member_sel)})
						if (attributes.count(id))
							newNode->set_attribute(id, attributes.at(id)->clone());
					if (!item_node->dimensions.empty() && children.size() > 0) {
						if (children[0]->type == AST_RANGE)
							newNode->integer = 1;
						else if (children[0]->type == AST_MULTIRANGE)
							newNode->integer = children[0]->children.size();
					}
					newNode->basic_prep = true;
					if (item_node->is_signed)
						newNode = std::make_unique<AstNode>(location, AST_TO_SIGNED, std::move(newNode));
					goto apply_newNode;
				}
			}
		}
	}
	// annotate identifiers using scope resolution and create auto-wires as needed
	if (type == AST_IDENTIFIER) {
		if (current_scope.count(str) == 0 && id2ast && id2ast->type != AST_AUTOWIRE) {
			// This identifier was already resolved to a real declaration in an
			// earlier simplify pass. Re-simplification can run with an empty or
			// partial scope (e.g. sign/width detection during genrtlil, after
			// the module scope was cleared); that must not turn the name into
			// an implicit net and stomp the existing binding.
			current_scope[str] = id2ast;
		}
		if (current_scope.count(str) == 0) {
			AstNode *current_scope_ast = (current_ast_mod == nullptr) ? current_ast : current_ast_mod;
			str = try_pop_module_prefix();
			for (auto& node : current_scope_ast->children) {
				if (!node)
					continue;
				//log("looking at mod scope child %s\n", type2str(node->type));
				switch (node->type) {
				case AST_PARAMETER:
				case AST_LOCALPARAM:
				case AST_WIRE:
				case AST_AUTOWIRE:
				case AST_GENVAR:
				case AST_MEMORY:
				case AST_FUNCTION:
				case AST_TASK:
				case AST_DPI_FUNCTION:
				case AST_TYPEDEF:
					//log("found child %s, %s\n", type2str(node->type), node->str);
					if (str == node->str) {
						//log("add %s, type %s to scope\n", str, type2str(node->type));
						current_scope[node->str] = node.get();
					}
					break;
				case AST_ENUM:
					current_scope[node->str] = node.get();
					for (auto& enum_node : node->children) {
						if (!enum_node)
							continue;
						log_assert(enum_node->type==AST_ENUM_ITEM);
						if (str == enum_node->str) {
							//log("\nadding enum item %s to scope\n", str);
							current_scope[str] = enum_node.get();
						}
					}
					break;
				default:
					break;
				}
			}
		}
		if (current_scope.count(str) == 0 && current_ast) {
			for (auto &node : current_ast->children) {
				if (!node)
					continue;
				if (node->type == AST_TYPEDEF && node->str == str)
					current_scope[str] = node.get();
				if (node->type == AST_PACKAGE) {
					for (auto &inner : node->children)
						if (inner && inner->type == AST_TYPEDEF && inner->str == str)
							current_scope[str] = inner.get();
				}
			}
		}
		if (current_scope.count(str) == 0) {
			bool seq_meth = is_sequence_method_ref(str) || is_sva_decl_ref(str);
			if (current_ast_mod == nullptr) {
				input_error("Identifier `%s' is implicitly declared outside of a module.\n", str);
			} else if (flag_autowire || str == "\\$global_clock" || seq_meth) {
				auto auto_wire = std::make_unique<AstNode>(location, AST_AUTOWIRE);
				auto_wire->str = str;
				current_scope[str] = auto_wire.get();
				current_ast_mod->children.push_back(std::move(auto_wire));
				did_something = true;
			} else {
				// `default_nettype none` (or -noautowire): upstream errors
				// here; -permissive keeps the fork's wire so elaboration of
				// a file with missing packages can finish (marked)
				if (!flag_permissive)
					input_error("Identifier `%s' is implicitly declared and `default_nettype is set to none.\n", str);
				degrade(location.to_loc(), "Identifier `%s' is implicitly declared under `default_nettype none; a 1-bit wire is implied.\n", str.c_str());
				auto auto_wire = std::make_unique<AstNode>(location, AST_AUTOWIRE);
				auto_wire->str = str;
				current_scope[str] = auto_wire.get();
				current_ast_mod->children.push_back(std::move(auto_wire));
				did_something = true;
			}
		}
		if (id2ast != current_scope[str]) {
			id2ast = current_scope[str];
			did_something = true;
		}
	}

	// fold reads of parameter arrays initialized with an assignment
	// pattern, e.g. `localparam int T [5] = '{1,2,3,4,0}; T[2]`.
	// Multi-dimensional arrays (`localparam int R [5][5] = '{'{..},..};
	// R[x][y]`) arrive as one AST_MULTIRANGE child holding one AST_RANGE
	// per index; each index descends one level into the nested pattern.
	// A non-constant index (`RC[rnd]`) becomes a ternary chain over the
	// elements, and a trailing packed part-select on a leaf constant
	// (`RC[rnd][W-1:0]`) is sliced out of each leaf.  Without this the
	// read stayed symbolic: a generate-if over it was silently skipped
	// (prim_keccak rho offsets) and a dynamic index read an undriven
	// wire (prim_keccak round constants).
	if (type == AST_IDENTIFIER && id2ast &&
			(id2ast->type == AST_LOCALPARAM || id2ast->type == AST_PARAMETER) &&
			children.size() == 1 &&
			(children[0]->type == AST_RANGE || children[0]->type == AST_MULTIRANGE) &&
			!id2ast->children.empty() && id2ast->children[0]->type == AST_ASSIGN_PATTERN) {
		std::vector<AstNode*> sels;
		if (children[0]->type == AST_RANGE)
			sels.push_back(children[0].get());
		else
			for (auto &c : children[0]->children)
				sels.push_back(c.get());
		// packed slice of a leaf constant: bits are stored LSB-first with
		// bit 0 at the parameter's packed range_right
		std::function<std::unique_ptr<AstNode>(AstNode*, size_t)> resolve;
		resolve = [&](AstNode *cur, size_t d) -> std::unique_ptr<AstNode> {
			if (d == sels.size())
				return cur->clone();
			AstNode *sel = sels[d];
			if (sel->type != AST_RANGE || sel->children.empty())
				return nullptr;
			if (cur->type == AST_ASSIGN_PATTERN) {
				// pattern item 0 is the declared LEFT bound: [lo:hi] counts
				// up from lo, [hi:lo] down from hi (no recorded dimension:
				// [0:n-1])
				int base = 0;
				bool ascending = true;
				if (d < (size_t)id2ast->unpacked_dimensions) {
					auto &dm = id2ast->dimensions[d];
					ascending = dm.range_swapped;
					base = ascending ? dm.range_right : dm.range_right + dm.range_width - 1;
					if (dm.range_width != GetSize(cur->children))
						return nullptr;
				}
				auto pos_of = [&](int index) { return ascending ? index - base : base - index; };
				auto index_of = [&](int pos) { return ascending ? base + pos : base - pos; };
				if (sel->range_valid) {
					if (sels.size() > 1 && sel->range_left != sel->range_right)
						return nullptr;
					int idx = pos_of(sel->range_left);
					if (idx < 0 || idx >= GetSize(cur->children))
						return nullptr;
					return resolve(cur->children[idx].get(), d + 1);
				}
				if (sel->children.size() != 1)
					return nullptr;
				std::vector<std::unique_ptr<AstNode>> leaves;
				int w = -1;
				bool all_const = true;
				for (auto &child : cur->children) {
					auto leaf = resolve(child.get(), d + 1);
					if (!leaf)
						return nullptr;
					if (leaf->type != AST_CONSTANT || (w >= 0 && GetSize(leaf->bits) != w))
						all_const = false;
					else
						w = GetSize(leaf->bits);
					leaves.push_back(std::move(leaf));
				}
				auto idx = sel->children[0]->clone();
				if (!ascending)
					idx = std::make_unique<AstNode>(location, AST_SUB, mkconst_int(location, base, true), std::move(idx));
				else if (base != 0)
					idx = std::make_unique<AstNode>(location, AST_SUB, std::move(idx), mkconst_int(location, base, true));
				if (all_const && w > 0 && !in_param) {
					// dynamic index into a constant table: one $shiftx over
					// the flattened table (element i at bits [i*w +: w]),
					// cast back to the element width.  A ternary chain here
					// is O(n) muxes per read and made the AES S-box
					// lookups in caliptra's fv_doe_* properties (256
					// entries, called per byte per round) take minutes.
					std::vector<RTLIL::State> flat;
					for (auto &leaf : leaves)
						flat.insert(flat.end(), leaf->bits.begin(), leaf->bits.end());
					bool is_signed = leaves[0]->is_signed;
					auto shamt = std::make_unique<AstNode>(location, AST_MUL, std::move(idx), mkconst_int(location, w, true));
					auto sh = std::make_unique<AstNode>(location, AST_SHIFTX, mkconst_bits(location, flat, false), std::move(shamt));
					auto cast = std::make_unique<AstNode>(location, AST_CAST_SIZE, mkconst_int(location, w, true), std::move(sh));
					cast->is_signed = is_signed;
					return cast;
				}
				// constant context or non-uniform leaves:
				// idx == 0 ? e0 : idx == 1 ? e1 : ... : e_last
				std::unique_ptr<AstNode> chain;
				for (int i = GetSize(leaves) - 1; i >= 0; i--) {
					auto leaf = std::move(leaves[i]);
					if (!chain) {
						chain = std::move(leaf);
						continue;
					}
					auto cond = std::make_unique<AstNode>(location, AST_EQ,
							sel->children[0]->clone(), mkconst_int(location, index_of(i), true));
					chain = std::make_unique<AstNode>(location, AST_TERNARY,
							std::move(cond), std::move(leaf), std::move(chain));
				}
				return chain;
			}
			// leaf: only a single constant packed part-select is supported
			if (cur->type != AST_CONSTANT || d + 1 != sels.size() || !sel->range_valid)
				return nullptr;
			if (!id2ast->range_valid || id2ast->range_swapped)
				return nullptr;
			int lo = std::min(sel->range_left, sel->range_right) - id2ast->range_right;
			int hi = std::max(sel->range_left, sel->range_right) - id2ast->range_right;
			if (lo < 0 || hi >= GetSize(cur->bits))
				return nullptr;
			std::vector<RTLIL::State> bits(cur->bits.begin() + lo, cur->bits.begin() + hi + 1);
			return mkconst_bits(location, bits, false);
		};
		auto folded = resolve(id2ast->children[0].get(), 0);
		if (folded) {
			newNode = std::move(folded);
			goto apply_newNode;
		}
	}

	// split memory access with bit select to individual statements
	if (type == AST_IDENTIFIER && children.size() == 2 && children[0]->type == AST_RANGE && children[1]->type == AST_RANGE && !in_lvalue && stage == 2)
	{
		if (id2ast == nullptr)
			input_error("Invalid bit-select on memory access!\n");
		if (id2ast->type != AST_MEMORY) {
			// Unpacked array ports and wire arrays take the multidimensional
			// array access path below instead.
			if (id2ast->type != AST_WIRE || id2ast->unpacked_dimensions == 0)
				input_error("Invalid bit-select on memory access!\n");
		} else {
		if (children[0]->children.size() != 1)
			input_error("Invalid bit-select on memory access!\n");

		int mem_width, mem_size, addr_bits;
		id2ast->meminfo(mem_width, mem_size, addr_bits);

		int data_range_left = id2ast->children[0]->range_left;
		int data_range_right = id2ast->children[0]->range_right;

		if (id2ast->children[0]->range_swapped)
			std::swap(data_range_left, data_range_right);

		std::stringstream sstr;
		sstr << "$mem2bits$" << str << "$" << RTLIL::encode_filename(*location.begin.filename) << ":" << location.begin.line << "$" << (autoidx++);
		std::string wire_id = sstr.str();

		auto wire_owned = std::make_unique<AstNode>(location, AST_WIRE, std::make_unique<AstNode>(location, AST_RANGE, mkconst_int(location, data_range_left, true), mkconst_int(location, data_range_right, true)));
		auto* wire = wire_owned.get();
		current_ast_mod->children.push_back(std::move(wire_owned));
		wire->str = wire_id;
		if (current_block)
			wire->set_attribute(ID::nosync, AstNode::mkconst_int(location, 1, false));
		while (wire->simplify(true, 1, -1, false)) { }

		auto data = clone();
		data->children.pop_back();

		auto assign = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), std::move(data));
		assign->children[0]->str = wire_id;
		assign->children[0]->was_checked = true;

		if (current_block)
		{
			size_t assign_idx = 0;
			while (assign_idx < current_block->children.size() && current_block->children[assign_idx].get() != current_block_child)
				assign_idx++;
			log_assert(assign_idx < current_block->children.size());
			current_block->children.insert(current_block->children.begin()+assign_idx, std::move(assign));
			wire->is_reg = true;
		}
		else
		{
			auto proc = std::make_unique<AstNode>(location, AST_ALWAYS, std::make_unique<AstNode>(location, AST_BLOCK));
			proc->children[0]->children.push_back(std::move(assign));
			current_ast_mod->children.push_back(std::move(proc));
		}

		newNode = std::make_unique<AstNode>(location, AST_IDENTIFIER, children[1]->clone());
		newNode->str = wire_id;
		newNode->integer = integer; // save original number of dimensions for $size() etc.
		newNode->id2ast = wire;
		goto apply_newNode;
		}
	}

	if (type == AST_WHILE && current_always && current_block && children.size() >= 2 &&
			children[1] && children[1]->type == AST_BLOCK && !children[1]->children.empty()) {
		AstNode *body0 = children[1].get();
		while (body0->type == AST_BLOCK && body0->children.size() == 1 && body0->children[0]->type == AST_BLOCK)
			body0 = body0->children[0].get();
		std::set<std::string> writes, reads;
		std::function<void(AstNode *)> collect_writes = [&](AstNode *n) {
			if (!n)
				return;
			if ((n->type == AST_ASSIGN_EQ || n->type == AST_ASSIGN || n->type == AST_ASSIGN_LE) &&
					n->children.size() >= 2 && n->children[0]->type == AST_IDENTIFIER && n->children[0]->children.empty())
				writes.insert(n->children[0]->str);
			for (auto &ch : n->children)
				collect_writes(ch.get());
		};
		std::function<void(AstNode *)> collect_reads = [&](AstNode *n) {
			if (!n)
				return;
			if (n->type == AST_IDENTIFIER && n->children.empty())
				reads.insert(n->str);
			for (auto &ch : n->children)
				collect_reads(ch.get());
		};
		collect_writes(body0);
		collect_reads(children[0].get());
		std::string iv;
		int matches = 0;
		for (auto &w : writes) {
			if (!reads.count(w))
				continue;
			iv = w;
			matches++;
		}
		int iv_val = 0;
		bool have_init = false;
		if (matches == 1) {
			for (auto &ch : current_block->children) {
				if (ch.get() == current_block_child)
					break;
				if (!ch || ch->children.size() < 2 || ch->children[0]->type != AST_IDENTIFIER || ch->children[0]->str != iv)
					continue;
				if (ch->type != AST_ASSIGN_EQ && ch->type != AST_ASSIGN_LE && ch->type != AST_ASSIGN)
					continue;
				auto rhs = ch->children[1]->clone();
				while (rhs->simplify(true, stage, 32, true)) { }
				if (rhs->type == AST_CONSTANT) {
					iv_val = rhs->asInt(true);
					have_init = true;
				}
			}
		}
		auto subst_iv = [&](AstNode *n, const AstNode *value, bool lvalue) {
			std::function<void(AstNode *, bool)> rec = [&](AstNode *node, bool lv) {
				if (!node)
					return;
				if (node->type == AST_IDENTIFIER && !lv && node->children.empty() && node->str == iv) {
					value->clone()->cloneInto(*node);
					return;
				}
				for (size_t i = 0; i < node->children.size(); i++) {
					bool child_lv = (node->type == AST_ASSIGN || node->type == AST_ASSIGN_EQ || node->type == AST_ASSIGN_LE) && i == 0;
					rec(node->children[i].get(), child_lv);
				}
			};
			rec(n, lvalue);
		};
		bool do_first = get_bool_attribute(ID(sv_do));
		if (have_init) {
			std::vector<std::unique_ptr<AstNode>> emitted;
			bool ok = true;
			bool first = true;
			for (int n = 0; ok && n < 1024; n++) {
				if (!(do_first && first)) {
					auto cond = children[0]->clone();
					auto cur = mkconst_int(location, iv_val, true);
					subst_iv(cond.get(), cur.get(), false);
					while (cond->simplify(true, stage, 1, false)) { }
					if (cond->type != AST_CONSTANT) {
						ok = false;
						break;
					}
					if (!cond->asBool())
						break;
				}
				first = false;
				auto body = body0->clone();
				int cur_iv = iv_val;
				std::vector<std::unique_ptr<AstNode>> done;
				std::function<void(std::vector<std::unique_ptr<AstNode>> &)> walk = [&](std::vector<std::unique_ptr<AstNode>> &stmts) {
					for (auto &st : stmts) {
						if (!st)
							continue;
						if (st->type == AST_BLOCK) {
							walk(st->children);
							continue;
						}
						auto cur = mkconst_int(location, cur_iv, true);
						subst_iv(st.get(), cur.get(), false);
						while (st->simplify(true, stage, -1, false)) { }
						if ((st->type == AST_ASSIGN_EQ || st->type == AST_ASSIGN || st->type == AST_ASSIGN_LE) &&
								st->children.size() >= 2 && st->children[0]->type == AST_IDENTIFIER &&
								st->children[0]->str == iv && st->children[1]->type == AST_CONSTANT)
							cur_iv = st->children[1]->asInt(true);
						done.push_back(std::move(st));
					}
				};
				walk(body->children);
				int exit_kind = trim_uncond_loop_exit(done);
				bool found_next = false;
				int next_val = iv_val;
				std::function<void(AstNode *)> find_next_iv = [&](AstNode *n) {
					if (!n)
						return;
					if ((n->type == AST_ASSIGN_EQ || n->type == AST_ASSIGN || n->type == AST_ASSIGN_LE) &&
							n->children.size() >= 2 && n->children[0]->type == AST_IDENTIFIER &&
							n->children[0]->str == iv && n->children[1]->type == AST_CONSTANT) {
						next_val = n->children[1]->asInt(true);
						found_next = true;
					}
					for (auto &ch : n->children)
						find_next_iv(ch.get());
				};
				for (auto &st : done)
					find_next_iv(st.get());
				for (auto &st : done)
					emitted.push_back(std::move(st));
				if (exit_kind == 1)
					break;
				if (!found_next) {
					ok = false;
					break;
				}
				iv_val = next_val;
			}
			if (ok && current_block) {
				size_t at = 0;
				while (at < current_block->children.size() && current_block->children[at].get() != current_block_child)
					at++;
				size_t from = at;
				for (auto &st : emitted) {
					current_block->children.insert(current_block->children.begin() + at, std::move(st));
					at++;
				}
				absorb_loop_tail(current_block, this, from);
				newNode = std::make_unique<AstNode>(location, AST_BLOCK);
				goto apply_newNode;
			}
		}
	}

	if (type == AST_WHILE) {
		degrade(location.to_loc(), "While loop outside a constant function is left unrolled.\n");
		newNode = std::make_unique<AstNode>(location, AST_BLOCK);
		goto apply_newNode;
	}

	if (type == AST_REPEAT)
	{
		auto count = std::move(children[0]);
		auto body = std::move(children[1]);

		// eval count expression
		while (count->simplify(true, stage, 32, true)) { }

		if (count->type != AST_CONSTANT) {
			degrade(location.to_loc(), "Repeat loop without a constant count is left unrolled.\n");
			type = AST_BLOCK;
			children.clear();
			did_something = true;
		} else {
			int n = count->bitsAsConst().as_int();
			if (n > 1024)
				n = 1024;
			bool returning = contains_function_return(body.get());
			if (returning && current_block) {
				size_t at = 0;
				while (at < current_block->children.size() && current_block->children[at].get() != current_block_child)
					at++;
				size_t from = at;
				for (int i = 0; i < n; i++) {
					auto copy = body->clone();
					current_block->children.insert(current_block->children.begin() + at, std::move(copy));
					at++;
				}
				absorb_loop_tail(current_block, this, from);
				newNode = std::make_unique<AstNode>(location, AST_BLOCK);
				goto apply_newNode;
			}
			// convert to a block with the body repeated n times
			type = AST_BLOCK;
			children.clear();
			for (int i = 0; i < n; i++)
				children.insert(children.begin(), body->clone());
			did_something = true;
		}
	}

	// unroll for loops and generate-for blocks
	if ((type == AST_GENFOR || type == AST_FOR) && children.size() != 0)
	{
		// Extra initializers and step assignments (`for (int i = 0, T t = a;
		// ...; i++, t = f(t))`) are not part of the unrolled index: a
		// procedural loop runs the extra initializers once before the
		// first iteration and the extra steps after each body (they used
		// to be dropped, leaving the extra variable undriven).
		std::vector<std::unique_ptr<AstNode>> extra_inits, extra_steps;
		while (children.size() > 1 && children[0]->type == AST_ASSIGN_EQ && children[1]->type == AST_ASSIGN_EQ) {
			extra_inits.push_back(std::move(children[1]));
			children.erase(children.begin() + 1);
		}
		while (children.size() > 4 && children[3]->type != AST_BLOCK && children[3]->type != AST_GENBLOCK) {
			extra_steps.push_back(std::move(children[3]));
			children.erase(children.begin() + 3);
		}
		if (type == AST_GENFOR) {
			extra_inits.clear();
			extra_steps.clear();
		}

		auto& init_ast = children[0];
		auto& while_ast = children[1];
		auto& next_ast = children[2];
		auto* body_ast = children[3].get();

		while (body_ast->type == AST_GENBLOCK && body_ast->str.empty() &&
				body_ast->children.size() == 1 && body_ast->children.at(0)->type == AST_GENBLOCK)
			body_ast = body_ast->children.at(0).get();

		const char* loop_type_str = "procedural";
		const char* var_type_str = "register";
		AstNodeType var_type = AST_WIRE;
		if (type == AST_GENFOR) {
			loop_type_str = "generate";
			var_type_str = "genvar";
			var_type = AST_GENVAR;
		}

		if (init_ast->type != AST_ASSIGN_EQ)
			input_error("Unsupported 1st expression of %s for-loop!\n", loop_type_str);
		if (next_ast->type != AST_ASSIGN_EQ)
			input_error("Unsupported 3rd expression of %s for-loop!\n", loop_type_str);

		// A name left in current_scope by an earlier module can hide this
		// module's genvar. Bind the generate loop to the local genvar.
		if (type == AST_GENFOR && current_ast_mod && init_ast->children[0]->type == AST_IDENTIFIER) {
			for (auto &node : current_ast_mod->children) {
				if (node && node->type == AST_GENVAR && node->str == init_ast->children[0]->str) {
					init_ast->children[0]->id2ast = node.get();
					current_scope[node->str] = node.get();
					if (next_ast->children[0]->type == AST_IDENTIFIER && next_ast->children[0]->str == node->str)
						next_ast->children[0]->id2ast = node.get();
					break;
				}
			}
		}

		if (init_ast->children[0]->id2ast == nullptr || init_ast->children[0]->id2ast->type != var_type) {
			if (type == AST_FOR) {
				// the loop is dropped: its effects are lost, so this is a
				// degradation (an error unless -permissive), not a warning
				degrade(location.to_loc(), "Procedural for-loop dropped; its induction variable is not a register.\n");
				type = AST_BLOCK;
				children.clear();
				did_something = true;
				goto skip_for_unroll;
			}
			input_error("Left hand side of 1st expression of %s for-loop is not a %s!\n", loop_type_str, var_type_str);
		}
		if (next_ast->children[0]->id2ast == nullptr || next_ast->children[0]->id2ast->type != var_type) {
			degrade(location.to_loc(), "%s for-loop dropped; its step does not assign the %s.\n", loop_type_str, var_type_str);
			type = AST_BLOCK;
			children.clear();
			did_something = true;
			goto skip_for_unroll;
		}

		if (init_ast->children[0]->id2ast != next_ast->children[0]->id2ast)
			input_error("Incompatible left-hand sides in 1st and 3rd expression of %s for-loop!\n", loop_type_str);

		// eval 1st expression
		auto varbuf = init_ast->children[1]->clone();
		{
			int expr_width_hint = -1;
			bool expr_sign_hint = true;
			varbuf->detectSignWidth(expr_width_hint, expr_sign_hint);
			while (varbuf->simplify(true, stage, 32, true)) { }
		}

		if (varbuf->type != AST_CONSTANT)
			varbuf = mkconst_int(location, 0, true);

		auto resolved = current_scope.at(init_ast->children[0]->str);
		auto apply_loop_var_type = [&resolved](std::unique_ptr<AstNode> &value) {
			if (resolved->range_valid) {
				int const_size = value->range_left - value->range_right;
				int resolved_size = resolved->range_left - resolved->range_right;
				if (const_size < resolved_size) {
					for (int i = const_size; i < resolved_size; i++)
						value->bits.push_back(resolved->is_signed ? value->bits.back() : State::S0);
					value->range_left = resolved->range_left;
					value->range_right = resolved->range_right;
					value->range_swapped = resolved->range_swapped;
					value->range_valid = resolved->range_valid;
				}
			}
			value->is_signed = resolved->is_signed;
		};

		apply_loop_var_type(varbuf);

		varbuf = std::make_unique<AstNode>(location, AST_LOCALPARAM, std::move(varbuf));
		varbuf->str = init_ast->children[0]->str;
		varbuf->is_signed = resolved->is_signed;

		AstNode *backup_scope_varbuf = current_scope[varbuf->str];
		current_scope[varbuf->str] = varbuf.get();

		size_t current_block_idx = 0;
		if (type == AST_FOR) {
			while (current_block_idx < current_block->children.size() &&
					current_block->children[current_block_idx].get() != current_block_child)
				current_block_idx++;
		}
		for (auto &init : extra_inits)
			current_block->children.insert(current_block->children.begin() + current_block_idx++, std::move(init));
		size_t return_from = current_block_idx;
		bool saw_iter_return = false;
		std::string exit_brk, exit_cont;

		int unroll_iters = 0;
		while (1)
		{
			// a GENERATE loop elaborates a scope per iteration (cells,
			// assertions): far heavier than a procedural one, and a runaway
			// bound there is usually an unsigned expression that wrapped
			// (`for (i = 1; i < MAX_PENDING-1; i++)` with an int unsigned 0:
			// ~2^32 iterations, a 36 CPU-minute hang on the AXI4 FVIP)
			if (type == AST_GENFOR && unroll_iters + 1 > (1 << 14)) {
				input_error("Generate loop did not terminate after %d iterations "
						"(a runaway bound? e.g. an unsigned parameter expression that wrapped below 0).\n", 1 << 14);
				break;
			}
			if (++unroll_iters > (1 << 20)) {
				// upstream has no cap at all (memory-init loops of several
				// thousand iterations are routine); the fork's old 1024 cap
				// silently truncated them.  Keep only a runaway guard.
				input_error("For-loop did not terminate after %d iterations.\n", 1 << 20);
				break;
			}
			// eval 2nd expression
			auto buf = while_ast->clone();
			{
				int expr_width_hint = -1;
				bool expr_sign_hint = true;
				buf->detectSignWidth(expr_width_hint, expr_sign_hint);
				while (buf->simplify(true, stage, expr_width_hint, expr_sign_hint)) { }
			}

			if (buf->type != AST_CONSTANT) {
				degrade(location.to_loc(), "Loop condition is not constant; the loop is skipped.\n");
				break;
			}

			if (buf->integer == 0) {
				break;
			}

			// expand body
			int index = varbuf->children[0]->integer;
			log_assert(body_ast->type == AST_GENBLOCK || body_ast->type == AST_BLOCK);
			log_assert(!body_ast->str.empty());
			buf = body_ast->clone();
			for (auto &step : extra_steps)
				buf->children.push_back(step->clone());

			std::stringstream sstr;
			sstr << buf->str << "[" << index << "].";
			std::string prefix = sstr.str();

			// create a scoped localparam for the current value of the loop variable
			auto local_index = varbuf->clone();
			size_t pos = local_index->str.rfind('.');
			if (pos != std::string::npos) // remove outer prefix
				local_index->str = "\\" + local_index->str.substr(pos + 1);
			local_index->str = prefix_id(prefix, local_index->str);
			current_scope[local_index->str] = local_index.get();
			current_ast_mod->children.push_back(std::move(local_index));

			buf->expand_genblock(prefix);

			int loop_exit = 0;
			if (type == AST_FOR) {
				// Fold this iteration in place. Simplifying the named block
				// itself would prefix the names a second time.
				for (auto &child : buf->children)
					while (child && child->simplify(true, stage, -1, false)) { }
				loop_exit = trim_uncond_loop_exit(buf->children);
				if (contains_function_return(buf.get()))
					saw_iter_return = true;
				if (loop_exit == 0 && contains_loop_exit(buf.get())) {
					if (exit_brk.empty()) {
						// flags of this loop: per-call locals, never state
						std::stringstream fs;
						fs << "$loop_exit$" << RTLIL::encode_filename(*location.begin.filename) << ":" << location.begin.line << "$" << (autoidx++);
						exit_brk = fs.str() + ".break";
						exit_cont = fs.str() + ".continue";
						for (auto &name : {exit_brk, exit_cont}) {
							auto wire = std::make_unique<AstNode>(location, AST_WIRE, make_range(location, 0, 0));
							wire->str = name;
							wire->is_reg = true;
							wire->is_logic = true;
							wire->set_attribute(ID::nosync, AstNode::mkconst_int(location, 1, false));
							current_scope[name] = wire.get();
							while (wire->simplify(true, 1, -1, false)) { }
							current_ast_mod->children.push_back(std::move(wire));
						}
						auto id = std::make_unique<AstNode>(location, AST_IDENTIFIER);
						id->str = exit_brk;
						current_block->children.insert(current_block->children.begin() + return_from,
								std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::move(id), mkconst_int(location, 0, false, 1)));
						current_block_idx++;
					}
					auto id = std::make_unique<AstNode>(location, AST_IDENTIFIER);
					id->str = exit_cont;
					buf->children.insert(buf->children.begin(),
							std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::move(id), mkconst_int(location, 0, false, 1)));
					guard_loop_exits(buf->children, exit_brk, exit_cont, location);
				}
				if (!exit_brk.empty()) {
					// after a possible break, every later iteration is conditional
					auto body = std::make_unique<AstNode>(location, AST_BLOCK);
					for (auto &st : buf->children)
						body->children.push_back(std::move(st));
					buf->children.clear();
					auto id = std::make_unique<AstNode>(location, AST_IDENTIFIER);
					id->str = exit_brk;
					auto go = std::make_unique<AstNode>(location, AST_REDUCE_BOOL, std::make_unique<AstNode>(location, AST_LOGIC_NOT, std::move(id)));
					buf->children.push_back(std::make_unique<AstNode>(location, AST_CASE, std::move(go),
							std::make_unique<AstNode>(location, AST_COND, mkconst_int(location, 1, false, 1), std::move(body))));
				}
			}

			if (type == AST_GENFOR) {
				for (size_t i = 0; i < buf->children.size(); i++) {
					buf->children[i]->simplify(const_fold, stage, -1, false);
					current_ast_mod->children.push_back(std::move(buf->children[i]));
				}
			} else {
				for (size_t i = 0; i < buf->children.size(); i++)
					current_block->children.insert(current_block->children.begin() + current_block_idx++, std::move(buf->children[i]));
			}
			buf->children.clear();

			// eval 3rd expression
			buf = next_ast->children[1]->clone();
			buf->set_in_param_flag(true);
			{
				int expr_width_hint = -1;
				bool expr_sign_hint = true;
				buf->detectSignWidth(expr_width_hint, expr_sign_hint);
				while (buf->simplify(true, stage, expr_width_hint, expr_sign_hint)) { }
			}

			if (loop_exit == 1)
				break;
			if (buf->type != AST_CONSTANT)
				break;

			apply_loop_var_type(buf);
			varbuf->children[0] = std::move(buf);
		}

		if (type == AST_FOR) {
			auto buf = next_ast->clone();
			buf->children[1] = varbuf->children[0]->clone();
			current_block->children.insert(current_block->children.begin() + current_block_idx++, std::move(buf));
		}
		// A return inside an iteration must also hide every later statement in
		// this block, including the assignment after the loop.
		if (type == AST_FOR && saw_iter_return)
			absorb_loop_tail(current_block, this, return_from);

		current_scope[varbuf->str] = backup_scope_varbuf;
		delete_children();
		did_something = true;
	}
skip_for_unroll:

	// check for local objects in unnamed block
	if (type == AST_BLOCK && str.empty())
	{
		if (this == current_top_block) {
			dict<std::string, ForceHold> held;
			dict<std::string, int> known;
			apply_force_override(children, held, known);
		}
		for (size_t i = 0; i < children.size(); i++)
			if (children[i]->type == AST_WIRE || children[i]->type == AST_MEMORY || children[i]->type == AST_PARAMETER || children[i]->type == AST_LOCALPARAM || children[i]->type == AST_TYPEDEF)
			{
				log_assert(!sv_mode_but_global_and_used_for_literally_one_condition);
				children[i]->input_error("Local declaration in unnamed block is only supported in SystemVerilog mode!\n");
			}
	}

	// transform block with name
	if (type == AST_BLOCK && !str.empty())
	{
		// Loops in an inlined function are unrolled above. Finish returns now
		// that each iteration is a straight-line statement in this block.
		if (get_bool_attribute(ID(sv_return_scope)) && contains_function_return(this)) {
			absorb_function_return(children);
			strip_function_return(children);
			did_something = true;
		}
		dict<std::string, int> known_consts;
		erase_uncond_disable(children, str, known_consts);
		// Return lowering may have moved later function statements into this block.
		// Those names were already prefixed when the function was inlined.
		if (!get_bool_attribute(ID(sv_noexpand)))
			expand_genblock(str + ".");

		// if this is an autonamed block is in an always_comb
		if (current_always && current_always->attributes.count(ID::always_comb)
				&& is_autonamed_block(str))
			// track local variables in this block so we can consider adding
			// nosync once the block has been fully elaborated
			for (auto& child : children)
				if (child->type == AST_WIRE &&
						!child->attributes.count(ID::nosync))
					mark_auto_nosync(this, child.get());

		// a block-local enum type (`enum {...} e;` or a typedef of one) is
		// hoisted with the block's wires, renamed after the block so that
		// copies of the block (loop unrolling) stay distinct; the enum_type
		// attribute of its users follows. Left in the block it reached
		// genrtlil (abort).
		{
			dict<std::string, std::string> enum_rename;
			for (auto &child : children)
				if (child->type == AST_ENUM && !child->str.empty())
					enum_rename[child->str] = prefix_id(str + ".", child->str);
			if (!enum_rename.empty()) {
				auto rename_enum_type = [&](AstNode *n) {
					if (n && n->attributes.count(ID::enum_type) && enum_rename.count(n->attributes.at(ID::enum_type)->str)) {
						std::string en = enum_rename.at(n->attributes.at(ID::enum_type)->str);
						n->attributes[ID::enum_type] = AstNode::mkconst_str(n->location, en);
					}
				};
				for (auto &child : children) {
					rename_enum_type(child.get());
					if (child->type == AST_TYPEDEF && !child->children.empty())
						rename_enum_type(child->children[0].get());
					if (child->type == AST_ENUM) {
						child->str = enum_rename.at(child->str);
						for (auto &item : child->children)
							current_scope[item->str] = item.get();
					}
				}
			}
		}

		std::vector<std::unique_ptr<AstNode>> new_children;
		for (size_t i = 0; i < children.size(); i++)
			if (children[i]->type == AST_ENUM) {
				current_scope[children[i]->str] = children[i].get();
				current_ast_mod->children.push_back(std::move(children[i]));
				current_ast_mod->children.back()->simplify(false, stage, -1, false);
			} else if (children[i]->type == AST_WIRE || children[i]->type == AST_MEMORY || children[i]->type == AST_PARAMETER || children[i]->type == AST_LOCALPARAM || children[i]->type == AST_TYPEDEF) {
				children[i]->simplify(false, stage, -1, false);
				current_scope[children[i]->str] = children[i].get();
				current_ast_mod->children.push_back(std::move(children[i]));
			} else
				new_children.push_back(std::move(children[i]));

		children.swap(new_children);
		did_something = true;
		str.clear();
	}

	// simplify unconditional generate block
	if (type == AST_GENBLOCK && children.size() != 0)
	{
		if (!str.empty()) {
			expand_genblock(str + ".");
		}

		for (size_t i = 0; i < children.size(); i++) {
			children[i]->simplify(const_fold, stage, -1, false);
			current_ast_mod->children.push_back(std::move(children[i]));
		}

		children.clear();
		did_something = true;
	}

	// simplify generate-if blocks
	if (type == AST_GENIF && children.size() != 0)
	{
		auto buf = children[0]->clone();
		while (buf->simplify(true, stage, width_hint, sign_hint)) { }
		if (buf->type != AST_CONSTANT) {
			degrade(location.to_loc(), "Generate if condition is not constant; the block is skipped.\n");
			buf.reset();
		} else if (buf->asBool() != 0) {
			buf = children[1]->clone();
		} else {
			buf = children.size() > 2 ? children[2]->clone() : nullptr;
		}

		if (buf)
		{
			if (buf->type != AST_GENBLOCK)
				buf = std::make_unique<AstNode>(location, AST_GENBLOCK, std::move(buf));

			if (!buf->str.empty()) {
				buf->expand_genblock(buf->str + ".");
			}

			for (size_t i = 0; i < buf->children.size(); i++) {
				buf->children[i]->simplify(const_fold, stage, -1, false);
				current_ast_mod->children.push_back(std::move(buf->children[i]));
			}

			buf->children.clear();
		}

		delete_children();
		did_something = true;
	}

	// simplify generate-case blocks
	if (type == AST_GENCASE && children.size() != 0)
	{
		auto buf = children[0]->clone();
		while (buf->simplify(true, stage, width_hint, sign_hint)) { }
		if (buf->type != AST_CONSTANT) {
			degrade(location.to_loc(), "Generate case selector is not constant; no case arm is generated.\n");
			delete_children();
			did_something = true;
			goto skip_generate_case;
		}

		bool ref_signed = buf->is_signed;
		RTLIL::Const ref_value = buf->bitsAsConst();

		AstNode *selected_case = nullptr;
		for (size_t i = 1; i < children.size(); i++)
		{
			log_assert(children.at(i)->type == AST_COND || children.at(i)->type == AST_CONDX || children.at(i)->type == AST_CONDZ);

			AstNode *this_genblock = nullptr;
			for (auto& child : children.at(i)->children) {
				log_assert(this_genblock == nullptr);
				if (child->type == AST_GENBLOCK)
					this_genblock = child.get();
			}

			for (auto& child : children.at(i)->children)
			{
				if (child->type == AST_DEFAULT) {
					if (selected_case == nullptr)
						selected_case = this_genblock;
					continue;
				}
				if (child->type == AST_GENBLOCK)
					continue;

				buf = child->clone();
				buf->set_in_param_flag(true);
				while (buf->simplify(true, stage, width_hint, sign_hint)) { }
				if (buf->type != AST_CONSTANT) {
					//logger().for_each_sink([&](LogSink &sink) {
					//	FILE *f = sink.file_handle();
					//	if (f)
					//		dumpAst(f, "verilog-ast> ");
					//});
					input_error("Expression in generate case is not constant!\n");
				}

				bool is_selected = RTLIL::const_eq(ref_value, buf->bitsAsConst(), ref_signed && buf->is_signed, ref_signed && buf->is_signed, 1).as_bool();

				if (is_selected) {
					selected_case = this_genblock;
					i = children.size();
					break;
				}
			}
		}

		if (selected_case != nullptr)
		{
			log_assert(selected_case->type == AST_GENBLOCK);
			buf = selected_case->clone();

			if (!buf->str.empty()) {
				buf->expand_genblock(buf->str + ".");
			}

			for (size_t i = 0; i < buf->children.size(); i++) {
				buf->children[i]->simplify(const_fold, stage, -1, false);
				current_ast_mod->children.push_back(std::move(buf->children[i]));
			}

			buf->children.clear();
		}

		delete_children();
		did_something = true;
	}
skip_generate_case:;

	// unroll cell arrays
	if (type == AST_CELLARRAY)
	{
		if (!children.at(0)->range_valid)
			input_error("Non-constant array range on cell array.\n");

		newNode = std::make_unique<AstNode>(location, AST_GENBLOCK);
		int num = max(children.at(0)->range_left, children.at(0)->range_right) - min(children.at(0)->range_left, children.at(0)->range_right) + 1;

		if (this->children.at(1)->type == AST_PRIMITIVE) {
			// Move the range to the AST_PRIMITIVE node and replace this with the AST_PRIMITIVE node handled below
			newNode = std::move(this->children.at(1));
			newNode->range_left = this->children.at(0)->range_left;
			newNode->range_right = this->children.at(0)->range_right;
			newNode->range_valid = true;
			goto apply_newNode;
		}

		for (int i = 0; i < num; i++) {
			int idx = children.at(0)->range_left > children.at(0)->range_right ? children.at(0)->range_right + i : children.at(0)->range_right - i;
			auto new_cell_owned = children.at(1)->clone();
			auto* new_cell = new_cell_owned.get();
			newNode->children.push_back(std::move(new_cell_owned));
			new_cell->str += stringf("[%d]", idx);

			log_assert(new_cell->children.at(0)->type == AST_CELLTYPE);
			new_cell->children.at(0)->str = stringf("$array:%d:%d:%s", i, num, new_cell->children.at(0)->str);
		}

		goto apply_newNode;
	}

	// replace primitives with assignments
	if (type == AST_PRIMITIVE)
	{
		if (children.size() < 2 && str != "pullup" && str != "pulldown")
			input_error("Insufficient number of arguments for primitive `%s'!\n", str);

		std::vector<std::unique_ptr<AstNode>> children_list;
		for (auto& child : children) {
			log_assert(child->type == AST_ARGUMENT);
			log_assert(child->children.size() == 1);
			children_list.push_back(std::move(child->children[0]));
			child->children.clear();
		}
		children.clear();

		// TODO handle bit-widths of primitives and support cell arrays for more primitives

		if (range_valid && str != "tran")
			input_error("Cell arrays of primitives are currently not supported.\n");

		if (str == "bufif0" || str == "bufif1" || str == "notif0" || str == "notif1")
		{
			if (children_list.size() != 3)
				input_error("Invalid number of arguments for primitive `%s'!\n", str);

			std::vector<RTLIL::State> z_const(1, RTLIL::State::Sz);

			auto& mux_input = children_list.at(1);
			if (str == "notif0" || str == "notif1") {
				mux_input = std::make_unique<AstNode>(location, AST_BIT_NOT, std::move(mux_input));
			}
			auto node = std::make_unique<AstNode>(location, AST_TERNARY, std::move(children_list.at(2)));
			if (str == "bufif0") {
				node->children.push_back(AstNode::mkconst_bits(location, z_const, false));
				node->children.push_back(std::move(mux_input));
			} else {
				node->children.push_back(std::move(mux_input));
				node->children.push_back(AstNode::mkconst_bits(location, z_const, false));
			}

			str.clear();
			type = AST_ASSIGN;
			children.push_back(std::move(children_list.at(0)));
			children.back()->was_checked = true;
			children.push_back(std::move(node));
			fixup_hierarchy_flags();
			did_something = true;
		}
		else if (str == "pullup" || str == "pulldown")
		{
			if (children_list.size() != 1)
				input_error("Invalid number of arguments for primitive `%s'!\n", str);
			int bit = str == "pullup" ? 1 : 0;
			str.clear();
			type = AST_ASSIGN;
			children.push_back(std::move(children_list.at(0)));
			children.back()->was_checked = true;
			children.push_back(AstNode::mkconst_int(location, bit, false, 1));
			fixup_hierarchy_flags();
			did_something = true;
		}
		else if (str == "buf" || str == "not" || str == "tran")
		{
			auto& input = children_list.back();
			if (str == "not")
				input = std::make_unique<AstNode>(location, AST_BIT_NOT, std::move(input));

			newNode = std::make_unique<AstNode>(location, AST_GENBLOCK);
			for (auto it = children_list.begin(); it != std::prev(children_list.end()); it++) {
				newNode->children.push_back(std::make_unique<AstNode>(location, AST_ASSIGN, std::move(*it), input->clone()));
				newNode->children.back()->was_checked = true;
			}

			did_something = true;
		}
		else
		{
			AstNodeType op_type = AST_NONE;
			bool invert_results = false;

			if (str == "and")
				op_type = AST_BIT_AND;
			if (str == "nand")
				op_type = AST_BIT_AND, invert_results = true;
			if (str == "or")
				op_type = AST_BIT_OR;
			if (str == "nor")
				op_type = AST_BIT_OR, invert_results = true;
			if (str == "xor")
				op_type = AST_BIT_XOR;
			if (str == "xnor")
				op_type = AST_BIT_XOR, invert_results = true;
			log_assert(op_type != AST_NONE);

			auto& node = children_list[1];
			if (op_type != AST_POS)
				for (size_t i = 2; i < children_list.size(); i++) {
					node = std::make_unique<AstNode>(location, op_type, std::move(node), std::move(children_list[i]));
					node->location = location;
				}
			if (invert_results)
				node = std::make_unique<AstNode>(location, AST_BIT_NOT, std::move(node));

			str.clear();
			type = AST_ASSIGN;
			children.push_back(std::move(children_list[0]));
			children.back()->was_checked = true;
			children.push_back(std::move(node));
			fixup_hierarchy_flags();
			did_something = true;
		}
	}

	// replace dynamic ranges in left-hand side expressions (e.g. "foo[bar] <= 1'b1;") with
	// either a big case block that selects the correct single-bit assignment, or mask and
	// shift operations.
	if (type == AST_ASSIGN_EQ || type == AST_ASSIGN_LE)
	{
		if (children[0]->type != AST_IDENTIFIER || children[0]->children.size() == 0)
			goto skip_dynamic_range_lvalue_expansion;
		if (children[0]->children[0]->range_valid || did_something)
			goto skip_dynamic_range_lvalue_expansion;
		if (children[0]->id2ast == nullptr || children[0]->id2ast->type != AST_WIRE)
			goto skip_dynamic_range_lvalue_expansion;
		if (!children[0]->id2ast->range_valid)
			goto skip_dynamic_range_lvalue_expansion;

		AST::AstNode *member_node = children[0]->get_struct_member();
		int wire_width = member_node ?
			member_node->range_left - member_node->range_right + 1 :
			children[0]->id2ast->range_left - children[0]->id2ast->range_right + 1;
		int wire_offset = children[0]->id2ast->range_right;
		int result_width = 1;

		std::unique_ptr<AstNode> shift_expr = nullptr;
		auto& range = children[0]->children[0];

		if (!try_determine_range_width(range.get(), result_width))
			input_error("Unsupported expression on dynamic range select on signal `%s'!\n", str);

		if (range->children.size() >= 2)
			shift_expr = range->children[1]->clone();
		else
			shift_expr = range->children[0]->clone();

		bool use_case_method = children[0]->id2ast->get_bool_attribute(ID::nowrshmsk);

		if (!use_case_method && current_always && current_always->detect_latch(children[0]->str))
			use_case_method = true;

		if (use_case_method) {
			// big case block

			int stride = 1;
			long long bitno_div = stride;

			int case_width_hint;
			bool case_sign_hint;
			shift_expr->detectSignWidth(case_width_hint, case_sign_hint);
			int max_width = case_width_hint;

			if (member_node) {  // Member in packed struct/union
				// Clamp chunk to range of member within struct/union.
				log_assert(!wire_offset && !children[0]->id2ast->range_swapped);

				// When the (* nowrshmsk *) attribute is set, a CASE block is generated below
				// to select the indexed bit slice. When a multirange array is indexed, the
				// start of each possible slice is separated by the bit stride of the last
				// index dimension, and we can optimize the CASE block accordingly.
				// The dimension of the original array expression is saved in the 'integer' field.
				int dims = children[0]->integer;
				stride = wire_width;
				for (int dim = 0; dim < dims; dim++) {
					stride /= member_node->dimensions[dim].range_width;
				}
				bitno_div = stride;
			} else {
				// Extract (index)*(width) from non_opt_range pattern ((@selfsz@((index)*(width)))+(0)).
				AstNode *lsb_expr =
					shift_expr->type == AST_ADD && shift_expr->children[0]->type == AST_SELFSZ &&
					shift_expr->children[1]->type == AST_CONSTANT && shift_expr->children[1]->integer == 0 ?
					shift_expr->children[0]->children[0].get() :
					shift_expr.get();

				// Extract stride from indexing of two-dimensional packed arrays and
				// variable slices on the form dst[i*stride +: width] = src.
				if (lsb_expr->type == AST_MUL &&
					(lsb_expr->children[0]->type == AST_CONSTANT ||
					 lsb_expr->children[1]->type == AST_CONSTANT))
				{
					int stride_ix = lsb_expr->children[1]->type == AST_CONSTANT;
					stride = (int)lsb_expr->children[stride_ix]->integer;
					bitno_div = stride != 0 ? stride : 1;

					// Check whether i*stride can overflow.
					int i_width;
					bool i_sign;
					lsb_expr->children[1 - stride_ix]->detectSignWidth(i_width, i_sign);
					int stride_width;
					bool stride_sign;
					lsb_expr->children[stride_ix]->detectSignWidth(stride_width, stride_sign);
					max_width = std::max(i_width, stride_width);
					// Stride width calculated from actual stride value.
					if (stride == 0)
						stride_width = 0;
					else
						stride_width = std::ceil(std::log2(std::abs(stride)));

					if (i_width + stride_width > max_width) {
						// For (truncated) i*stride to be within the range of dst, the following must hold:
						//   i*stride ≡ bitno (mod shift_mod), i.e.
						//   i*stride = k*shift_mod + bitno
						//
						// The Diophantine equation on the form ax + by = c:
						//   stride*i - shift_mod*k = bitno
						// has solutions iff c is a multiple of d = gcd(a, b), i.e.
						//   bitno mod gcd(stride, shift_mod) = 0
						//
						// long long is at least 64 bits in C++11
						long long shift_mod = 1ll << (max_width - case_sign_hint);
						bitno_div = std::gcd((long long)stride, shift_mod);
					}
				}
			}

			// long long is at least 64 bits in C++11
			long long max_offset = (1ll << (max_width - case_sign_hint)) - 1;
			long long min_offset = case_sign_hint ? -(1ll << (max_width - 1)) : 0;

			// A temporary register holds the result of the (possibly complex) rvalue expression,
			// avoiding repetition in each AST_COND below.
			int rvalue_width;
			bool rvalue_sign;
			children[1]->detectSignWidth(rvalue_width, rvalue_sign);
			auto rvalue = mktemp_logic(location, "$bitselwrite$rvalue$", current_ast_mod, true, rvalue_width - 1, 0, rvalue_sign);
			auto* rvalue_leaky = rvalue.get();
			log("make 1\n");
			auto case_node_owned = std::make_unique<AstNode>(location, AST_CASE, std::move(shift_expr));
			auto* case_node = case_node_owned.get();
			newNode = std::make_unique<AstNode>(location, AST_BLOCK,
						  std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::move(rvalue), children[1]->clone()),
						  std::move(case_node_owned));

			did_something = true;
			for (int i = 1 - result_width; i < wire_width; i++) {
				// Out of range indexes are handled in genrtlil.cc
				int start_bit = wire_offset + i;
				int end_bit = start_bit + result_width - 1;
				// Check whether the current index can be generated by shift_expr.
				if (start_bit < min_offset || start_bit > max_offset)
					continue;
				if (start_bit%bitno_div != 0 || (stride == 0 && start_bit != 0))
					continue;

				auto cond = std::make_unique<AstNode>(location, AST_COND, mkconst_int(location, start_bit, case_sign_hint, max_width));
				auto lvalue = children[0]->clone();
				lvalue->delete_children();
				if (member_node)
					lvalue->set_attribute(ID::wiretype, member_node->clone());
				lvalue->children.push_back(std::make_unique<AstNode>(location, AST_RANGE,
						mkconst_int(location, end_bit, true), mkconst_int(location, start_bit, true)));
				cond->children.push_back(std::make_unique<AstNode>(location, AST_BLOCK, std::make_unique<AstNode>(location, std::move(type), std::move(lvalue), rvalue_leaky->clone())));
				case_node->children.push_back(std::move(cond));
			}
		} else {
			// mask and shift operations
			// dst = (dst & ~(width'1 << lsb)) | unsigned'(width'(src)) << lsb)

			auto lvalue = children[0]->clone();
			lvalue->delete_children();
			if (member_node)
				lvalue->set_attribute(ID::wiretype, member_node->clone());

			auto old_data = lvalue->clone();
			if (type == AST_ASSIGN_LE)
				old_data->lookahead = true;

			int shift_width_hint;
			bool shift_sign_hint;
			shift_expr->detectSignWidth(shift_width_hint, shift_sign_hint);

			// All operations are carried out in a new block.
			newNode = std::make_unique<AstNode>(location, AST_BLOCK);

			// Temporary register holding the result of the bit- or part-select position expression.
			auto pos = mktemp_logic(location, "$bitselwrite$pos$", current_ast_mod, true, shift_width_hint - 1, 0, shift_sign_hint);
			// Calculate lsb from position.
			auto shift_val = pos->clone();

			newNode->children.push_back(std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::move(pos), std::move(shift_expr)));

			// If the expression is signed, we must add an extra bit for possible negation of the most negative number.
			// If the expression is unsigned, we must add an extra bit for sign.
			shift_val = std::make_unique<AstNode>(location, AST_CAST_SIZE, mkconst_int(location, shift_width_hint + 1, true), std::move(shift_val));
			if (!shift_sign_hint)
				shift_val = std::make_unique<AstNode>(location, AST_TO_SIGNED, std::move(shift_val));

			// offset the shift amount by the lower bound of the dimension
			if (wire_offset != 0)
				shift_val = std::make_unique<AstNode>(location, AST_SUB, std::move(shift_val), mkconst_int(location, wire_offset, true));

			// reflect the shift amount if the dimension is swapped
			if (children[0]->id2ast->range_swapped)
				shift_val = std::make_unique<AstNode>(location, AST_SUB, mkconst_int(location, wire_width - result_width, true), std::move(shift_val));

			// AST_SHIFT uses negative amounts for shifting left
			shift_val = std::make_unique<AstNode>(location, AST_NEG, std::move(shift_val));
			auto also_shift_val = shift_val->clone();

			// dst = (dst & ~(width'1 << lsb)) | unsigned'(width'(src)) << lsb)
			did_something = true;
			auto bitmask = mkconst_bits(location, std::vector<RTLIL::State>(result_width, State::S1), false);
			newNode->children.push_back(
				std::make_unique<AstNode>(location, std::move(type),
						std::move(lvalue),
						std::make_unique<AstNode>(location, AST_BIT_OR,
							std::make_unique<AstNode>(location, AST_BIT_AND,
									std::move(old_data),
									std::make_unique<AstNode>(location, AST_BIT_NOT,
										std::make_unique<AstNode>(location, AST_SHIFT,
												std::move(bitmask),
												std::move(shift_val)))),
							std::make_unique<AstNode>(location, AST_SHIFT,
									std::make_unique<AstNode>(location, AST_TO_UNSIGNED,
										std::make_unique<AstNode>(location, AST_CAST_SIZE,
												mkconst_int(location, result_width, true),
												children[1]->clone())),
									std::move(also_shift_val)))));

			newNode->fixup_hierarchy_flags(true);
		}

		goto apply_newNode;
	}
skip_dynamic_range_lvalue_expansion:;

	// found right-hand side identifier for memory -> replace with memory read port
	if (stage > 1 && type == AST_IDENTIFIER && id2ast != nullptr && id2ast->type == AST_MEMORY && !in_lvalue &&
			children.size() == 1 && children[0]->type == AST_RANGE && children[0]->children.size() == 1) {
		if (integer < (unsigned)id2ast->unpacked_dimensions) {
			// upstream: an error (tests/svtypes/multirange_subarray_access);
			// the fork used to fall through to a whole-array reference
			if (!flag_permissive)
				input_error("Insufficient number of array indices for %s.\n", RTLIL::unescape_id(str));
			degrade(location.to_loc(), "Insufficient number of array indices for %s; the reference is left unexpanded.\n", RTLIL::unescape_id(str).c_str());
			goto skip_memory_read;
		}
		newNode = std::make_unique<AstNode>(location, AST_MEMRD, children[0]->children[0]->clone());
		newNode->str = str;
		newNode->id2ast = id2ast;
		goto apply_newNode;
	}

	// multi-word slice read of a one-dimensional unpacked memory, e.g. arr[5:3]
	// -> concatenation of single-word read ports, leftmost written element first
	if (stage > 1 && type == AST_IDENTIFIER && id2ast != nullptr && id2ast->type == AST_MEMORY && !in_lvalue &&
			id2ast->unpacked_dimensions == 1 && children.size() == 1 && children[0]->type == AST_RANGE &&
			children[0]->children.size() == 2 && children[0]->range_valid &&
			children[0]->range_left != children[0]->range_right) {
		if (integer < (unsigned)id2ast->unpacked_dimensions) {
			// upstream: an error (tests/svtypes/multirange_subarray_access);
			// the fork used to fall through to a whole-array reference
			if (!flag_permissive)
				input_error("Insufficient number of array indices for %s.\n", RTLIL::unescape_id(str));
			degrade(location.to_loc(), "Insufficient number of array indices for %s; the reference is left unexpanded.\n", RTLIL::unescape_id(str).c_str());
			goto skip_memory_read;
		}
		int mem_width, mem_size, addr_bits;
		id2ast->meminfo(mem_width, mem_size, addr_bits);
		int sel_left = children[0]->range_left, sel_right = children[0]->range_right;
		int num_words = std::abs(sel_left - sel_right) + 1;
		if (num_words <= mem_size) {
			int sel_lo = std::min(sel_left, sel_right);
			newNode = std::make_unique<AstNode>(location, AST_CONCAT);
			for (int i = 0; i < num_words; i++) {
				auto rd = std::make_unique<AstNode>(location, AST_MEMRD, mkconst_int(location, sel_lo + i, true));
				rd->str = str;
				rd->id2ast = id2ast;
				newNode->children.push_back(std::move(rd));
			}
			goto apply_newNode;
		}
	}
skip_memory_read:;

	// assignment with nontrivial member in left-hand concat expression -> split assignment
	if ((type == AST_ASSIGN_EQ || type == AST_ASSIGN_LE) && children[0]->type == AST_CONCAT && width_hint > 0)
	{
		bool found_nontrivial_member = false;

		for (auto& child : children[0]->children) {
			if (child->type == AST_IDENTIFIER && child->id2ast != nullptr && child->id2ast->type == AST_MEMORY)
				found_nontrivial_member = true;
		}

		if (found_nontrivial_member)
		{
			newNode = std::make_unique<AstNode>(location, AST_BLOCK);

			auto wire_tmp_owned = std::make_unique<AstNode>(location, AST_WIRE, std::make_unique<AstNode>(location, AST_RANGE, mkconst_int(location, width_hint-1, true), mkconst_int(location, 0, true)));
			auto wire_tmp = wire_tmp_owned.get();
			wire_tmp->str = stringf("$splitcmplxassign$%s:%d$%d", RTLIL::encode_filename(*location.begin.filename), location.begin.line, autoidx++);
			current_scope[wire_tmp->str] = wire_tmp;
			current_ast_mod->children.push_back(std::move(wire_tmp_owned));
			wire_tmp->set_attribute(ID::nosync, AstNode::mkconst_int(location, 1, false));
			while (wire_tmp->simplify(true, 1, -1, false)) { }
			wire_tmp->is_logic = true;

			auto wire_tmp_id_owned = std::make_unique<AstNode>(location, AST_IDENTIFIER);
			auto* wire_tmp_id = wire_tmp_id_owned.get();
			wire_tmp_id->str = wire_tmp->str;

			newNode->children.push_back(std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::move(wire_tmp_id_owned), children[1]->clone()));
			newNode->children.back()->was_checked = true;

			int cursor = 0;
			for (auto& child : children[0]->children)
			{
				int child_width_hint = -1;
				bool child_sign_hint = true;
				child->detectSignWidth(child_width_hint, child_sign_hint);

				auto rhs = wire_tmp_id->clone();
				rhs->children.push_back(std::make_unique<AstNode>(location, AST_RANGE, AstNode::mkconst_int(location, cursor+child_width_hint-1, true), AstNode::mkconst_int(location, cursor, true)));
				newNode->children.push_back(std::make_unique<AstNode>(location, type, child->clone(), std::move(rhs)));

				cursor += child_width_hint;
			}

			goto apply_newNode;
		}
	}

	// IEEE 1800-2009 10.9: named pattern members follow the type, not source order.
	if ((type == AST_ASSIGN_EQ || type == AST_ASSIGN_LE || type == AST_ASSIGN) &&
			children.size() >= 2 && children[1] && children[1]->type == AST_ASSIGN_PATTERN) {
		AstNode *pat = children[1].get();
		bool named = false;
		for (auto &ch : pat->children)
			if (ch && (ch->attributes.count(ID(sv_member)) || ch->attributes.count(ID(sv_typekey))))
				named = true;
		AstNode *lhs_id = children[0].get();
		// a whole unpacked array of structs (`v = '{default: D}`) takes
		// an array pattern; the struct members order only its elements
		bool whole_array = lhs_id && lhs_id->type == AST_IDENTIFIER && lhs_id->id2ast &&
				lhs_id->children.empty() &&
				(lhs_id->id2ast->type == AST_MEMORY || lhs_id->id2ast->unpacked_dimensions > 0);
		if (named && !whole_array && lhs_id && lhs_id->type == AST_IDENTIFIER && lhs_id->id2ast &&
				(lhs_id->attributes.count(ID::wiretype) || lhs_id->id2ast->attributes.count(ID::wiretype))) {
			// a member target (`assign hw2reg.tpm_cap = '{...}`, OpenTitan
			// reg-top connections) is a slice of the struct wire by now; the
			// member rewrite leaves the member's type on the identifier
			AstNode *st = lhs_id->attributes.count(ID::wiretype) ? resolve_wiretype_struct(lhs_id)
					: resolve_wiretype_struct(lhs_id->id2ast);
			auto bare = [](std::string s) {
				if (!s.empty() && s[0] == '\\')
					s = s.substr(1);
				return s;
			};
			auto lookup_struct = [&](const std::string &tname) -> AstNode * {
				AstNode *td = current_scope.count(tname) ? current_scope.at(tname) : nullptr;
				if (!td && current_ast) {
					for (auto &node : current_ast->children)
						if (node && node->type == AST_TYPEDEF && node->str == tname)
							td = node.get();
				}
				if (td && td->type == AST_TYPEDEF && !td->children.empty() &&
						(td->children[0]->type == AST_STRUCT || td->children[0]->type == AST_UNION))
					return td->children[0].get();
				return nullptr;
			};
			if (st && (st->type == AST_STRUCT || st->type == AST_UNION)) {
				std::function<void(AstNode *, AstNode *)> order_pat = [&](AstNode *pattern, AstNode *stype) {
					dict<std::string, AstNode*> by_name;
					AstNode *fallback = nullptr;
					for (auto &ch : pattern->children) {
						if (!ch)
							continue;
						if (ch->attributes.count(ID(sv_member))) {
							std::string mem = bare(ch->attributes.at(ID(sv_member))->str.c_str());
							if (mem == "default")
								fallback = ch.get();
							else
								by_name[mem] = ch.get();
						} else if (ch->attributes.count(ID(sv_typekey))) {
							// a simple-type key fills every member;
							// later keys overwrite earlier ones
							fallback = ch.get();
						}
					}
					std::vector<std::unique_ptr<AstNode>> ordered;
					for (auto &item : stype->children) {
						if (!item)
							continue;
						if (item->type != AST_STRUCT_ITEM && item->type != AST_STRUCT && item->type != AST_UNION)
							continue;
						std::string iname = bare(item->str);
						AstNode *src = by_name.count(iname) ? by_name.at(iname) : fallback;
						std::unique_ptr<AstNode> val;
						if (!src) {
							// IEEE 1800-2009 10.9: members without an assignment
							// get the default value of their type.
							degrade(item->location.to_loc(), "Named assignment pattern at %s leaves member `%s' unassigned; using default value.\n",
									pattern->location.to_string().c_str(), item->str.c_str());
							int member_width = 1;
							if (item->range_valid)
								member_width = std::abs(item->range_left - item->range_right) + 1;
							else
								for (auto &ch : item->children)
									if (ch && ch->type == AST_RANGE && ch->range_valid)
										member_width *= std::abs(ch->range_left - ch->range_right) + 1;
							val = AstNode::mkconst_bits(item->location, RTLIL::Const(0, member_width).to_bits(), false);
						} else {
							val = src->clone();
						}
						val->attributes.erase(ID(sv_member));
						val->attributes.erase(ID(sv_typekey));
						AstNode *nested = nullptr;
						if (item->type == AST_STRUCT || item->type == AST_UNION)
							nested = item.get();
						else {
							for (auto &ch : item->children)
								if (ch && ch->type == AST_WIRETYPE)
									nested = lookup_struct(ch->str);
						}
						if (nested && val->type == AST_ASSIGN_PATTERN)
							order_pat(val.get(), nested);
						ordered.push_back(std::move(val));
					}
					pattern->children.swap(ordered);
				};
				order_pat(pat, st);
				did_something = true;
			}
		}
	}

	// IEEE 1800-2009 10.9.1: '{index: value} follows the declared index, not source order.
	if ((type == AST_ASSIGN_EQ || type == AST_ASSIGN_LE || type == AST_ASSIGN) &&
			children.size() >= 2 && children[1] && children[1]->type == AST_ASSIGN_PATTERN) {
		AstNode *pat = children[1].get();
		bool indexed = false;
		bool has_default = false;
		for (auto &ch : pat->children)
			if (ch) {
				if (ch->attributes.count(ID(sv_index)))
					indexed = true;
				if (ch->attributes.count(ID(sv_member)) && ch->attributes.at(ID(sv_member))->str == "default")
					has_default = true;
			}
		AstNode *lhs_id = children[0].get();
		if ((indexed || has_default) && lhs_id && lhs_id->type == AST_IDENTIFIER && lhs_id->id2ast) {
			dict<int, AstNode*> by_idx;
			AstNode *fallback = nullptr;
			for (auto &ch : pat->children) {
				if (!ch)
					continue;
				if (ch->attributes.count(ID(sv_member)) && ch->attributes.at(ID(sv_member))->str == "default")
					fallback = ch.get();
				if (ch->attributes.count(ID(sv_index))) {
					int idx = ch->attributes.at(ID(sv_index))->asInt(true);
					by_idx[idx] = ch.get();
				}
			}
			std::vector<int> order;
			AstNode *mem = lhs_id->id2ast;
			if (mem->unpacked_dimensions == 1) {
				for (int p = 0; p < mem->dimensions[0].range_width; p++)
					order.push_back(array_indices_from_position(mem, {p})[0]);
			} else if (mem->range_valid && mem->unpacked_dimensions == 0) {
				int left = mem->range_swapped ? mem->range_right : mem->range_left;
				int right = mem->range_swapped ? mem->range_left : mem->range_right;
				int step = left >= right ? -1 : 1;
				for (int idx = left; ; idx += step) {
					order.push_back(idx);
					if (idx == right)
						break;
				}
			}
			if (!order.empty()) {
				std::vector<std::unique_ptr<AstNode>> positioned;
				for (int idx : order) {
					AstNode *src = by_idx.count(idx) ? by_idx.at(idx) : fallback;
					if (!src)
						input_error("Index assignment pattern does not assign index %d.\n", idx);
					auto val = src->clone();
					val->attributes.erase(ID(sv_index));
					val->attributes.erase(ID(sv_member));
						val->attributes.erase(ID(sv_typekey));
					positioned.push_back(std::move(val));
				}
				pat->children.swap(positioned);
				did_something = true;
			}
		}
	}

	// Expand array assignment: arr_out = arr_in OR arr_out = cond ? arr_a : arr_b OR arr_out = '{a, b}
	// Supports multi-dimensional unpacked arrays
	if ((type == AST_ASSIGN_EQ || type == AST_ASSIGN_LE || type == AST_ASSIGN) &&
	    is_unexpanded_array_ref(children[0].get()))
	{
		AstNode *lhs = children[0].get();
		AstNode *rhs = children[1].get();
		AstNode *lhs_mem = lhs->id2ast;

		// Case 1: Direct array assignment (b = a)
		bool is_direct_assign = is_unexpanded_array_ref(rhs);

		// Case 2: Ternary array assignment (out = sel ? a : b)
		bool is_ternary_assign = (rhs->type == AST_TERNARY &&
		                          is_unexpanded_array_ref(rhs->children[1].get()) &&
		                          is_unexpanded_array_ref(rhs->children[2].get()));

		// Case 3: Positional assignment pattern (out = '{a, b})
		bool is_pattern_assign = rhs->type == AST_ASSIGN_PATTERN;

		if (is_direct_assign || is_ternary_assign || is_pattern_assign)
		{
			AstNode *direct_rhs_mem = nullptr;
			AstNode *true_mem = nullptr;
			AstNode *false_mem = nullptr;

			int num_dims = lhs_mem->unpacked_dimensions;
			// A typed array can be stored as a memory whose unpacked-dimension
			// count is still 0. Expanding that creates the same assignment again.
			if (num_dims < 1)
				goto skip_array_expand;
			int total_elements = 1;
			for (int d = 0; d < num_dims; d++)
				total_elements *= lhs_mem->dimensions[d].range_width;
			int element_width, mem_size, addr_bits;
			lhs_mem->meminfo(element_width, mem_size, addr_bits);
			bool pattern_is_flat = false;

			// IEEE 1800-2009 10.9: materialize keyed/default assignment patterns
			// (e.g. '{default: x} or '{1: y, default: x}) on multi-dimensional
			// arrays into positional form so the shape checks below can map them
			// to array positions. Single-dimensional patterns were ordered above.
			if (is_pattern_assign && num_dims > 1) {
				std::function<std::unique_ptr<AstNode>(AstNode *, int)> fill_subarray =
				[&](AstNode *elem, int dim) -> std::unique_ptr<AstNode> {
					auto out = std::make_unique<AstNode>(location, AST_ASSIGN_PATTERN);
					int width = lhs_mem->dimensions[dim].range_width;
					for (int i = 0; i < width; i++) {
						if (dim + 1 == num_dims) {
							auto val = elem->clone();
							val->attributes.erase(ID(sv_index));
							val->attributes.erase(ID(sv_member));
						val->attributes.erase(ID(sv_typekey));
							out->children.push_back(std::move(val));
						} else {
							out->children.push_back(fill_subarray(elem, dim + 1));
						}
					}
					return out;
				};
				std::function<std::unique_ptr<AstNode>(AstNode *, int)> materialize_pattern =
				[&](AstNode *pattern, int dim) -> std::unique_ptr<AstNode> {
					log_assert(pattern->type == AST_ASSIGN_PATTERN);
					log_assert(dim < num_dims);
					AstNode *dflt = nullptr;
					dict<int, AstNode *> by_idx;
					bool keyed = false;
					for (auto &ch : pattern->children) {
						if (!ch)
							continue;
						if (ch->attributes.count(ID(sv_member)) && ch->attributes.at(ID(sv_member))->str == "default") {
							dflt = ch.get();
							keyed = true;
						} else if (ch->attributes.count(ID(sv_index))) {
							by_idx[ch->attributes.at(ID(sv_index))->asInt(true)] = ch.get();
							keyed = true;
						}
					}
					if (!keyed)
						return pattern->clone();
					auto out = std::make_unique<AstNode>(location, AST_ASSIGN_PATTERN);
					int width = lhs_mem->dimensions[dim].range_width;
					bool last = dim + 1 == num_dims;
					int low = lhs_mem->dimensions[dim].range_right;
					int high = low + width - 1;
					bool swapped = lhs_mem->dimensions[dim].range_swapped;
					for (int p = 0; p < width; p++) {
						int idx = swapped ? (low + p) : (high - p);
						AstNode *src = by_idx.count(idx) ? by_idx.at(idx) : dflt;
						if (!src)
							input_error("Index assignment pattern does not assign index %d.\n", idx);
						if (last) {
							auto val = src->clone();
							val->attributes.erase(ID(sv_index));
							val->attributes.erase(ID(sv_member));
						val->attributes.erase(ID(sv_typekey));
							out->children.push_back(std::move(val));
						} else if (src->type == AST_ASSIGN_PATTERN) {
							out->children.push_back(materialize_pattern(src, dim + 1));
						} else {
							// scalar value for a sub-array slot: repeat it
							// across every element of the remaining dimensions
							out->children.push_back(fill_subarray(src, dim + 1));
						}
					}
					return out;
				};
				bool keyed = false;
				for (auto &ch : rhs->children)
					if (ch && (ch->attributes.count(ID(sv_index)) ||
							(ch->attributes.count(ID(sv_member)) && ch->attributes.at(ID(sv_member))->str == "default")))
						keyed = true;
				if (keyed) {
					auto materialized = materialize_pattern(rhs, 0);
					rhs->children.swap(materialized->children);
				}
			}

			// Helper to add indices to an array identifier clone.
			auto add_indices_to_id = [&](std::unique_ptr<AstNode> id, const std::vector<int>& indices) {
				int indexed_dims = GetSize(indices);
				if (indexed_dims == 1) {
					// Single dimension: use AST_RANGE
					id->children.push_back(std::make_unique<AstNode>(location, AST_RANGE,
						mkconst_int(location, indices[0], true)));
				} else {
					// Multiple dimensions: use AST_MULTIRANGE
					auto multirange = std::make_unique<AstNode>(location, AST_MULTIRANGE);
					for (int idx : indices) {
						multirange->children.push_back(std::make_unique<AstNode>(location, AST_RANGE,
							mkconst_int(location, idx, true)));
					}
					id->children.push_back(std::move(multirange));
				}
				id->integer = indexed_dims;
				// Reset basic_prep so multirange gets resolved during subsequent simplify passes
				id->basic_prep = false;
				return id;
			};

			auto add_position_to_id = [&](std::unique_ptr<AstNode> id, AstNode *mem, const std::vector<int>& position) {
				return add_indices_to_id(std::move(id), array_indices_from_position(mem, position));
			};

			// Validate nested assignment pattern shape against unpacked dimensions.
			bool pattern_shape_ok = true;
			std::function<void(AstNode*, int)> validate_pattern_shape = [&](AstNode *pattern, int dim) {
				log_assert(pattern->type == AST_ASSIGN_PATTERN);

				// Expand '{N{...}} replication elements so the shape check
				// and positional selection see the effective element count.
				for (int i = 0; i < GetSize(pattern->children); ) {
					AstNode *ch = pattern->children[i].get();
					if (ch && ch->type == AST_REPLICATE && GetSize(ch->children) == 2 &&
							ch->children[0]->type == AST_CONSTANT) {
						int n = ch->children[0]->asInt(true);
						if (n <= 0) {
							i++;
							continue;
						}
						auto rep = std::move(pattern->children[i]);
						pattern->children.erase(pattern->children.begin() + i);
						AstNode *body = rep->children[1].get();
						int per = 1;
						if (body->type == AST_CONCAT)
							per = GetSize(body->children);
						for (int k = 0; k < n; k++) {
							if (body->type == AST_CONCAT) {
								for (auto &c : body->children)
									pattern->children.insert(pattern->children.begin() + i + k * per, c->clone());
							} else {
								pattern->children.insert(pattern->children.begin() + i + k, body->clone());
							}
						}
						i += n * per;
					} else {
						i++;
					}
				}

				int expected = lhs_mem->dimensions[dim].range_width;
				if (GetSize(pattern->children) != expected)
					input_error("Assignment pattern element count mismatch at dimension %d: got %d, expected %d\n",
						dim + 1, GetSize(pattern->children), expected);

				if (dim + 1 == num_dims)
					return;

				for (auto& child : pattern->children) {
					if (child->type == AST_ASSIGN_PATTERN) {
						validate_pattern_shape(child.get(), dim + 1);
						if (!pattern_shape_ok)
							return;
					} else if (is_unexpanded_array_ref(child.get()) &&
							arrays_have_compatible_dims_from(lhs_mem, dim + 1, child->id2ast)) {
						continue;
					} else {
						degrade(location.to_loc(), "Nested assignment pattern does not match dimension %d; leaving the assignment unexpanded.\n", dim + 2);
						pattern_shape_ok = false;
						return;
					}
				}
			};

			// Select the assignment pattern element for an unpacked array position.
			auto pattern_element_at_position = [&](const std::vector<int>& position, int flat_index) {
				if (pattern_is_flat)
					return rhs->children[flat_index]->clone();

				AstNode *pattern = rhs;
				for (int d = 0; d < num_dims; d++) {
					log_assert(pattern->type == AST_ASSIGN_PATTERN);
					AstNode *element = pattern->children[position[d]].get();

					if (d + 1 == num_dims)
						return element->clone();

					if (element->type == AST_ASSIGN_PATTERN) {
						pattern = element;
					} else {
						std::vector<int> subposition(position.begin() + d + 1, position.end());
						return add_position_to_id(element->clone(), element->id2ast, subposition);
					}
				}
				log_abort();
			};

			// Validate array compatibility
			if (is_direct_assign) {
				direct_rhs_mem = rhs->id2ast;
				if (!arrays_have_compatible_dims(lhs_mem, direct_rhs_mem)) {
					log_file_warning(location.to_loc(), "Array dimension mismatch in assignment; leaving it unexpanded.\n");
					goto skip_array_expand;
				}
			} else if (is_ternary_assign) {
				true_mem = rhs->children[1]->id2ast;
				false_mem = rhs->children[2]->id2ast;
				if (!arrays_have_compatible_dims(lhs_mem, true_mem) ||
				    !arrays_have_compatible_dims(lhs_mem, false_mem))
					input_error("Array dimension mismatch in ternary expression\n");
			} else {
				if (num_dims > 1 && GetSize(rhs->children) == lhs_mem->dimensions[0].range_width) {
					validate_pattern_shape(rhs, 0);
					if (!pattern_shape_ok)
						goto skip_array_expand;
				} else if (num_dims == 1 && GetSize(rhs->children) == total_elements) {
					pattern_is_flat = true;
				} else {
					if (num_dims > 1 && GetSize(rhs->children) == lhs_mem->dimensions[0].range_width)
						validate_pattern_shape(rhs, 0);
					int expected = num_dims > 1 ? lhs_mem->dimensions[0].range_width : total_elements;
					degrade(location.to_loc(), "Assignment pattern element count mismatch: got %d, expected %d. Leaving the assignment unexpanded.\n",
						GetSize(rhs->children), expected);
					goto skip_array_expand;
				}
			}

			// Warn if array assignment expansion is large.
			if (total_elements > 10000)
				log_warning("Expanding array assignment with %d elements at %s, this may be slow.\n",
					total_elements, location.to_string().c_str());

			// Collect all assignments
			std::vector<std::unique_ptr<AstNode>> assignments;
			std::vector<std::unique_ptr<AstNode>> pattern_temp_assignments;

			foreach_array_position(lhs_mem, [&](const std::vector<int>& position) {
				auto lhs_idx = add_position_to_id(lhs->clone(), lhs_mem, position);

				std::unique_ptr<AstNode> rhs_expr;
				if (is_direct_assign) {
					rhs_expr = add_position_to_id(rhs->clone(), direct_rhs_mem, position);
				} else if (is_ternary_assign) {
					// Ternary case
					AstNode *cond = rhs->children[0].get();
					AstNode *true_val = rhs->children[1].get();
					AstNode *false_val = rhs->children[2].get();

					auto true_idx = add_position_to_id(true_val->clone(), true_mem, position);
					auto false_idx = add_position_to_id(false_val->clone(), false_mem, position);

					rhs_expr = std::make_unique<AstNode>(location, AST_TERNARY,
						cond->clone(), std::move(true_idx), std::move(false_idx));
				} else {
					auto pattern_rhs = pattern_element_at_position(position, GetSize(assignments));

					if (type == AST_ASSIGN_EQ) {
						auto wire_tmp_owned = std::make_unique<AstNode>(location, AST_WIRE,
							std::make_unique<AstNode>(location, AST_RANGE,
								mkconst_int(location, element_width - 1, true),
								mkconst_int(location, 0, true)));
						auto wire_tmp = wire_tmp_owned.get();
						wire_tmp->str = stringf("$assignpattern$%s:%d$%d",
							RTLIL::encode_filename(*location.begin.filename), location.begin.line, autoidx++);
						current_scope[wire_tmp->str] = wire_tmp;
						current_ast_mod->children.push_back(std::move(wire_tmp_owned));
						wire_tmp->set_attribute(ID::nosync, AstNode::mkconst_int(location, 1, false));
						while (wire_tmp->simplify(true, 1, -1, false)) { }
						wire_tmp->is_logic = true;
						wire_tmp->is_signed = lhs_mem->is_signed;

						auto tmp_id = std::make_unique<AstNode>(location, AST_IDENTIFIER);
						tmp_id->str = wire_tmp->str;
						pattern_temp_assignments.push_back(std::make_unique<AstNode>(location, AST_ASSIGN_EQ,
							tmp_id->clone(), std::move(pattern_rhs)));
						rhs_expr = std::move(tmp_id);
					} else {
						rhs_expr = std::move(pattern_rhs);
					}
				}

				auto assign = std::make_unique<AstNode>(location, type,
					std::move(lhs_idx), std::move(rhs_expr));
				assign->was_checked = true;
				assignments.push_back(std::move(assign));
			});

			// For continuous assignments, add to module; for procedural, use block
			if (type == AST_ASSIGN) {
				// Add all but last to module
				for (size_t i = 0; i + 1 < assignments.size(); i++)
					current_ast_mod->children.push_back(std::move(assignments[i]));
				// Last one replaces current node
				newNode = std::move(assignments.back());
			} else {
				// Wrap in AST_BLOCK for procedural
				newNode = std::make_unique<AstNode>(location, AST_BLOCK);
				for (auto& assign : pattern_temp_assignments)
					newNode->children.push_back(std::move(assign));
				for (auto& assign : assignments)
					newNode->children.push_back(std::move(assign));
			}

			goto apply_newNode;
		}
	}
skip_array_expand:;

	// assignment with memory in left-hand side expression -> replace with memory write port
	if (stage > 1 && (type == AST_ASSIGN_EQ || type == AST_ASSIGN_LE) && children[0]->type == AST_IDENTIFIER &&
			children[0]->id2ast && children[0]->id2ast->type == AST_MEMORY && children[0]->id2ast->children.size() >= 2 &&
			children[0]->id2ast->children[0]->range_valid && children[0]->id2ast->children[1]->range_valid &&
			(children[0]->children.size() == 1 || children[0]->children.size() == 2) && children[0]->children[0]->type == AST_RANGE)
	{
		if (children[0]->integer < (unsigned)children[0]->id2ast->unpacked_dimensions) {
			if (!flag_permissive)
				input_error("Insufficient number of array indices for %s.\n", RTLIL::unescape_id(children[0]->str));
			degrade(location.to_loc(), "Insufficient number of array indices for %s; the assignment is left unexpanded.\n", RTLIL::unescape_id(children[0]->str).c_str());
			goto skip_memory_write;
		}

		// multi-word slice write to a one-dimensional unpacked memory, e.g. arr[5:3] = x
		// -> evaluate the right-hand side once into a temporary, then write each word
		if (children[0]->id2ast->unpacked_dimensions == 1 && children[0]->children.size() == 1 &&
				children[0]->children[0]->range_valid && children[0]->children[0]->range_left != children[0]->children[0]->range_right)
		{
			int mw_width, mw_size, mw_addr_bits;
			children[0]->id2ast->meminfo(mw_width, mw_size, mw_addr_bits);
			int sel_left = children[0]->children[0]->range_left, sel_right = children[0]->children[0]->range_right;
			int num_words = std::abs(sel_left - sel_right) + 1;
			int dim_left = children[0]->id2ast->children[1]->range_left, dim_right = children[0]->id2ast->children[1]->range_right;
			int sel_lo = std::min(sel_left, sel_right), sel_hi = std::max(sel_left, sel_right);
			int dim_lo = std::min(dim_left, dim_right), dim_hi = std::max(dim_left, dim_right);
			if (num_words <= mw_size && sel_lo >= dim_lo && sel_hi <= dim_hi && mw_width > 0) {
				auto tmp = mktemp_logic(location, "$memslice$", current_ast_mod, true, num_words*mw_width - 1, 0, false);
				newNode = std::make_unique<AstNode>(location, AST_BLOCK);
				newNode->children.push_back(std::make_unique<AstNode>(location, AST_ASSIGN_EQ, tmp->clone(), std::move(children[1])));
				for (int i = 0; i < num_words; i++) {
					auto lhs = children[0]->clone();
					lhs->children.clear();
					lhs->children.push_back(std::make_unique<AstNode>(location, AST_RANGE, mkconst_int(location, sel_lo + i, true)));
					auto rhs = tmp->clone();
					rhs->children.push_back(std::make_unique<AstNode>(location, AST_RANGE,
							mkconst_int(location, (i+1)*mw_width - 1, true),
							mkconst_int(location, i*mw_width, true)));
					auto assign = std::make_unique<AstNode>(location, type, std::move(lhs), std::move(rhs));
					assign->was_checked = true;
					newNode->children.push_back(std::move(assign));
				}
				did_something = true;
				goto apply_newNode;
			}
		}

		std::stringstream sstr;
		sstr << "$memwr$" << children[0]->str << "$" << RTLIL::encode_filename(*location.begin.filename) << ":" << location.begin.line << "$" << (autoidx++);
		std::string id_addr = sstr.str() + "_ADDR", id_data = sstr.str() + "_DATA", id_en = sstr.str() + "_EN";

		int mem_width, mem_size, addr_bits;
		bool mem_signed = children[0]->id2ast->is_signed;
		children[0]->id2ast->meminfo(mem_width, mem_size, addr_bits);

		newNode = std::make_unique<AstNode>(location, AST_BLOCK);
		auto defNode = std::make_unique<AstNode>(location, AST_BLOCK);

		int data_range_left = children[0]->id2ast->children[0]->range_left;
		int data_range_right = children[0]->id2ast->children[0]->range_right;
		int mem_data_range_offset = std::min(data_range_left, data_range_right);

		int addr_width_hint = -1;
		bool addr_sign_hint = true;
		children[0]->children[0]->children[0]->detectSignWidthWorker(addr_width_hint, addr_sign_hint);
		addr_bits = std::max(addr_bits, addr_width_hint);

		std::vector<RTLIL::State> x_bits_addr, x_bits_data, set_bits_en;
		for (int i = 0; i < addr_bits; i++)
			x_bits_addr.push_back(RTLIL::State::Sx);
		for (int i = 0; i < mem_width; i++)
			x_bits_data.push_back(RTLIL::State::Sx);
		for (int i = 0; i < mem_width; i++)
			set_bits_en.push_back(RTLIL::State::S1);

		std::unique_ptr<AstNode> node_addr = nullptr;
		if (children[0]->children[0]->children[0]->isConst()) {
			node_addr = children[0]->children[0]->children[0]->clone();
		} else {
			auto wire_addr_owned = std::make_unique<AstNode>(location, AST_WIRE, std::make_unique<AstNode>(location, AST_RANGE, mkconst_int(location, addr_bits-1, true), mkconst_int(location, 0, true)));
			auto* wire_addr = wire_addr_owned.get();
			wire_addr->str = id_addr;
			wire_addr->was_checked = true;
			current_ast_mod->children.push_back(std::move(wire_addr_owned));
			current_scope[wire_addr->str] = wire_addr;
			while (wire_addr->simplify(true, 1, -1, false)) { }

			auto assign_addr = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), mkconst_bits(location, x_bits_addr, false));
			assign_addr->children[0]->str = id_addr;
			assign_addr->children[0]->was_checked = true;
			defNode->children.push_back(std::move(assign_addr));

			assign_addr = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), children[0]->children[0]->children[0]->clone());
			assign_addr->children[0]->str = id_addr;
			assign_addr->children[0]->was_checked = true;
			newNode->children.push_back(std::move(assign_addr));

			node_addr = std::make_unique<AstNode>(location, AST_IDENTIFIER);
			node_addr->str = id_addr;
		}

		std::unique_ptr<AstNode> node_data = nullptr;
		if (children[0]->children.size() == 1 && children[1]->isConst()) {
			node_data = children[1]->clone();
		} else {
			auto wire_data_owned = std::make_unique<AstNode>(location, AST_WIRE, std::make_unique<AstNode>(location, AST_RANGE, mkconst_int(location, mem_width-1, true), mkconst_int(location, 0, true)));
			auto* wire_data = wire_data_owned.get();
			wire_data->str = id_data;
			wire_data->was_checked = true;
			wire_data->is_signed = mem_signed;
			current_scope[wire_data->str] = wire_data;
			current_ast_mod->children.push_back(std::move(wire_data_owned));
			while (wire_data->simplify(true, 1, -1, false)) { }

			auto assign_data = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), mkconst_bits(location, x_bits_data, false));
			assign_data->children[0]->str = id_data;
			assign_data->children[0]->was_checked = true;
			defNode->children.push_back(std::move(assign_data));

			node_data = std::make_unique<AstNode>(location, AST_IDENTIFIER);
			node_data->str = id_data;
		}

		auto wire_en_owned = std::make_unique<AstNode>(location, AST_WIRE, std::make_unique<AstNode>(location, AST_RANGE, mkconst_int(location, mem_width-1, true), mkconst_int(location, 0, true)));
		auto* wire_en = wire_en_owned.get();
		wire_en->str = id_en;
		wire_en->was_checked = true;
		current_scope[wire_en->str] = wire_en;
		current_ast_mod->children.push_back(std::move(wire_en_owned));
		while (wire_en->simplify(true, 1, -1, false)) { }

		auto assign_en_first = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), mkconst_int(location, 0, false, mem_width));
		assign_en_first->children[0]->str = id_en;
		assign_en_first->children[0]->was_checked = true;
		defNode->children.push_back(std::move(assign_en_first));

		auto node_en = std::make_unique<AstNode>(location, AST_IDENTIFIER);
		node_en->str = id_en;

		if (!defNode->children.empty())
			current_top_block->children.insert(current_top_block->children.begin(), std::move(defNode));

		std::unique_ptr<AstNode> assign_data = nullptr;
		std::unique_ptr<AstNode> assign_en = nullptr;
		if (children[0]->children.size() == 2)
		{
			if (children[0]->children[1]->range_valid)
			{
				int offset = children[0]->children[1]->range_right;
				int width = children[0]->children[1]->range_left - offset + 1;
				offset -= mem_data_range_offset;

				std::vector<RTLIL::State> padding_x(offset, RTLIL::State::Sx);

				assign_data = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER),
						std::make_unique<AstNode>(location, AST_CONCAT, mkconst_bits(location, padding_x, false), children[1]->clone()));
				assign_data->children[0]->str = id_data;
				assign_data->children[0]->was_checked = true;

				for (int i = 0; i < mem_width; i++)
					set_bits_en[i] = offset <= i && i < offset+width ? RTLIL::State::S1 : RTLIL::State::S0;
				assign_en = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), mkconst_bits(location, set_bits_en, false));
				assign_en->children[0]->str = id_en;
				assign_en->children[0]->was_checked = true;
			}
			else
			{
				auto& the_range = children[0]->children[1];
				std::unique_ptr<AstNode> offset_ast;
				int width;

				if (!try_determine_range_width(the_range.get(), width))
					input_error("Unsupported expression on dynamic range select on signal `%s'!\n", str);

				if (the_range->children.size() >= 2)
					offset_ast = the_range->children[1]->clone();
				else
					offset_ast = the_range->children[0]->clone();

				if (mem_data_range_offset)
					offset_ast = std::make_unique<AstNode>(location, AST_SUB, std::move(offset_ast), mkconst_int(location, mem_data_range_offset, true));

				assign_data = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER),
						std::make_unique<AstNode>(location, AST_SHIFT_LEFT, children[1]->clone(), offset_ast->clone()));
				assign_data->children[0]->str = id_data;
				assign_data->children[0]->was_checked = true;

				for (int i = 0; i < mem_width; i++)
					set_bits_en[i] = i < width ? RTLIL::State::S1 : RTLIL::State::S0;
				assign_en = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER),
						std::make_unique<AstNode>(location, AST_SHIFT_LEFT, mkconst_bits(location, set_bits_en, false), offset_ast->clone()));
				assign_en->children[0]->str = id_en;
				assign_en->children[0]->was_checked = true;
			}
		}
		else
		{
			if (!(children[0]->children.size() == 1 && children[1]->isConst())) {
				assign_data = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), children[1]->clone());
				assign_data->children[0]->str = id_data;
				assign_data->children[0]->was_checked = true;
			}

			assign_en = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), mkconst_bits(location, set_bits_en, false));
			assign_en->children[0]->str = id_en;
			assign_en->children[0]->was_checked = true;
		}
		if (assign_data)
			newNode->children.push_back(std::move(assign_data));
		if (assign_en)
			newNode->children.push_back(std::move(assign_en));

		std::unique_ptr<AstNode> wrnode;
		if (current_always->type == AST_INITIAL)
			wrnode = std::make_unique<AstNode>(location, AST_MEMINIT, std::move(node_addr), std::move(node_data), std::move(node_en), mkconst_int(location, 1, false));
		else
			wrnode = std::make_unique<AstNode>(location, AST_MEMWR, std::move(node_addr), std::move(node_data), std::move(node_en));
		wrnode->str = children[0]->str;
		wrnode->id2ast = children[0]->id2ast;
		wrnode->location = location;
		if (wrnode->type == AST_MEMWR) {
			int portid = current_memwr_count[wrnode->str]++;
			wrnode->children.push_back(mkconst_int(location, portid, false));
			std::vector<RTLIL::State> priority_mask;
			for (int i = 0; i < portid; i++) {
				bool has_prio = current_memwr_visible[wrnode->str].count(i);
				priority_mask.push_back(State(has_prio));
			}
			wrnode->children.push_back(mkconst_bits(location, priority_mask, false));
			current_memwr_visible[wrnode->str].insert(portid);
			current_always->children.push_back(std::move(wrnode));
		} else {
			current_ast_mod->children.push_back(std::move(wrnode));
		}

		if (newNode->children.empty()) {
			newNode = std::make_unique<AstNode>(location);
		}
		goto apply_newNode;
	}
skip_memory_write:;

	// replace function and task calls with the code from the function or task
	// break/continue stay as statements; the for unroller and constant
	// functions read them.
	if ((type == AST_FCALL || type == AST_TCALL) && !str.empty() &&
			!(type == AST_TCALL && (str == "\\break" || str == "\\continue" || str == "\\release" || str == "\\deassign" || str == "\\return")))
	{
		if (type == AST_FCALL)
		{
			if (str == "\\$initstate")
			{
				int myidx = autoidx++;

				auto wire_owned = std::make_unique<AstNode>(location, AST_WIRE);
				auto* wire = wire_owned.get();
				current_ast_mod->children.push_back(std::move(wire_owned));
				wire->str = stringf("$initstate$%d_wire", myidx);
				while (wire->simplify(true, 1, -1, false)) { }

				auto cell_owned = std::make_unique<AstNode>(location, AST_CELL, std::make_unique<AstNode>(location, AST_CELLTYPE), std::make_unique<AstNode>(location, AST_ARGUMENT, std::make_unique<AstNode>(location, AST_IDENTIFIER)));
				auto* cell = cell_owned.get();
				cell->str = stringf("$initstate$%d", myidx);
				cell->children[0]->str = "$initstate";
				cell->children[1]->str = "\\Y";
				cell->children[1]->children[0]->str = wire->str;
				cell->children[1]->children[0]->id2ast = wire;
				current_ast_mod->children.push_back(std::move(cell_owned));
				while (cell->simplify(true, 1, -1, false)) { }

				newNode = std::make_unique<AstNode>(location, AST_IDENTIFIER);
				newNode->str = wire->str;
				newNode->id2ast = wire;
				goto apply_newNode;
			}

			if (str == "\\$past")
			{
				if (width_hint < 0)
					goto replace_fcall_later;

				int num_steps = 1;

				if (GetSize(children) != 1 && GetSize(children) != 2)
					input_error("System function %s got %d arguments, expected 1 or 2.\n",
							RTLIL::unescape_id(str), int(children.size()));

				if (!current_always_clocked) {
					// No enclosing clocked block: a concurrent assertion
					// context.  Keep the call; genrtlil emits a $past cell
					// and sva2check lowers it against the property clock.
					recursion_counter--;
					return false;
				}

				if (GetSize(children) == 2)
				{
					auto buf = children[1]->clone();
					while (buf->simplify(true, stage, -1, false)) { }
					if (buf->type != AST_CONSTANT)
						input_error("Failed to evaluate system function `%s' with non-constant value.\n", str);

					num_steps = buf->asInt(true);
				}

				AstNode *block = nullptr;

				for (auto& child : current_always->children)
					if (child->type == AST_BLOCK)
						block = child.get();

				log_assert(block != nullptr);

				if (num_steps == 0) {
					newNode = children[0]->clone();
					goto apply_newNode;
				}

				int myidx = autoidx++;
				AstNode* outreg = nullptr;

				for (int i = 0; i < num_steps; i++)
				{
					auto reg_owned = std::make_unique<AstNode>(location, AST_WIRE, std::make_unique<AstNode>(location, AST_RANGE,
							mkconst_int(location, width_hint-1, true), mkconst_int(location, 0, true)));
					auto* reg = reg_owned.get();
					current_ast_mod->children.push_back(std::move(reg_owned));

					reg->str = stringf("$past$%s:%d$%d$%d", RTLIL::encode_filename(*location.begin.filename), location.begin.line, myidx, i);
					reg->is_reg = true;
					reg->is_signed = sign_hint;


					while (reg->simplify(true, 1, -1, false)) { }

					auto regid = std::make_unique<AstNode>(location, AST_IDENTIFIER);
					regid->str = reg->str;
					regid->id2ast = reg;
					regid->was_checked = true;

					std::unique_ptr<AstNode> rhs = nullptr;

					if (outreg == nullptr) {
						rhs = children.at(0)->clone();
					} else {
						rhs = std::make_unique<AstNode>(location, AST_IDENTIFIER);
						rhs->str = outreg->str;
						rhs->id2ast = outreg;
					}

					block->children.push_back(std::make_unique<AstNode>(location, AST_ASSIGN_LE, std::move(regid), std::move(rhs)));
					outreg = reg;
				}

				newNode = std::make_unique<AstNode>(location, AST_IDENTIFIER);
				newNode->str = outreg->str;
				newNode->id2ast = outreg;
				goto apply_newNode;
			}

			if (str == "\\$stable" || str == "\\$rose" || str == "\\$fell" || str == "\\$changed")
			{
				if (GetSize(children) != 1 && GetSize(children) != 2)
					input_error("System function %s got %d arguments, expected 1 or 2.\n",
							RTLIL::unescape_id(str), int(children.size()));

				if (!current_always_clocked) {
					recursion_counter--;
					return false;
				}

				{
				// Second argument is an optional clocking event. Drop it when
				// rewriting into $past, which uses the enclosing clock.
				if (GetSize(children) == 2)
					children.pop_back();

				auto present = children.at(0)->clone();
				auto past = clone();
				past->str = "\\$past";

				if (str == "\\$stable")
					newNode = std::make_unique<AstNode>(location, AST_EQ, std::move(past), std::move(present));

				else if (str == "\\$changed")
					newNode = std::make_unique<AstNode>(location, AST_NE, std::move(past), std::move(present));

				else if (str == "\\$rose")
					newNode = std::make_unique<AstNode>(location, AST_LOGIC_AND,
							std::make_unique<AstNode>(location, AST_LOGIC_NOT, std::make_unique<AstNode>(location, AST_BIT_AND, std::move(past), mkconst_int(location, 1, false))),
							std::make_unique<AstNode>(location, AST_BIT_AND, std::move(present), mkconst_int(location, 1, false)));

				else if (str == "\\$fell")
					newNode = std::make_unique<AstNode>(location, AST_LOGIC_AND,
							std::make_unique<AstNode>(location, AST_BIT_AND, std::move(past), mkconst_int(location, 1, false)),
							std::make_unique<AstNode>(location, AST_LOGIC_NOT, std::make_unique<AstNode>(location, AST_BIT_AND, std::move(present), mkconst_int(location, 1, false))));

				else
					log_abort();

				goto apply_newNode;
				}
			}

			// $anyconst and $anyseq are mapped in AstNode::genRTLIL()
			if (str == "\\$anyconst" || str == "\\$anyseq" || str == "\\$allconst" || str == "\\$allseq") {
				recursion_counter--;
				return false;
			}

			if (str == "\\$isunbounded")
			{
				// IEEE 1800-2017 20.6.3: true iff the constant argument is `$`.
				// `$` itself lexes as the identifier `\$`; any other argument
				// must fold to a constant, which is bounded.
				if (children.size() != 1)
					input_error("System function %s got %d arguments, expected 1.\n",
							RTLIL::unescape_id(str), int(children.size()));
				if (children[0]->type == AST_IDENTIFIER && children[0]->str == "\\$" && children[0]->children.empty()) {
					newNode = mkconst_int(location, 1, false, 1);
					goto apply_newNode;
				}
				auto buf = children[0]->clone();
				while (buf->simplify(true, stage, width_hint, sign_hint)) { }
				if (buf->type == AST_CONSTANT || buf->type == AST_REALVALUE) {
					newNode = mkconst_int(location, 0, false, 1);
					goto apply_newNode;
				}
				if (stage > 1)
					input_error("System function %s needs a constant argument (a parameter or `$').\n",
							RTLIL::unescape_id(str));
			}

			if (str == "\\$clog2")
			{
				if (children.empty())
					input_error("System function %s got %d arguments, expected 1.\n",
							RTLIL::unescape_id(str), int(children.size()));
				if (children.size() != 1) {
					degrade(location.to_loc(), "System function %s got %d arguments; using the first.\n",
								RTLIL::unescape_id(str).c_str(), int(children.size()));
				}

				auto buf = children[0]->clone();
				while (buf->simplify(true, stage, width_hint, sign_hint)) { }
				if (buf->type == AST_CONSTANT) {
					// IEEE 1800 20.8.1: $clog2 is a constant function of a constant argument.
					RTLIL::Const arg_value = buf->bitsAsConst();
					if (arg_value.as_bool())
						arg_value = const_sub(arg_value, 1, false, false, GetSize(arg_value));

					uint32_t result = 0;
					for (auto i = 0; i < arg_value.size(); i++)
						if (arg_value.at(i) == RTLIL::State::S1)
							result = i + 1;

					newNode = mkconst_int(location, result, true);
					goto apply_newNode;
				}
			}

			if (str == "\\$dimensions" || str == "\\$unpacked_dimensions" ||
				str == "\\$increment" || str == "\\$size" || str == "\\$bits" || str == "\\$high" || str == "\\$low" || str == "\\$left" || str == "\\$right")
			{
				int dim = 1;
				if (str == "\\$dimensions" || str == "\\$unpacked_dimensions" || str == "\\$bits") {
					if (children.size() != 1)
						input_error("System function %s got %d arguments, expected 1.\n",
								RTLIL::unescape_id(str), int(children.size()));
				} else {
					if (children.size() != 1 && children.size() != 2)
						input_error("System function %s got %d arguments, expected 1 or 2.\n",
							RTLIL::unescape_id(str), int(children.size()));
					if (children.size() == 2) {
						auto buf = children[1]->clone();
						// Evaluate constant expression
						while (buf->simplify(true, stage, width_hint, sign_hint)) { }
						if (buf->type != AST_CONSTANT) {
							degrade(location.to_loc(), "Non-constant dimension in `%s'; using dimension 1.\n", str.c_str());
							dim = 1;
						} else
							dim = buf->asInt(false);
					}
				}
				auto buf = children[0]->clone();
				int mem_depth = 1;
				int result, high = 0, low = 0, left = 0, right = 0, width = 1; // defaults for a simple wire
				int expr_dimensions = 0, expr_unpacked_dimensions = 0;
				AstNode *id_ast = nullptr;

				buf->detectSignWidth(width_hint, sign_hint);

				if (buf->type == AST_IDENTIFIER) {
					id_ast = buf->id2ast;
					if (id_ast == nullptr && current_scope.count(buf->str))
						id_ast = current_scope.at(buf->str);
					if (!id_ast) {
						if (!flag_permissive)
							log_file_error(location.to_loc(), "Failed to detect width for identifier %s!\n", buf->str.c_str());
						degrade(location.to_loc(), "Failed to resolve `%s' for width detection; using width 1.\n", buf->str.c_str());
						newNode = mkconst_int(location, 1, false);
						goto apply_newNode;
					}

					if (id_ast->type == AST_TYPEDEF && !id_ast->children.empty()) {
						// IEEE 1800: $bits(data_type) is the packed width of that type.
						AstNode *td = id_ast->children[0].get();
						while (td->simplify(true, stage, -1, false)) { }
						int bits = 1;
						if (td->type == AST_STRUCT || td->type == AST_UNION)
							bits = size_packed_struct(td, 0);
						else if (td->range_valid)
							bits = std::abs(td->range_left - td->range_right) + 1;
						else if (!td->children.empty() && td->children[0] && td->children[0]->type == AST_RANGE) {
							while (td->children[0]->simplify(true, stage, -1, false)) { }
							bits = range_width(td, td->children[0].get());
						}
						width = bits;
						high = bits - 1;
						left = high;
						expr_dimensions = 1;
					} else if (id_ast->type == AST_WIRE || id_ast->type == AST_MEMORY) {
						// Check for item in packed struct / union
						AstNode *item_node = buf->get_struct_member();
						if (item_node)
							id_ast = item_node;

						// The dimension of the original array expression is saved in the 'integer' field
						int sel_dims = buf->integer;
						if (buf->attributes.count(ID(sv_elem_member))) {
							// element member select `W[i].m` (rewritten into
							// a range on W): the expression has the member's
							// type, indexed once more if the member was
							id_ast = buf->attributes.at(ID(sv_elem_member)).get();
							sel_dims = buf->attributes.at(ID(sv_elem_member_sel))->integer;
						}
						dim += sel_dims;

						int dims = GetSize(id_ast->dimensions);
						// TODO: IEEE Std 1800-2017 20.7: "If the first argument to an array query function would cause $dimensions to return 0
						// or if the second argument is out of range, then 'x shall be returned."
						if (dim < 1 || dim > dims)
							input_error("Dimension %d out of range in `%s', as it only has %d dimensions!\n", dim, id_ast->str, dims);

						expr_dimensions = dims - dim + 1;
						expr_unpacked_dimensions = std::max(id_ast->unpacked_dimensions - dim + 1, 0);

						right = low  = id_ast->dimensions[dim - 1].range_right;
						left  = high = low + id_ast->dimensions[dim - 1].range_width - 1;
						if (id_ast->dimensions[dim - 1].range_swapped) {
							std::swap(left, right);
						}
						for (int i = dim; i < dims; i++) {
							mem_depth *= id_ast->dimensions[i].range_width;
						}
					} else if ((id_ast->type == AST_PARAMETER || id_ast->type == AST_LOCALPARAM) && id_ast->unpacked_dimensions > 0) {
						// parameter array (`localparam logic [7:0] M [4:6]`):
						// its unpacked dimensions, then the packed range
						std::vector<dimension_t> pdims = id_ast->dimensions;
						if (GetSize(pdims) == id_ast->unpacked_dimensions) {
							if (id_ast->range_valid)
								pdims.push_back({ std::min(id_ast->range_left, id_ast->range_right),
										std::abs(id_ast->range_left - id_ast->range_right) + 1, id_ast->range_swapped });
							else
								pdims.push_back({ 0, 1, false });
						}
						dim += buf->children.empty() ? 0 : (buf->children[0]->type == AST_MULTIRANGE ? GetSize(buf->children[0]->children) : 1);
						int dims = GetSize(pdims);
						if (dim < 1 || dim > dims)
							input_error("Dimension %d out of range in `%s', as it only has %d dimensions!\n", dim, id_ast->str, dims);
						expr_dimensions = dims - dim + 1;
						expr_unpacked_dimensions = std::max(id_ast->unpacked_dimensions - dim + 1, 0);
						right = low = pdims[dim - 1].range_right;
						left = high = low + pdims[dim - 1].range_width - 1;
						if (pdims[dim - 1].range_swapped)
							std::swap(left, right);
						for (int i = dim; i < dims; i++)
							mem_depth *= pdims[i].range_width;
					}
					width = high - low + 1;
				} else {
					width = width_hint;
					right = low  = 0;
					left  = high = width - 1;
					expr_dimensions = 1;
				}
				if (str == "\\$dimensions")
					result = expr_dimensions;
				else if (str == "\\$unpacked_dimensions")
					result = expr_unpacked_dimensions;
				else if (str == "\\$high")
					result = high;
				else if (str == "\\$low")
					result = low;
				else if (str == "\\$left")
					result = left;
				else if (str == "\\$right")
					result = right;
				else if (str == "\\$increment")
					result = left >= right ? 1 : -1;
				else if (str == "\\$size")
					result = width;
				else { // str == "\\$bits"
					result = width * mem_depth;
				}
				newNode = mkconst_int(location, result, true);
				goto apply_newNode;
			}

			if (str == "\\$ln" || str == "\\$log10" || str == "\\$exp" || str == "\\$sqrt" || str == "\\$pow" ||
					str == "\\$floor" || str == "\\$ceil" || str == "\\$sin" || str == "\\$cos" || str == "\\$tan" ||
					str == "\\$asin" || str == "\\$acos" || str == "\\$atan" || str == "\\$atan2" || str == "\\$hypot" ||
					str == "\\$sinh" || str == "\\$cosh" || str == "\\$tanh" || str == "\\$asinh" || str == "\\$acosh" || str == "\\$atanh" ||
					str == "\\$rtoi" || str == "\\$itor")
			{
				bool func_with_two_arguments = str == "\\$pow" || str == "\\$atan2" || str == "\\$hypot";
				double x = 0, y = 0;

				if (func_with_two_arguments) {
					if (children.size() != 2)
						input_error("System function %s got %d arguments, expected 2.\n",
								RTLIL::unescape_id(str), int(children.size()));
				} else {
					if (children.size() != 1)
						input_error("System function %s got %d arguments, expected 1.\n",
								RTLIL::unescape_id(str), int(children.size()));
				}

				if (children.size() >= 1) {
					while (children[0]->simplify(true, stage, width_hint, sign_hint)) { }
					if (!children[0]->isConst())
						input_error("Failed to evaluate system function `%s' with non-constant argument.\n",
								RTLIL::unescape_id(str));
					int child_width_hint = width_hint;
					bool child_sign_hint = sign_hint;
					children[0]->detectSignWidth(child_width_hint, child_sign_hint);
					x = children[0]->asReal(child_sign_hint);
				}

				if (children.size() >= 2) {
					while (children[1]->simplify(true, stage, width_hint, sign_hint)) { }
					if (!children[1]->isConst())
						input_error("Failed to evaluate system function `%s' with non-constant argument.\n",
								RTLIL::unescape_id(str));
					int child_width_hint = width_hint;
					bool child_sign_hint = sign_hint;
					children[1]->detectSignWidth(child_width_hint, child_sign_hint);
					y = children[1]->asReal(child_sign_hint);
				}

				if (str == "\\$rtoi") {
					// Truncate toward zero, then keep the two's-complement bits.
					int32_t truncated = static_cast<int32_t>(std::trunc(x));
					newNode = AstNode::mkconst_int(location, static_cast<uint32_t>(truncated), true);
				} else {
					newNode = std::make_unique<AstNode>(location, AST_REALVALUE);
					if (str == "\\$ln")		 newNode->realvalue = ::log(x);
					else if (str == "\\$log10") newNode->realvalue = ::log10(x);
					else if (str == "\\$exp")   newNode->realvalue = ::exp(x);
					else if (str == "\\$sqrt")  newNode->realvalue = ::sqrt(x);
					else if (str == "\\$pow")   newNode->realvalue = ::pow(x, y);
					else if (str == "\\$floor") newNode->realvalue = ::floor(x);
					else if (str == "\\$ceil")  newNode->realvalue = ::ceil(x);
					else if (str == "\\$sin")   newNode->realvalue = ::sin(x);
					else if (str == "\\$cos")   newNode->realvalue = ::cos(x);
					else if (str == "\\$tan")   newNode->realvalue = ::tan(x);
					else if (str == "\\$asin")  newNode->realvalue = ::asin(x);
					else if (str == "\\$acos")  newNode->realvalue = ::acos(x);
					else if (str == "\\$atan")  newNode->realvalue = ::atan(x);
					else if (str == "\\$atan2") newNode->realvalue = ::atan2(x, y);
					else if (str == "\\$hypot") newNode->realvalue = ::hypot(x, y);
					else if (str == "\\$sinh")  newNode->realvalue = ::sinh(x);
					else if (str == "\\$cosh")  newNode->realvalue = ::cosh(x);
					else if (str == "\\$tanh")  newNode->realvalue = ::tanh(x);
					else if (str == "\\$asinh") newNode->realvalue = ::asinh(x);
					else if (str == "\\$acosh") newNode->realvalue = ::acosh(x);
					else if (str == "\\$atanh") newNode->realvalue = ::atanh(x);
					else if (str == "\\$itor")  newNode->realvalue = x;
					else log_abort();
				}
				goto apply_newNode;
			}

			if (str == "\\$sformatf") {
				Fmt fmt = processFormat(stage, /*sformat_like=*/true, /*default_base=*/10, /*first_arg_at=*/0, /*may_fail=*/true);
				newNode = AstNode::mkconst_str(location, fmt.render());
				goto apply_newNode;
			}

			if (str == "\\$countbits") {
				if (children.size() < 2)
					input_error("System function %s got %d arguments, expected at least 2.\n",
							RTLIL::unescape_id(str), int(children.size()));

				std::vector<RTLIL::State> control_bits;

				// Determine which bits to count
				for (size_t i = 1; i < children.size(); i++) {
					auto& node = children[i];
					while (node->simplify(true, stage, -1, false)) { }
					if (node->type != AST_CONSTANT)
						input_error("Failed to evaluate system function `%s' with non-constant control bit argument.\n", str);
					if (node->bits.size() != 1)
						input_error("Failed to evaluate system function `%s' with control bit width != 1.\n", str);
					control_bits.push_back(node->bits[0]);
				}

				// Detect width of exp (first argument of $countbits)
				int  exp_width = -1;
				bool exp_sign  = false;
				auto& exp = children[0];
				exp->detectSignWidth(exp_width, exp_sign, nullptr);

				newNode = mkconst_int(location, 0, false);

				for (int i = 0; i < exp_width; i++) {
					// Generate nodes for:  exp << i >> ($size(exp) - 1)
					//						  ^^   ^^
					auto lsh_node = std::make_unique<AstNode>(location, AST_SHIFT_LEFT, exp->clone(), mkconst_int(location, i, false));
					auto rsh_node = std::make_unique<AstNode>(location, AST_SHIFT_RIGHT, std::move(lsh_node), mkconst_int(location, exp_width - 1, false));

					std::unique_ptr<AstNode> or_node = nullptr;

					for (RTLIL::State control_bit : control_bits) {
						// Generate node for:  (exp << i >> ($size(exp) - 1)) === control_bit
						//													^^^
						auto eq_node = std::make_unique<AstNode>(location, AST_EQX, rsh_node->clone(), mkconst_bits(location, {control_bit}, false));

						// Or the result for each checked bit value
						if (or_node)
							or_node = std::make_unique<AstNode>(location, AST_LOGIC_OR, std::move(or_node), std::move(eq_node));
						else
							or_node = std::move(eq_node);
					}

					// We should have at least one element in control_bits,
					// because we checked for the number of arguments above
					log_assert(or_node != nullptr);

					// Generate node for adding with result of previous bit
					newNode = std::make_unique<AstNode>(location, AST_ADD, std::move(newNode), std::move(or_node));
				}

				goto apply_newNode;
			}

			if (str == "\\$countones" || str == "\\$isunknown" || str == "\\$onehot" || str == "\\$onehot0") {
				if (children.size() != 1)
					input_error("System function %s got %d arguments, expected 1.\n",
							RTLIL::unescape_id(str), int(children.size()));

				auto countbits = clone();
				countbits->str = "\\$countbits";

				if (str == "\\$countones") {
					countbits->children.push_back(mkconst_bits(location, {RTLIL::State::S1}, false));
					newNode = std::move(countbits);
				} else if (str == "\\$isunknown") {
					countbits->children.push_back(mkconst_bits(location, {RTLIL::Sx}, false));
					countbits->children.push_back(mkconst_bits(location, {RTLIL::Sz}, false));
					newNode = std::make_unique<AstNode>(location, AST_GT, std::move(countbits), mkconst_int(location, 0, false));
				} else if (str == "\\$onehot") {
					countbits->children.push_back(mkconst_bits(location, {RTLIL::State::S1}, false));
					newNode = std::make_unique<AstNode>(location, AST_EQ, std::move(countbits), mkconst_int(location, 1, false));
				} else if (str == "\\$onehot0") {
					countbits->children.push_back(mkconst_bits(location, {RTLIL::State::S1}, false));
					newNode = std::make_unique<AstNode>(location, AST_LE, std::move(countbits), mkconst_int(location, 1, false));
				} else {
					log_abort();
				}

				goto apply_newNode;
			}

			if (current_scope.count(str) != 0 && current_scope[str]->type == AST_DPI_FUNCTION)
			{
				AstNode *dpi_decl = current_scope[str];

				std::string rtype, fname;
				std::vector<std::string> argtypes;
				std::vector<std::unique_ptr<AstNode>> args;

				rtype = RTLIL::unescape_id(dpi_decl->children.at(0)->str);
				fname = RTLIL::unescape_id(dpi_decl->children.at(1)->str);
				auto dpi_type_ok = [](const std::string &t) {
					return t == "integer" || t == "real" || t == "shortreal" || t == "chandle";
				};
				bool dpi_ok = dpi_type_ok(rtype);

				for (int i = 2; i < GetSize(dpi_decl->children); i++)
				{
					if (i-2 >= GetSize(children))
						input_error("Insufficient number of arguments in DPI function call.\n");

					argtypes.push_back(RTLIL::unescape_id(dpi_decl->children.at(i)->str));
					if (!dpi_type_ok(argtypes.back()))
						dpi_ok = false;
					args.push_back(children.at(i-2)->clone());
					while (args.back()->simplify(true, stage, -1, false)) { }

					if (dpi_ok && args.back()->type != AST_CONSTANT && args.back()->type != AST_REALVALUE)
						input_error("Failed to evaluate DPI function with non-constant argument.\n");
				}

				if (!dpi_ok) {
					degrade(location.to_loc(), "DPI function `%s' is not evaluated; the call becomes constant 0.\n", fname.c_str());
					newNode = mkconst_int(location, 0, false);
					goto apply_newNode;
				}
				newNode = dpi_call(dpi_decl->location, rtype, fname, argtypes, args);

				goto apply_newNode;
			}

			if (current_scope.count(str) == 0)
				str = try_pop_module_prefix();
			if (current_scope.count(str) == 0 || current_scope[str]->type != AST_FUNCTION) {
				degrade(location.to_loc(), "Unresolved function `%s'; the call becomes constant 0.\n", str.c_str());
				newNode = mkconst_int(location, 0, false);
				goto apply_newNode;
			}
		}

		if (type == AST_TCALL)
		{
			if (str == "\\$readmemh" || str == "\\$readmemb")
			{
				if (GetSize(children) < 2 || GetSize(children) > 4)
					input_error("System function %s got %d arguments, expected 2-4.\n",
							RTLIL::unescape_id(str), int(children.size()));

				auto node_filename = children[0]->clone();
				while (node_filename->simplify(true, stage, width_hint, sign_hint)) { }
				if (node_filename->type != AST_CONSTANT) {
					// Runtime-variable filename (e.g. chosen by plusargs):
					// yosys only supports compile-time $readmem. Elide the
					// call instead of aborting the parse -- the file cannot
					// be resolved statically, and the memory keeps its
					// (possibly undriven) state, same as simulation before
					// the initial block runs.
					log_warning("Failed to evaluate system function `%s' with non-constant 1st argument; eliding the call (runtime $readmem is not supported).\n", str);
					newNode = std::make_unique<AstNode>(location, AST_BLOCK);
					goto apply_newNode;
				}

				auto node_memory = children[1]->clone();
				while (node_memory->simplify(true, stage, width_hint, sign_hint)) { }
				if (node_memory->type != AST_IDENTIFIER || node_memory->id2ast == nullptr || node_memory->id2ast->type != AST_MEMORY)
					input_error("Failed to evaluate system function `%s' with non-memory 2nd argument.\n", str);

				int start_addr = -1, finish_addr = -1;

				if (GetSize(children) > 2) {
					auto node_addr = children[2]->clone();
					while (node_addr->simplify(true, stage, width_hint, sign_hint)) { }
					if (node_addr->type != AST_CONSTANT)
						input_error("Failed to evaluate system function `%s' with non-constant 3rd argument.\n", str);
					start_addr = int(node_addr->asInt(false));
				}

				if (GetSize(children) > 3) {
					auto node_addr = children[3]->clone();
					while (node_addr->simplify(true, stage, width_hint, sign_hint)) { }
					if (node_addr->type != AST_CONSTANT)
						input_error("Failed to evaluate system function `%s' with non-constant 4th argument.\n", str);
					finish_addr = int(node_addr->asInt(false));
				}

				bool unconditional_init = false;
				if (current_always->type == AST_INITIAL) {
					pool<AstNode*> queue;
					log_assert(current_always->children[0]->type == AST_BLOCK);
					queue.insert(current_always->children[0].get());
					while (!unconditional_init && !queue.empty()) {
						pool<AstNode*> next_queue;
						for (auto& n : queue)
						for (auto& c : n->children) {
							if (c.get() == this)
								unconditional_init = true;
							next_queue.insert(c.get());
						}
						next_queue.swap(queue);
					}
				}

				newNode = readmem(str == "\\$readmemh", node_filename->bitsAsConst().decode_string(), node_memory->id2ast, start_addr, finish_addr, unconditional_init);
				goto apply_newNode;
			}

			if (current_scope.count(str) == 0)
				str = try_pop_module_prefix();
			if (current_scope.count(str) == 0 || current_scope[str]->type != AST_TASK) {
				// IEEE 1800 20.10 elaboration severity tasks are known to
				// genrtlil (dropped there); do not report them as
				// unresolved -- upstream's cells_sim libraries use $fatal
				// and their tests expect a warning-free elaboration.
				bool severity_task = str == "\\$fatal" || str == "\\$error" || str == "\\$warning" || str == "\\$info" ||
						str == "$fatal" || str == "$error" || str == "$warning" || str == "$info";
				if (!severity_task)
					degrade(location.to_loc(), "Unresolved task `%s'; the call is ignored.\n", str.c_str());
				newNode = std::make_unique<AstNode>(location, AST_BLOCK);
				goto apply_newNode;
			}
		}


		std::stringstream sstr;
		sstr << str << "$func$" << RTLIL::encode_filename(*location.begin.filename) << ":" << location.begin.line << "$" << (autoidx++) << '.';
		std::string prefix = sstr.str();

		auto* decl = current_scope[str];
		if (unevaluated_tern_branch && decl->is_recursive_function())
			goto replace_fcall_later;
		auto decl_clone = decl->clone();
		decl = decl_clone.get(); // sketchy?
		decl->replace_result_wire_name_in_function(str, "$result"); // enables recursion
		{
			// a wildcard-imported package function was registered under
			// its bare name (`\mix`) but its result variable and every
			// reference to it inside the body carry the package-qualified
			// name (`\r::mix`): rename that form too, or the inlined body
			// never drives `$result` (register v3 D7: the stress `func`
			// case read x where Verilator had the value)
			std::string base = str;
			if (!base.empty() && base[0] == '\\')
				base = base.substr(1);
			for (auto &c : decl->children) {
				if (!c || (c->type != AST_WIRE && c->type != AST_MEMORY) || c->str == "$result")
					continue;
				std::string cs = c->str;
				if (!cs.empty() && cs[0] == '\\')
					cs = cs.substr(1);
				if (cs.size() > base.size() + 2 && cs.compare(cs.size() - base.size() - 2, base.size() + 2, "::" + base) == 0) {
					decl->replace_result_wire_name_in_function(c->str, "$result");
					// the body may still spell the result variable by the bare
					// name (`plus1 = ...` / `return` inside a wildcard-imported
					// package function): the declaration was package-qualified by
					// rename_in_package_stmts, the references were not, and the
					// evaluator then saw an "assignment to a non-local variable"
					// (register v3 D7c).  The helper skips FCALL/TCALL nodes, so
					// a recursive call keeps its name.
					decl->replace_result_wire_name_in_function("\\" + base, "$result");
					break;
				}
			}
		}
		{
			// The package rename leaves the body assigning the result under
			// `<pkg>::<name>` (or the bare name) while the declaration may
			// already be `$result`; expand_genblock does not prefix a
			// `::`-qualified reference, so the inlined block drove a wire
			// nobody read and the call's value was x (register v3 D7d: the
			// stress `func` XFAIL).  Rename every such reference to `$result`
			// before the expansion, keyed on the references themselves.
			std::string base = str;
			if (!base.empty() && base[0] == '\\')
				base = base.substr(1);
			size_t bq = base.rfind("::");
			if (bq != std::string::npos)
				base = base.substr(bq + 2);
			std::set<std::string> refs;
			std::function<void(AstNode*)> scan = [&](AstNode *n) {
				for (auto &c : n->children) {
					if (!c)
						continue;
					if (c->type == AST_IDENTIFIER && c->str != "$result") {
						std::string cs = c->str;
						if (!cs.empty() && cs[0] == '\\')
							cs = cs.substr(1);
						size_t q = cs.rfind("::");
						std::string b = q == std::string::npos ? cs : cs.substr(q + 2);
						if (b == base && (q != std::string::npos || cs == base))
							refs.insert(c->str);
					}
					scan(c.get());
				}
			};
			scan(decl);
			for (auto &r : refs)
				decl->replace_result_wire_name_in_function(r, "$result");
		}
		decl->expand_genblock(prefix);

		if (decl->type == AST_FUNCTION && !decl->attributes.count(ID::via_celltype))
		{
			bool require_const_eval = decl->has_const_only_constructs();
			bool all_args_const = true;
			for (auto& child : children) {
				while (child->simplify(true, 1, -1, false)) { }
				if (child->type != AST_CONSTANT && child->type != AST_REALVALUE) {
					all_args_const = false;
					if (getenv("SVA_DEBUG_FCALL"))
						child->dumpAst(nullptr, "nonconst-arg> ");
				}
			}

			if (all_args_const) {
				auto func_workspace = decl->clone();
				func_workspace->set_in_param_flag(true);
				func_workspace->str = prefix_id(prefix, "$result");
				// func_workspace->dumpAst(stdout, "func_workspace ");
				newNode = func_workspace->eval_const_function(this, in_param || require_const_eval);
			}
			if (!newNode && (in_param || (require_const_eval && all_args_const))) {
				degrade(location.to_loc(), "Constant function `%s' did not evaluate; using constant 0.\n", str.c_str());
				newNode = AstNode::mkconst_int(location, 0, false);
			}
			if (newNode)
				goto apply_newNode;
		}

		if (probe_eval_depth > 0) {
			recursion_counter--;
			return did_something;
		}

		size_t arg_count = 0;
		dict<std::string, AstNode*> wire_cache;
		vector<std::unique_ptr<AstNode>> new_stmts;
		vector<std::unique_ptr<AstNode>> output_assignments;

		if (current_block == nullptr)
		{
			log_assert(type == AST_FCALL);

			std::unique_ptr<AstNode> wire = nullptr;
			std::string res_name = prefix_id(prefix, "$result");
			for (auto& child : decl->children)
				if ((child->type == AST_WIRE || child->type == AST_MEMORY) && child->str == res_name)
					wire = child->clone();
			if (!wire) {
				// a package function imported by wildcard keeps its result
				// wire under the package-qualified name
				// (`\r::mix$result`) while the call site's prefix is the
				// bare name: match the unique `$result` child by suffix
				// before giving up (this used to degrade to constant 0 on
				// a throwaway elaboration pass and warn once; strict mode
				// made it fatal: stress sim_equiv `func`, ibex_cheriot_ex)
				// the result variable is named after the function (the
				// package rename makes it `<prefix>r::mix`): match
				// `<prefix><base>` or `<prefix>...::<base>`
				std::string base = str;
				if (!base.empty() && base[0] == '\\')
					base = base.substr(1);
				size_t qp = base.rfind("::");
				if (qp != std::string::npos)
					base = base.substr(qp + 2);
				for (auto& child : decl->children) {
					if (child->type != AST_WIRE && child->type != AST_MEMORY)
						continue;
					const std::string &cs = child->str;
					if (cs.compare(0, prefix.size(), prefix) != 0)
						continue;
					std::string tail = cs.substr(prefix.size());
					if (tail == base || (tail.size() > base.size() + 2 &&
							tail.compare(tail.size() - base.size() - 2, base.size() + 2, "::" + base) == 0)) {
						wire = child->clone();
						wire->str = res_name;
						// the body still assigns the result under the matched
						// name; the call reads `res_name`.  Rename the references
						// in this per-call clone, or the inlined block drives a
						// wire nobody reads and the call's value is x (register
						// v3 D7d: the stress `func` XFAIL, wildcard-imported
						// package functions in continuous assigns).
						decl->replace_result_wire_name_in_function(cs, res_name);
						break;
					}
				}
			}
			if (!wire) {
				degrade(location.to_loc(), "Function `%s' has no result wire; using constant 0.\n", str.c_str());
				newNode = AstNode::mkconst_int(location, 0, false);
				goto apply_newNode;
			}

			wire->port_id = 0;
			wire->is_input = false;
			wire->is_output = false;

			auto* wire_leaky = wire.get();
			current_scope[wire->str] = wire_leaky;
			current_ast_mod->children.push_back(std::move(wire));
			while (wire_leaky->simplify(true, 1, -1, false)) { }

			auto lvalue = std::make_unique<AstNode>(location, AST_IDENTIFIER);
			lvalue->str = wire_leaky->str;

			auto always = std::make_unique<AstNode>(location, AST_ALWAYS, std::make_unique<AstNode>(location, AST_BLOCK,
					std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::move(lvalue), clone())));
			always->children[0]->children[0]->was_checked = true;

			current_ast_mod->children.push_back(std::move(always));

			goto replace_fcall_with_id;
		}

		if (decl->attributes.count(ID::via_celltype))
		{
			std::string celltype = decl->attributes.at(ID::via_celltype)->asAttrConst().decode_string();
			std::string outport = str;

			if (celltype.find(' ') != std::string::npos) {
				int pos = celltype.find(' ');
				outport = RTLIL::escape_id(celltype.substr(pos+1));
				celltype = RTLIL::escape_id(celltype.substr(0, pos));
			} else
				celltype = RTLIL::escape_id(celltype);

			auto cell = std::make_unique<AstNode>(location, AST_CELL, std::make_unique<AstNode>(location, AST_CELLTYPE));
			cell->str = prefix.substr(0, GetSize(prefix)-1);
			cell->children[0]->str = celltype;

			for (auto& attr : decl->attributes)
				if (attr.first.str().rfind("\\via_celltype_defparam_", 0) == 0)
				{
					auto cell_arg = std::make_unique<AstNode>(location, AST_PARASET, attr.second->clone());
					cell_arg->str = RTLIL::escape_id(attr.first.substr(strlen("\\via_celltype_defparam_")));
					cell->children.push_back(std::move(cell_arg));
				}

			for (auto& child : decl->children)
				if (child->type == AST_WIRE && (child->is_input || child->is_output || (type == AST_FCALL && child->str == str)))
				{
					auto wire = child->clone();
					wire->port_id = 0;
					wire->is_input = false;
					wire->is_output = false;
					current_ast_mod->children.push_back(std::move(wire));
					while (wire->simplify(true, 1, -1, false)) { }

					auto wire_id = std::make_unique<AstNode>(location, AST_IDENTIFIER);
					wire_id->str = wire->str;

					if ((child->is_input || child->is_output) && arg_count < children.size())
					{
						auto arg = children[arg_count++]->clone();
						auto assign = child->is_input ?
								std::make_unique<AstNode>(location, AST_ASSIGN_EQ, wire_id->clone(), std::move(arg)) :
								std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::move(arg), wire_id->clone());
						assign->children[0]->was_checked = true;

						for (auto it = current_block->children.begin(); it != current_block->children.end(); it++) {
							if (it->get() != current_block_child)
								continue;
							current_block->children.insert(it, std::move(assign));
							break;
						}
					}

					auto cell_arg = std::make_unique<AstNode>(location, AST_ARGUMENT, std::move(wire_id));
					cell_arg->str = child->str == str ? outport : child->str;
					cell->children.push_back(std::move(cell_arg));
				}

			current_ast_mod->children.push_back(std::move(cell));
			goto replace_fcall_with_id;
		}

		// A typedef local to the subroutine (IEEE 1800-2017 13.3, 13.4) is
		// hoisted like its wires, under the call's prefix and before them, so
		// a local `p_t v;` resolves to it. Left in the body it reached the
		// caller's block as a statement (an assertion in an unnamed block).
		// A local enum type is hoisted too, renamed per call (its items
		// already carry the call's prefix); its users name it by the
		// enum_type attribute.
		{
			dict<std::string, std::string> enum_rename;
			for (auto& child : decl->children)
				if (child->type == AST_ENUM && !child->str.empty())
					enum_rename[child->str] = prefix_id(prefix, child->str);
			auto rename_enum_type = [&](AstNode *n) {
				if (n && n->attributes.count(ID::enum_type) && enum_rename.count(n->attributes.at(ID::enum_type)->str)) {
					std::string en = enum_rename.at(n->attributes.at(ID::enum_type)->str);
					n->attributes[ID::enum_type] = AstNode::mkconst_str(n->location, en);
				}
			};
			for (auto& child : decl->children) {
				rename_enum_type(child.get());
				if (child->type == AST_TYPEDEF && !child->children.empty())
					rename_enum_type(child->children[0].get());
			}
			for (auto& child : decl->children)
				if (child->type == AST_ENUM || child->type == AST_TYPEDEF) {
					current_ast_mod->children.push_back(child->clone());
					AstNode *td = current_ast_mod->children.back().get();
					if (td->type == AST_ENUM) {
						td->str = enum_rename.at(td->str);
						for (auto& item : td->children)
							current_scope[item->str] = item.get();
					}
					current_scope[td->str] = td;
					while (td->simplify(true, 1, -1, false)) { }
				}
		}

		for (auto& child : decl->children)
			if (child->type == AST_WIRE || child->type == AST_MEMORY || child->type == AST_PARAMETER || child->type == AST_LOCALPARAM || child->type == AST_ENUM_ITEM)
			{
				AstNode *wire = nullptr;
				std::unique_ptr<AstNode> port_default;
				RTLIL::IdString default_attr = RTLIL::IdString("\\sv_default");
				if (child->attributes.count(default_attr))
					port_default = child->attributes.at(default_attr)->clone();

				if (wire_cache.count(child->str))
				{
					wire = wire_cache.at(child->str);
					bool contains_value = wire->type == AST_LOCALPARAM;
					if (wire->children.size() == contains_value) {
						for (auto& c : child->children)
							wire->children.push_back(c->clone());
					} else if (!child->children.empty()) {
						while (child->simplify(true, stage, -1, false)) { }
						if (GetSize(child->children) == GetSize(wire->children) - contains_value) {
							for (int i = 0; i < GetSize(child->children); i++)
								if (*child->children.at(i) != *wire->children.at(i + contains_value))
									goto tcall_incompatible_wires;
						} else {
					tcall_incompatible_wires:
							input_error("Incompatible re-declaration of wire %s.\n", child->str);
						}
					}
				}
				else
				{
					current_ast_mod->children.push_back(child->clone());
					wire = current_ast_mod->children.back().get();
					wire->attributes.erase(default_attr);
					wire->port_id = 0;
					wire->is_input = false;
					wire->is_output = false;
					wire->is_reg = true;
					wire->set_attribute(ID::nosync, AstNode::mkconst_int(location, 1, false));
					if (child->type == AST_ENUM_ITEM)
						wire->set_attribute(ID::enum_base_type, std::move(child->attributes[ID::enum_base_type]));

					wire_cache[child->str] = wire;

					current_scope[wire->str] = wire;
				}

				while (wire->simplify(true, 1, -1, false)) { }

				if ((child->is_input || child->is_output) && (arg_count < children.size() || port_default))
				{
					std::unique_ptr<AstNode> arg;
					if (arg_count < children.size())
						arg = children[arg_count++]->clone();
					else
						arg = port_default->clone();
					// convert purely constant arguments into localparams
					if (child->is_input && !child->get_bool_attribute(ID(sv_ref)) && child->type == AST_WIRE && arg->type == AST_CONSTANT && node_contains_assignment_to(decl, child.get())) {
						wire->type = AST_LOCALPARAM;
						wire->attributes.erase(ID::nosync);
						wire->children.insert(wire->children.begin(), arg->clone());
						// args without a range implicitly have width 1
						if (wire->children.back()->type != AST_RANGE) {
							// check if this wire is redeclared with an explicit size
							bool uses_explicit_size = false;
							for (auto& other_child : decl->children)
								if (other_child->type == AST_WIRE && child->str == other_child->str
										&& !other_child->children.empty()
										&& other_child->children.back()->type == AST_RANGE) {
									uses_explicit_size = true;
									break;
								}
							if (!uses_explicit_size) {
								auto range = std::make_unique<AstNode>(location);
								range->type = AST_RANGE;
								range->children.push_back(mkconst_int(location, 0, true));
								range->children.push_back(mkconst_int(location, 0, true));
								wire->children.push_back(std::move(range));
							}
						}
						wire->fixup_hierarchy_flags();
						// updates the sizing
						while (wire->simplify(true, 1, -1, false)) { }
						continue;
					}

					auto wire_id = std::make_unique<AstNode>(location, AST_IDENTIFIER);
					wire_id->str = wire->str;
					if (child->is_input) {
						auto assign = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, wire_id->clone(), arg->clone());
						assign->children[0]->was_checked = true;
						new_stmts.push_back(std::move(assign));
					}

					if (child->is_output) {
						auto assign = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, arg->clone(), wire_id->clone());
						assign->children[0]->was_checked = true;
						output_assignments.push_back(std::move(assign));
					}
				}
			}

		for (auto& child : decl->children)
			if (child->type != AST_WIRE && child->type != AST_MEMORY && child->type != AST_PARAMETER && child->type != AST_LOCALPARAM && child->type != AST_TYPEDEF && child->type != AST_ENUM)
				new_stmts.push_back(child->clone());

		// A local's declaration initializer (`logic d = 1'b0;`) is parsed
		// as an `initial` block; inlined, it would be hoisted to the module
		// and act once at time zero, leaving the local x on every call
		// (stock yosys rejects it as "Invalid nesting"). Run it in place as
		// a blocking assignment (IEEE 1800-2017 13.4.2: an automatic
		// function's variables are initialized on each call).
		inline_function_initializers(new_stmts);

		// return exits only the function. Later statements run on the other paths.
		absorb_function_return(new_stmts);
		strip_function_return(new_stmts);
		{
			bool return_in_loop = false;
			for (auto &stmt : new_stmts)
				if (contains_function_return(stmt.get()))
					return_in_loop = true;
			if (return_in_loop) {
				auto scope = std::make_unique<AstNode>(location, AST_BLOCK);
				scope->str = prefix + "return";
				scope->set_attribute(ID(sv_noexpand), AstNode::mkconst_int(location, 1, false));
				scope->set_attribute(ID(sv_return_scope), AstNode::mkconst_int(location, 1, false));
				for (auto &stmt : new_stmts)
					scope->children.push_back(std::move(stmt));
				new_stmts.clear();
				new_stmts.push_back(std::move(scope));
			}
		}

		new_stmts.reserve(new_stmts.size() + output_assignments.size());
		std::move(output_assignments.begin(), output_assignments.end(), std::back_inserter(new_stmts));

		for (auto it = current_block->children.begin(); ; it++) {
			log_assert(it != current_block->children.end());
			if (it->get() == current_block_child) {
				current_block->children.insert(it,
					std::make_move_iterator(new_stmts.begin()),
					std::make_move_iterator(new_stmts.end()));
				break;
			}
		}

	replace_fcall_with_id:
		if (type == AST_FCALL) {
			// delete_children() also drops the attributes; a call that is
			// the value of a named pattern element keeps its member key
			std::vector<std::pair<RTLIL::IdString, std::unique_ptr<AstNode>>> pattern_keys;
			for (auto key : {ID(sv_member), ID(sv_typekey), ID(sv_index)})
				if (attributes.count(key))
					pattern_keys.emplace_back(key, std::move(attributes.at(key)));
			delete_children();
			for (auto &[key, val] : pattern_keys)
				attributes[key] = std::move(val);
			type = AST_IDENTIFIER;
			str = prefix_id(prefix, "$result");
		}
		if (type == AST_TCALL)
			str = "";
		did_something = true;
	}

replace_fcall_later:;

	// perform const folding when activated
	if (const_fold)
	{
		bool string_op;
		std::vector<RTLIL::State> tmp_bits;
		RTLIL::Const (*const_func)(const RTLIL::Const&, const RTLIL::Const&, bool, bool, int);
		RTLIL::Const dummy_arg;

		switch (type)
		{
		case AST_IDENTIFIER: {
			// Package members carry package-qualified names, so a qualified
			// reference (pkg::PARAM) from inside another package is found via
			// id2ast rather than current_scope; prefer scope, fall back to id2ast.
			AstNode *pointee = nullptr;
			if (current_scope.count(str) > 0)
				pointee = current_scope[str];
			else if (id2ast)
				pointee = id2ast;
			if (pointee && (pointee->type == AST_PARAMETER || pointee->type == AST_LOCALPARAM || pointee->type == AST_ENUM_ITEM)) {
				AstNode *pval = pointee->children[0].get();
				// The parameter's own value may not be folded yet when a
				// module references it (package params fold after module
				// processing in the deferred flow). Fold simple arithmetic
				// values in place so the substitution below can proceed, and
				// flag progress so the simplify loop retries this node.
				if (pval->type != AST_CONSTANT && !pval->isConst() && expr_is_plain_arith(pval)) {
					while (pval->simplify(true, stage, -1, false))
						did_something = true;
				}
				// Per-module clones of package parameters can lag the folded
				// original (cross-package ordering); consult the design's own
				// package copy, whose simplify pass already ran.
				if (pval->type != AST_CONSTANT && !pval->isConst() && expr_is_plain_arith(pval)) {
					while (pval->simplify(true, stage, -1, false))
						did_something = true;
				}
				if (pval->type == AST_CONSTANT) {
					if (children.size() != 0 && children[0]->type == AST_RANGE && children[0]->range_valid) {
						std::vector<RTLIL::State> data;
						bool param_upto = pointee->range_valid && pointee->range_swapped;
						int param_offset = pointee->range_valid ? pointee->range_right : 0;
						int param_width = pointee->range_valid ? pointee->range_left - pointee->range_right + 1 :
								GetSize(pval->bits);
						int tmp_range_left = children[0]->range_left, tmp_range_right = children[0]->range_right;
						if (param_upto) {
							tmp_range_left = (param_width + 2*param_offset) - children[0]->range_right - 1;
							tmp_range_right = (param_width + 2*param_offset) - children[0]->range_left - 1;
						}
						AstNode *member_node = get_struct_member();
						int chunk_offset = member_node ? member_node->range_right : 0;
						log_assert(!(chunk_offset && param_upto));
						for (int i = tmp_range_right; i <= tmp_range_left; i++) {
							int index = i - param_offset;
							if (0 <= index && index < param_width)
								data.push_back(pval->bits[chunk_offset + index]);
							else
								data.push_back(RTLIL::State::Sx);
						}
						newNode = mkconst_bits(location, data, false);
					} else
					if (children.size() == 0)
						newNode = pval->clone();
				} else
				if (pval->isConst())
					newNode = pval->clone();
			}
			break;
		}
		case AST_BIT_NOT:
			if (children[0]->type == AST_CONSTANT) {
				RTLIL::Const y = RTLIL::const_not(children[0]->bitsAsConst(width_hint, sign_hint), dummy_arg, sign_hint, false, width_hint);
				newNode = mkconst_bits(location, y.to_bits(), sign_hint);
			}
			break;
		case AST_TO_SIGNED:
		case AST_TO_UNSIGNED:
			if (children[0]->type == AST_CONSTANT) {
				RTLIL::Const y = children[0]->bitsAsConst(width_hint, sign_hint);
				newNode = mkconst_bits(location, y.to_bits(), type == AST_TO_SIGNED);
			}
			break;
		if (0) { case AST_BIT_AND:  const_func = RTLIL::const_and;  }
		if (0) { case AST_BIT_OR:   const_func = RTLIL::const_or;   }
		if (0) { case AST_BIT_XOR:  const_func = RTLIL::const_xor;  }
		if (0) { case AST_BIT_XNOR: const_func = RTLIL::const_xnor; }
			if (children[0]->type == AST_CONSTANT && children[1]->type == AST_CONSTANT) {
				RTLIL::Const y = const_func(children[0]->bitsAsConst(width_hint, sign_hint),
						children[1]->bitsAsConst(width_hint, sign_hint), sign_hint, sign_hint, width_hint);
				newNode = mkconst_bits(location, y.to_bits(), sign_hint);
			}
			break;
		if (0) { case AST_REDUCE_AND:  const_func = RTLIL::const_reduce_and;  }
		if (0) { case AST_REDUCE_OR:   const_func = RTLIL::const_reduce_or;   }
		if (0) { case AST_REDUCE_XOR:  const_func = RTLIL::const_reduce_xor;  }
		if (0) { case AST_REDUCE_XNOR: const_func = RTLIL::const_reduce_xnor; }
		if (0) { case AST_REDUCE_BOOL: const_func = RTLIL::const_reduce_bool; }
			if (children[0]->type == AST_CONSTANT) {
				RTLIL::Const y = const_func(RTLIL::Const(children[0]->bits), dummy_arg, false, false, -1);
				newNode = mkconst_bits(location, y.to_bits(), false);
			}
			break;
		case AST_LOGIC_NOT:
			if (children[0]->type == AST_CONSTANT) {
				RTLIL::Const y = RTLIL::const_logic_not(RTLIL::Const(children[0]->bits), dummy_arg, children[0]->is_signed, false, -1);
				newNode = mkconst_bits(location, y.to_bits(), false);
			} else
			if (children[0]->isConst()) {
				newNode = mkconst_int(location, children[0]->asReal(sign_hint) == 0, false, 1);
			}
			break;
		if (0) { case AST_LOGIC_AND: const_func = RTLIL::const_logic_and; }
		if (0) { case AST_LOGIC_OR:  const_func = RTLIL::const_logic_or;  }
			if (children[0]->type == AST_CONSTANT && children[1]->type == AST_CONSTANT) {
				RTLIL::Const y = const_func(RTLIL::Const(children[0]->bits), RTLIL::Const(children[1]->bits),
						children[0]->is_signed, children[1]->is_signed, -1);
				newNode = mkconst_bits(location, y.to_bits(), false);
			} else
			if (children[0]->isConst() && children[1]->isConst()) {
				if (type == AST_LOGIC_AND)
					newNode = mkconst_int(location, (children[0]->asReal(sign_hint) != 0) && (children[1]->asReal(sign_hint) != 0), false, 1);
				else
					newNode = mkconst_int(location, (children[0]->asReal(sign_hint) != 0) || (children[1]->asReal(sign_hint) != 0), false, 1);
			}
			break;
		if (0) { case AST_SHIFT_LEFT:   const_func = RTLIL::const_shl;  }
		if (0) { case AST_SHIFT_RIGHT:  const_func = RTLIL::const_shr;  }
		if (0) { case AST_SHIFT_SLEFT:  const_func = RTLIL::const_sshl; }
		if (0) { case AST_SHIFT_SRIGHT: const_func = RTLIL::const_sshr; }
		if (0) { case AST_POW:		  const_func = RTLIL::const_pow; }
			if (children[0]->type == AST_CONSTANT && children[1]->type == AST_CONSTANT) {
				RTLIL::Const y = const_func(children[0]->bitsAsConst(width_hint, sign_hint),
						RTLIL::Const(children[1]->bits), sign_hint, type == AST_POW ? children[1]->is_signed : false, width_hint);
				newNode = mkconst_bits(location, y.to_bits(), sign_hint);
			} else
			if (type == AST_POW && children[0]->isConst() && children[1]->isConst()) {
				newNode = std::make_unique<AstNode>(location, AST_REALVALUE);
				newNode->realvalue = pow(children[0]->asReal(sign_hint), children[1]->asReal(sign_hint));
			}
			break;
		if (0) { case AST_LT:  const_func = RTLIL::const_lt; }
		if (0) { case AST_LE:  const_func = RTLIL::const_le; }
		if (0) { case AST_EQ:  const_func = RTLIL::const_eq; }
		if (0) { case AST_NE:  const_func = RTLIL::const_ne; }
		if (0) { case AST_EQX: const_func = RTLIL::const_eqx; }
		if (0) { case AST_NEX: const_func = RTLIL::const_nex; }
		if (0) { case AST_GE:  const_func = RTLIL::const_ge; }
		if (0) { case AST_GT:  const_func = RTLIL::const_gt; }
			if (children[0]->type == AST_CONSTANT && children[1]->type == AST_CONSTANT) {
				int cmp_width = max(children[0]->bits.size(), children[1]->bits.size());
				bool cmp_signed = children[0]->is_signed && children[1]->is_signed;
				RTLIL::Const y = const_func(children[0]->bitsAsConst(cmp_width, cmp_signed),
						children[1]->bitsAsConst(cmp_width, cmp_signed), cmp_signed, cmp_signed, 1);
				newNode = mkconst_bits(location, y.to_bits(), false);
			} else
			if (children[0]->isConst() && children[1]->isConst()) {
				bool cmp_signed = (children[0]->type == AST_REALVALUE || children[0]->is_signed) && (children[1]->type == AST_REALVALUE || children[1]->is_signed);
				switch (type) {
				case AST_LT:  newNode = mkconst_int(location, children[0]->asReal(cmp_signed) <  children[1]->asReal(cmp_signed), false, 1); break;
				case AST_LE:  newNode = mkconst_int(location, children[0]->asReal(cmp_signed) <= children[1]->asReal(cmp_signed), false, 1); break;
				case AST_EQ:  newNode = mkconst_int(location, children[0]->asReal(cmp_signed) == children[1]->asReal(cmp_signed), false, 1); break;
				case AST_NE:  newNode = mkconst_int(location, children[0]->asReal(cmp_signed) != children[1]->asReal(cmp_signed), false, 1); break;
				case AST_EQX: newNode = mkconst_int(location, children[0]->asReal(cmp_signed) == children[1]->asReal(cmp_signed), false, 1); break;
				case AST_NEX: newNode = mkconst_int(location, children[0]->asReal(cmp_signed) != children[1]->asReal(cmp_signed), false, 1); break;
				case AST_GE:  newNode = mkconst_int(location, children[0]->asReal(cmp_signed) >= children[1]->asReal(cmp_signed), false, 1); break;
				case AST_GT:  newNode = mkconst_int(location, children[0]->asReal(cmp_signed) >  children[1]->asReal(cmp_signed), false, 1); break;
				default: log_abort();
				}
			}
			break;
		if (0) { case AST_ADD: const_func = RTLIL::const_add; }
		if (0) { case AST_SUB: const_func = RTLIL::const_sub; }
		if (0) { case AST_MUL: const_func = RTLIL::const_mul; }
		if (0) { case AST_DIV: const_func = RTLIL::const_div; }
		if (0) { case AST_MOD: const_func = RTLIL::const_mod; }
			if (children[0]->type == AST_CONSTANT && children[1]->type == AST_CONSTANT) {
				RTLIL::Const y = const_func(children[0]->bitsAsConst(width_hint, sign_hint),
						children[1]->bitsAsConst(width_hint, sign_hint), sign_hint, sign_hint, width_hint);
				newNode = mkconst_bits(location, y.to_bits(), sign_hint);
			} else
			if (children[0]->isConst() && children[1]->isConst()) {
				newNode = std::make_unique<AstNode>(location, AST_REALVALUE);
				switch (type) {
				case AST_ADD: newNode->realvalue = children[0]->asReal(sign_hint) + children[1]->asReal(sign_hint); break;
				case AST_SUB: newNode->realvalue = children[0]->asReal(sign_hint) - children[1]->asReal(sign_hint); break;
				case AST_MUL: newNode->realvalue = children[0]->asReal(sign_hint) * children[1]->asReal(sign_hint); break;
				case AST_DIV: newNode->realvalue = children[0]->asReal(sign_hint) / children[1]->asReal(sign_hint); break;
				case AST_MOD: newNode->realvalue = fmod(children[0]->asReal(sign_hint), children[1]->asReal(sign_hint)); break;
				default: log_abort();
				}
			}
			break;
		if (0) { case AST_SELFSZ: const_func = RTLIL::const_pos; }
		if (0) { case AST_POS: const_func = RTLIL::const_pos; }
		if (0) { case AST_NEG: const_func = RTLIL::const_neg; }
			if (children[0]->type == AST_CONSTANT) {
				RTLIL::Const y = const_func(children[0]->bitsAsConst(width_hint, sign_hint), dummy_arg, sign_hint, false, width_hint);
				newNode = mkconst_bits(location, y.to_bits(), sign_hint);
			} else
			if (children[0]->isConst()) {
				newNode = std::make_unique<AstNode>(location, AST_REALVALUE);
				if (type == AST_NEG)
					newNode->realvalue = -children[0]->asReal(sign_hint);
				else
					newNode->realvalue = +children[0]->asReal(sign_hint);
			}
			break;
		case AST_TERNARY:
			if (children[0]->isConst())
			{
				auto pair = get_tern_choice();
				AstNode *choice = pair.first;
				AstNode *not_choice = pair.second;

				if (choice != nullptr) {
					if (choice->type == AST_CONSTANT) {
						int other_width_hint = width_hint;
						bool other_sign_hint = sign_hint, other_real = false;
						not_choice->detectSignWidth(other_width_hint, other_sign_hint, &other_real);
						if (other_real) {
							newNode = std::make_unique<AstNode>(location, AST_REALVALUE);
							choice->detectSignWidth(width_hint, sign_hint);
							newNode->realvalue = choice->asReal(sign_hint);
						} else {
							RTLIL::Const y = choice->bitsAsConst(width_hint, sign_hint);
							if (choice->is_string && y.size() % 8 == 0 && sign_hint == false)
								newNode = mkconst_str(location, y.to_bits());
							else
								newNode = mkconst_bits(location, y.to_bits(), sign_hint);
						}
					} else
					if (choice->isConst()) {
						newNode = choice->clone();
					}
				} else if (children[1]->type == AST_CONSTANT && children[2]->type == AST_CONSTANT) {
					RTLIL::Const a = children[1]->bitsAsConst(width_hint, sign_hint);
					RTLIL::Const b = children[2]->bitsAsConst(width_hint, sign_hint);
					log_assert(a.size() == b.size());
					for (auto i = 0; i < a.size(); i++)
						if (a[i] != b[i])
							a.set(i, RTLIL::State::Sx);
					newNode = mkconst_bits(location, a.to_bits(), sign_hint);
				} else if (children[1]->isConst() && children[2]->isConst()) {
					newNode = std::make_unique<AstNode>(location, AST_REALVALUE);
					if (children[1]->asReal(sign_hint) == children[2]->asReal(sign_hint))
						newNode->realvalue = children[1]->asReal(sign_hint);
					else
						// IEEE Std 1800-2012 Sec. 11.4.11 states that the entry in Table 7-1 for
						// the data type in question should be returned if the ?: is ambiguous. The
						// value in Table 7-1 for the 'real' type is 0.0.
						newNode->realvalue = 0.0;
				}
			}
			break;
		case AST_CAST_SIZE:
			if (children.at(0)->type == AST_CONSTANT && children.at(1)->type == AST_CONSTANT) {
				int width = children[0]->bitsAsConst().as_int();
				RTLIL::Const val;
				if (children[1]->is_unsized)
					val = children[1]->bitsAsUnsizedConst(width);
				else
					val = children[1]->bitsAsConst(width);
				newNode = mkconst_bits(location, val.to_bits(), children[1]->is_signed);
			}
			break;
		case AST_CONCAT:
			string_op = !children.empty();
			for (auto it = children.begin(); it != children.end(); it++) {
				if ((*it)->type != AST_CONSTANT)
					goto not_const;
				if (!(*it)->is_string)
					string_op = false;
				tmp_bits.insert(tmp_bits.end(), (*it)->bits.begin(), (*it)->bits.end());
			}
			newNode = string_op ? mkconst_str(location, tmp_bits) : mkconst_bits(location, tmp_bits, false);
			break;
		case AST_REPLICATE:
			if (attributes.count(ID(sv_pattern_rep)))
				goto not_const;
			if (children.at(0)->type != AST_CONSTANT || children.at(1)->type != AST_CONSTANT)
				goto not_const;
			for (int i = 0; i < children[0]->bitsAsConst().as_int(); i++)
				tmp_bits.insert(tmp_bits.end(), children.at(1)->bits.begin(), children.at(1)->bits.end());
			newNode = children.at(1)->is_string ? mkconst_str(location, tmp_bits) : mkconst_bits(location, tmp_bits, false);
			break;
		case AST_ASSIGN_PATTERN:
			goto not_const;
		default:
		not_const:
			break;
		}
	}

	// if any of the above set 'newNode' -> use 'newNode' as template to update 'this'
	if (newNode) {
apply_newNode:
		// fprintf(stderr, "----\n");
		// dumpAst(stderr, "- ");
		// newNode->dumpAst(stderr, "+ ");
		log_assert(newNode != nullptr);
		newNode->location.begin.filename = location.begin.filename;
		newNode->location = location;
		// (cloneInto keeps this node's assignment-pattern key)
		newNode->cloneInto(*this);
		fixup_hierarchy_flags();
		did_something = true;
	}

	if (!did_something)
		basic_prep = true;


	recursion_counter--;
	return did_something;
}

void AstNode::replace_result_wire_name_in_function(const std::string &from, const std::string &to)
{
	for (auto& child : children)
		child->replace_result_wire_name_in_function(from, to);
	if (str == from && type != AST_FCALL && type != AST_TCALL)
		str = to;
}

// replace a readmem[bh] TCALL ast node with a block of memory assignments
std::unique_ptr<AstNode> AstNode::readmem(bool is_readmemh, std::string mem_filename, AstNode *memory, int start_addr, int finish_addr, bool unconditional_init)
{
	int mem_width, mem_size, addr_bits;
	memory->meminfo(mem_width, mem_size, addr_bits);

	auto block = std::make_unique<AstNode>(location, AST_BLOCK);

	AstNode* meminit = nullptr;
	int next_meminit_cursor=0;
	vector<State> meminit_bits;
	vector<State> en_bits;
	int meminit_size=0;

	for (int i = 0; i < mem_width; i++)
		en_bits.push_back(State::S1);

	std::ifstream f;
	f.open(mem_filename.c_str());
	if (f.fail()) {
		std::string path = parent_from_file_path(*location.begin.filename);
		f.open(path + mem_filename.c_str());
		yosys_input_files.insert(path + mem_filename);
	} else {
		yosys_input_files.insert(mem_filename);
	}
	if (f.fail() || GetSize(mem_filename) == 0)
		input_error("Can not open file `%s` for %s.\n", mem_filename, str);

	log_assert(GetSize(memory->children) == 2 && memory->children[1]->type == AST_RANGE && memory->children[1]->range_valid);
	int range_left =  memory->children[1]->range_left, range_right =  memory->children[1]->range_right;
	int range_min = min(range_left, range_right), range_max = max(range_left, range_right);

	if (start_addr < 0)
		start_addr = range_min;

	if (finish_addr < 0)
		finish_addr = range_max + 1;

	bool in_comment = false;
	int increment = start_addr <= finish_addr ? +1 : -1;
	int cursor = start_addr;

	while (!f.eof())
	{
		std::string line, token;
		std::getline(f, line);

		for (int i = 0; i < GetSize(line); i++) {
			if (in_comment && line.compare(i, 2, "*/") == 0) {
				line[i] = ' ';
				line[i+1] = ' ';
				in_comment = false;
				continue;
			}
			if (!in_comment && line.compare(i, 2, "/*") == 0)
				in_comment = true;
			if (in_comment)
				line[i] = ' ';
		}

		while (1)
		{
			token = next_token(line, " \t\r\n");
			if (token.empty() || token.compare(0, 2, "//") == 0)
				break;

			if (token[0] == '@') {
				token = token.substr(1);
				const char *nptr = token.c_str();
				char *endptr;
				cursor = strtol(nptr, &endptr, 16);
				if (!*nptr || *endptr)
					input_error("Can not parse address `%s` for %s.\n", nptr, str);
				continue;
			}

			VERILOG_FRONTEND::ConstParser p{memory->location};
			auto value = p.const2ast(stringf("%d'%c", mem_width, is_readmemh ? 'h' : 'b') + token);

			if (unconditional_init)
			{
				if (meminit == nullptr || cursor != next_meminit_cursor)
				{
					if (meminit != nullptr) {
						meminit->children[1] = AstNode::mkconst_bits(location, meminit_bits, false);
						meminit->children[3] = AstNode::mkconst_int(location, meminit_size, false);
					}

					auto meminit_owned = std::make_unique<AstNode>(location, AST_MEMINIT);
					meminit = meminit_owned.get();
					meminit->children.push_back(AstNode::mkconst_int(location, cursor, false));
					meminit->children.push_back(nullptr);
					meminit->children.push_back(AstNode::mkconst_bits(location, en_bits, false));
					meminit->children.push_back(nullptr);
					meminit->str = memory->str;
					meminit->id2ast = memory;
					meminit_bits.clear();
					meminit_size = 0;

					current_ast_mod->children.push_back(std::move(meminit_owned));
					next_meminit_cursor = cursor;
				}

				meminit_size++;
				next_meminit_cursor++;
				meminit_bits.insert(meminit_bits.end(), value->bits.begin(), value->bits.end());
			}
			else
			{
				block->children.push_back(
					std::make_unique<AstNode>(location,
						AST_ASSIGN_EQ, std::make_unique<AstNode>(location,
							AST_IDENTIFIER, std::make_unique<AstNode>(location,
								AST_RANGE, AstNode::mkconst_int(location,
									cursor, false))),
							std::move(value)));
				block->children.back()->children[0]->str = memory->str;
				block->children.back()->children[0]->id2ast = memory;
				block->children.back()->children[0]->was_checked = true;
			}

			cursor += increment;
			if ((cursor == finish_addr+increment) || (increment > 0 && cursor > range_max) || (increment < 0 && cursor < range_min))
				break;
		}

		if ((cursor == finish_addr+increment) || (increment > 0 && cursor > range_max) || (increment < 0 && cursor < range_min))
			break;
	}

	if (meminit != nullptr) {
		meminit->children[1] = AstNode::mkconst_bits(location, meminit_bits, false);
		meminit->children[3] = AstNode::mkconst_int(location, meminit_size, false);
	}

	return block;
}

// annotate the names of all wires and other named objects in a named generate
// or procedural block; nested blocks are themselves annotated such that the
// prefix is carried forward, but resolution of their children is deferred
void AstNode::expand_genblock(const std::string &prefix)
{
	if (type == AST_IDENTIFIER || type == AST_FCALL || type == AST_TCALL || type == AST_WIRETYPE || type == AST_PREFIX) {
		log_assert(!str.empty());

		// search starting in the innermost scope and then stepping outward
		for (size_t ppos = prefix.size() - 1; ppos; --ppos) {
			if (prefix.at(ppos) != '.') continue;

			std::string new_prefix = prefix.substr(0, ppos + 1);
			auto attempt_resolve = [&new_prefix](const std::string &ident) -> std::string {
				std::string new_name = prefix_id(new_prefix, ident);
				if (current_scope.count(new_name))
					return new_name;
				return {};
			};

			// attempt to resolve the full identifier
			std::string resolved = attempt_resolve(str);
			if (!resolved.empty()) {
				str = resolved;
				break;
			}

			// attempt to resolve hierarchical prefixes within the identifier,
			// as the prefix could refer to a local scope which exists but
			// hasn't yet been elaborated
			for (size_t spos = str.size() - 1; spos; --spos) {
				if (str.at(spos) != '.') continue;
				resolved = attempt_resolve(str.substr(0, spos));
				if (!resolved.empty()) {
					str = resolved + str.substr(spos);
					ppos = 1; // break outer loop
					break;
				}
			}

		}
	}

	auto prefix_node = [&prefix](AstNode* child) {
		if (child->str.empty()) return;
		std::string new_name = prefix_id(prefix, child->str);
		if (child->type == AST_FUNCTION)
			child->replace_result_wire_name_in_function(child->str, new_name);
		else
			child->str = new_name;
		current_scope[new_name] = child;
	};

	for (size_t i = 0; i < children.size(); i++) {
		auto* child = children[i].get();
		if (!child)
			continue;
		// the parent scope prefixed this enum's items (case AST_ENUM below);
		// prefixing them again here made `\g.g.A` for an item `A` of block g
		if (type == AST_ENUM && child->type == AST_ENUM_ITEM)
			continue;

		switch (child->type) {
		case AST_WIRE:
		case AST_AUTOWIRE:
		case AST_MEMORY:
		case AST_STRUCT:
		case AST_UNION:
		case AST_PARAMETER:
		case AST_LOCALPARAM:
		case AST_FUNCTION:
		case AST_TASK:
		case AST_CELL:
		case AST_TYPEDEF:
		case AST_ENUM_ITEM:
		case AST_GENVAR:
			prefix_node(child);
			break;

		// IEEE 1800-2017 23.6: a labelled assertion in a scope is named
		// relative to it (symbol_in[1].wp, symbol_in[2].wp, ...), like the
		// scope's wires and cells; unprefixed, every generate iteration
		// produced the same $check cell name.
		case AST_ASSERT:
		case AST_ASSUME:
		case AST_LIVE:
		case AST_FAIR:
		case AST_COVER:
			if (!child->str.empty())
				child->str = prefix_id(prefix, child->str);
			break;

		case AST_BLOCK:
		case AST_GENBLOCK:
			if (!child->str.empty())
				prefix_node(child);
			break;

		case AST_ENUM:
			current_scope[child->str] = child;
			for (auto& enode : child->children){
				if (!enode)
					continue;
				log_assert(enode->type == AST_ENUM_ITEM);
				prefix_node(enode.get());
			}
			break;

		case AST_IDENTIFIER:
			if (!child->str.empty() && prefix.size() > 0) {
				bool is_resolved = false;
				std::string identifier_str = child->str;
				// Already resolved to a real declaration in an earlier pass; see
				// the matching guard in AstNode::simplify().  Name prefixing must
				// not treat it as a new implicit net.
				if (child->id2ast && child->id2ast->type != AST_AUTOWIRE)
					is_resolved = true;
				if (current_ast_mod != nullptr && identifier_str.size() > current_ast_mod->str.size() && identifier_str.compare(0, current_ast_mod->str.size(), current_ast_mod->str) == 0) {
					if (identifier_str.at(current_ast_mod->str.size()) == '.') {
						identifier_str = '\\' + identifier_str.substr(current_ast_mod->str.size()+1, identifier_str.size());
					}
				}
				// search starting in the innermost scope and then stepping outward
				for (size_t ppos = prefix.size() - 1; ppos; --ppos) {
					if (prefix.at(ppos) != '.') continue;

					std::string new_prefix = prefix.substr(0, ppos + 1);
					auto attempt_resolve = [&new_prefix](const std::string &ident) -> std::string {
						std::string new_name = prefix_id(new_prefix, ident);
						if (current_scope.count(new_name))
							return new_name;
						return {};
					};

					// attempt to resolve the full identifier
					std::string resolved = attempt_resolve(identifier_str);
					if (!resolved.empty()) {
						is_resolved = true;
						break;
					}
					// attempt to resolve hierarchical prefixes within the identifier,
					// as the prefix could refer to a local scope which exists but
					// hasn't yet been elaborated
					for (size_t spos = identifier_str.size() - 1; spos; --spos) {
						if (identifier_str.at(spos) != '.') continue;
						resolved = attempt_resolve(identifier_str.substr(0, spos));
						if (!resolved.empty()) {
							is_resolved = true;
							identifier_str = resolved + identifier_str.substr(spos);
							ppos = 1; // break outer loop
							break;
						}
					}
					if (!prefix_is_function_inline(prefix) && current_scope.count(identifier_str) == 0) {
						AstNode *current_scope_ast = (current_ast_mod == nullptr) ? current_ast : current_ast_mod;
						for (auto& node : current_scope_ast->children) {
							if (!node)
								continue;
							switch (node->type) {
							case AST_PARAMETER:
							case AST_LOCALPARAM:
							case AST_WIRE:
							case AST_AUTOWIRE:
							case AST_GENVAR:
							case AST_MEMORY:
							case AST_FUNCTION:
							case AST_TASK:
							case AST_DPI_FUNCTION:
								if (prefix_id(new_prefix, identifier_str) == node->str) {
									is_resolved = true;
									current_scope[node->str] = node.get();
								}
								break;
							case AST_ENUM:
								current_scope[node->str] = node.get();
								for (auto& enum_node : node->children) {
									if (!enum_node)
										continue;
									log_assert(enum_node->type==AST_ENUM_ITEM);
									if (prefix_id(new_prefix, identifier_str) == enum_node->str) {
										is_resolved = true;
										current_scope[enum_node->str] = enum_node.get();
									}
								}
								break;
							default:
								break;
							}
						}
					}
				}
				if (!prefix_is_function_inline(prefix) && (current_scope.count(identifier_str) == 0) && is_resolved == false) {
					bool is_type = current_scope.count(identifier_str) && current_scope[identifier_str] &&
							current_scope[identifier_str]->type == AST_TYPEDEF;
					AstNode *scope_ast = current_ast_mod ? current_ast_mod : current_ast;
					if (!is_type && scope_ast) {
						for (auto &node : scope_ast->children) {
							if (node && node->type == AST_TYPEDEF && node->str == identifier_str) {
								is_type = true;
								break;
							}
						}
					}
					// IEEE 1800: a type name is not an implicit net, and an
					// implicit net is a module declaration, not an extra
					// expression argument.
					if (!is_type && current_ast_mod == nullptr) {
						input_error("Identifier `%s' is implicitly declared outside of a module.\n", child->str.c_str());
					} else if (!is_type && is_sva_decl_ref(identifier_str)) {
						// a declaration reference from inside a generate
						// scope: no scope-prefixed wire (sva2check resolves
						// the declaration by its bare name); simplify() gives
						// it the module-level wire
					} else if (!is_type) {
						if (!(flag_autowire || identifier_str == "\\$global_clock")) {
							if (!flag_permissive)
								input_error("Identifier `%s' is implicitly declared and `default_nettype is set to none.\n", identifier_str.c_str());
							degrade(child->location.to_loc(), "Identifier `%s' is implicitly declared under `default_nettype none; a 1-bit wire is implied.\n", identifier_str.c_str());
						}
						std::string wire_name = prefix_id(prefix, identifier_str);
						// IEEE 1800-2017 23.8: a dotted name whose first
						// component is an instance of an enclosing scope (not
						// of this one) is a hierarchical reference resolved
						// upward, not a net of this scope; name it in the
						// scope holding the instance (`u_core.sig` in a
						// generate loop is the module's `u_core.sig`, which
						// flatten joins to the instance's wire)
						size_t head_end = identifier_str.find('.');
						if (head_end != std::string::npos) {
							std::string head = identifier_str.substr(0, head_end);
							auto is_cell = [](const std::string &name) {
								auto it = current_scope.find(name);
								return it != current_scope.end() && it->second && it->second->type == AST_CELL;
							};
							if (!is_cell(prefix_id(prefix, head))) {
								for (size_t ppos = prefix.size() - 1; ppos != std::string::npos; ) {
									size_t dot = ppos ? prefix.rfind('.', ppos - 1) : std::string::npos;
									std::string outer = dot == std::string::npos ? std::string() : prefix.substr(0, dot + 1);
									if (is_cell(outer.empty() ? head : prefix_id(outer, head))) {
										wire_name = outer.empty() ? identifier_str : prefix_id(outer, identifier_str);
										break;
									}
									ppos = dot;
								}
							}
						}
						if (current_scope.count(wire_name) == 0) {
							auto auto_wire = std::make_unique<AstNode>(child->location, AST_AUTOWIRE);
							auto_wire->str = wire_name;
							current_scope[wire_name] = auto_wire.get();
							current_ast_mod->children.push_back(std::move(auto_wire));
						}
					}
				}
			}
			break;
		default:
			break;
		}
	}

	for (size_t i = 0; i < children.size(); i++) {
		auto& child = children[i];
		// AST_PREFIX member names should not be prefixed; we recurse into them
		// as normal to ensure indices and ranges are properly resolved, and
		// then restore the previous string
		if (type == AST_PREFIX && i == 1) {
			std::string backup_scope_name = child->str;
			child->expand_genblock(prefix);
			child->str = backup_scope_name;
			continue;
		}
		// functions/tasks may reference wires, constants, etc. in this scope
		if (child->type == AST_FUNCTION || child->type == AST_TASK)
			continue;
		// named blocks pick up the current prefix and will expanded later
		if ((child->type == AST_GENBLOCK || child->type == AST_BLOCK) && !child->str.empty())
			continue;

		child->expand_genblock(prefix);
	}
}

// add implicit AST_GENBLOCK names according to IEEE 1364-2005 Section 12.4.3 or
// IEEE 1800-2017 Section 27.6
static bool is_directly_nested(AstNode *node)
{
	return node->type == AST_GENBLOCK && node->str.empty() && node->get_bool_attribute(ID(sv_gen_wrapper)) &&
			node->children.size() == 1 && (node->children[0]->type == AST_GENIF || node->children[0]->type == AST_GENCASE);
}

// IEEE 1800-2017 27.5: a generate block of a conditional construct that holds
// nothing but another conditional construct, without begin/end, is not a scope
// of its own; the inner construct's blocks belong to the outer one (so the
// blocks of `if .. else if .. else` are siblings and share its implicit name)
void AstNode::label_conditional_branches(std::set<std::string>& existing, int &counter)
{
	auto label_branch = [&](AstNode *branch) {
		if (is_directly_nested(branch))
			branch->children[0]->label_conditional_branches(existing, counter);
		else
			branch->label_genblks(existing, counter);
	};
	for (auto& child : children) {
		if (child->type == AST_COND || child->type == AST_CONDX || child->type == AST_CONDZ)
			for (auto& item : child->children)
				label_branch(item.get());
		else
			label_branch(child.get());
	}
}

void AstNode::label_genblks(std::set<std::string>& existing, int &counter)
{
	switch (type) {
	case AST_GENIF:
	case AST_GENCASE:
		// seeing a proper generate control flow construct increments the
		// counter once
		++counter;
		label_conditional_branches(existing, counter);
		break;

	case AST_GENFOR:
		++counter;
		for (auto& child : children)
			child->label_genblks(existing, counter);
		break;

	case AST_GENBLOCK: {
		// if this block is unlabeled, generate its corresponding unique name
		for (int padding = 0; str.empty(); ++padding) {
			std::string candidate = "\\genblk";
			for (int i = 0; i < padding; ++i)
				candidate += '0';
			candidate += std::to_string(counter);
			if (!existing.count(candidate))
				str = candidate;
		}
		// within a genblk, the counter starts fresh
		std::set<std::string> existing_local = existing;
		int counter_local = 0;
		for (auto& child : children)
			child->label_genblks(existing_local, counter_local);
		break;
	}

	default:
		// track names which could conflict with implicit genblk names
		if (str.rfind("\\genblk", 0) == 0)
			existing.insert(str);
		for (auto& child : children)
			child->label_genblks(existing, counter);
		break;
	}
}

// helper function for mem2reg_as_needed_pass1
static void mark_memories_assign_lhs_complex(dict<AstNode*, pool<std::string>> &mem2reg_places,
		dict<AstNode*, uint32_t> &mem2reg_candidates, AstNode *that)
{
	for (auto &child : that->children)
		mark_memories_assign_lhs_complex(mem2reg_places, mem2reg_candidates, child.get());

	if (that->type == AST_IDENTIFIER && that->id2ast && that->id2ast->type == AST_MEMORY) {
		AstNode *mem = that->id2ast;
		if (!(mem2reg_candidates[mem] & AstNode::MEM2REG_FL_CMPLX_LHS))
			mem2reg_places[mem].insert(stringf("%s:%d", RTLIL::encode_filename(*that->location.begin.filename), that->location.begin.line));
		mem2reg_candidates[mem] |= AstNode::MEM2REG_FL_CMPLX_LHS;
	}
}

// find memories that should be replaced by registers
void AstNode::mem2reg_as_needed_pass1(dict<AstNode*, pool<std::string>> &mem2reg_places,
		dict<AstNode*, uint32_t> &mem2reg_candidates, dict<AstNode*, uint32_t> &proc_flags, uint32_t &flags)
{
	uint32_t children_flags = 0;
	int lhs_children_counter = 0;

	if (type == AST_TYPEDEF)
		return; // don't touch content of typedefs

	if (type == AST_ASSIGN || type == AST_ASSIGN_LE || type == AST_ASSIGN_EQ)
	{
		// mark all memories that are used in a complex expression on the left side of an assignment
		for (auto &lhs_child : children[0]->children)
			mark_memories_assign_lhs_complex(mem2reg_places, mem2reg_candidates, lhs_child.get());

		if (children[0]->type == AST_IDENTIFIER && children[0]->id2ast && children[0]->id2ast->type == AST_MEMORY)
		{
			AstNode *mem = children[0]->id2ast;

			// activate mem2reg if this is assigned in an async proc
			if (flags & AstNode::MEM2REG_FL_ASYNC) {
				if (!(mem2reg_candidates[mem] & AstNode::MEM2REG_FL_SET_ASYNC))
					mem2reg_places[mem].insert(stringf("%s:%d", RTLIL::encode_filename(*location.begin.filename), location.begin.line));
				mem2reg_candidates[mem] |= AstNode::MEM2REG_FL_SET_ASYNC;
			}

			// remember if this is assigned blocking (=)
			if (type == AST_ASSIGN_EQ) {
				if (!(proc_flags[mem] & AstNode::MEM2REG_FL_EQ1))
					mem2reg_places[mem].insert(stringf("%s:%d", RTLIL::encode_filename(*location.begin.filename), location.begin.line));
				proc_flags[mem] |= AstNode::MEM2REG_FL_EQ1;
			}

			// for proper (non-init) writes: remember if this is a constant index or not
			if ((flags & MEM2REG_FL_INIT) == 0) {
				if (children[0]->children.size() && children[0]->children[0]->type == AST_RANGE && children[0]->children[0]->children.size()) {
					if (children[0]->children[0]->children[0]->type == AST_CONSTANT)
						mem2reg_candidates[mem] |= AstNode::MEM2REG_FL_CONST_LHS;
					else
						mem2reg_candidates[mem] |= AstNode::MEM2REG_FL_VAR_LHS;
				}
			}

			// remember where this is
			if (flags & MEM2REG_FL_INIT) {
				if (!(mem2reg_candidates[mem] & AstNode::MEM2REG_FL_SET_INIT))
					mem2reg_places[mem].insert(stringf("%s:%d", RTLIL::encode_filename(*location.begin.filename), location.begin.line));
				mem2reg_candidates[mem] |= AstNode::MEM2REG_FL_SET_INIT;
			} else {
				if (!(mem2reg_candidates[mem] & AstNode::MEM2REG_FL_SET_ELSE))
					mem2reg_places[mem].insert(stringf("%s:%d", RTLIL::encode_filename(*location.begin.filename), location.begin.line));
				mem2reg_candidates[mem] |= AstNode::MEM2REG_FL_SET_ELSE;
			}
		}

		lhs_children_counter = 1;
	}

	if (type == AST_IDENTIFIER && id2ast && id2ast->type == AST_MEMORY)
	{
		AstNode *mem = id2ast;

		if (integer < (unsigned)mem->unpacked_dimensions) {
			if (!flag_permissive)
				input_error("Insufficient number of array indices for %s.\n", RTLIL::unescape_id(str));
			return;
		}

		// flag if used after blocking assignment (in same proc)
		if ((proc_flags[mem] & AstNode::MEM2REG_FL_EQ1) && !(mem2reg_candidates[mem] & AstNode::MEM2REG_FL_EQ2)) {
			mem2reg_places[mem].insert(stringf("%s:%d", RTLIL::encode_filename(*location.begin.filename), location.begin.line));
			mem2reg_candidates[mem] |= AstNode::MEM2REG_FL_EQ2;
		}
	}

	// also activate if requested, either by using mem2reg attribute or by declaring array as 'wire' instead of 'reg' or 'logic'
	if (type == AST_MEMORY && (get_bool_attribute(ID::mem2reg) || (flags & AstNode::MEM2REG_FL_ALL) || !(is_reg || is_logic)))
		mem2reg_candidates[this] |= AstNode::MEM2REG_FL_FORCED;

	if ((type == AST_MODULE || type == AST_INTERFACE) && get_bool_attribute(ID::mem2reg))
		children_flags |= AstNode::MEM2REG_FL_ALL;

	dict<AstNode*, uint32_t> *proc_flags_p = nullptr;

	if (type == AST_ALWAYS) {
		int count_edge_events = 0;
		for (auto& child : children)
			if (child->type == AST_POSEDGE || child->type == AST_NEGEDGE)
				count_edge_events++;
		if (count_edge_events != 1)
			children_flags |= AstNode::MEM2REG_FL_ASYNC;
		proc_flags_p = new dict<AstNode*, uint32_t>;
	}
	else if (type == AST_INITIAL) {
		children_flags |= AstNode::MEM2REG_FL_INIT;
		proc_flags_p = new dict<AstNode*, uint32_t>;
	}

	uint32_t backup_flags = flags;
	flags |= children_flags;
	log_assert((flags & ~0x000000ff) == 0);

	for (auto& child : children)
	{
		if (lhs_children_counter > 0) {
			lhs_children_counter--;
			if (child->children.size() && child->children[0]->type == AST_RANGE && child->children[0]->children.size()) {
				for (auto& c : child->children[0]->children) {
					if (proc_flags_p)
						c->mem2reg_as_needed_pass1(mem2reg_places, mem2reg_candidates, *proc_flags_p, flags);
					else
						c->mem2reg_as_needed_pass1(mem2reg_places, mem2reg_candidates, proc_flags, flags);
				}
			}
		} else
		if (proc_flags_p)
			child->mem2reg_as_needed_pass1(mem2reg_places, mem2reg_candidates, *proc_flags_p, flags);
		else
			child->mem2reg_as_needed_pass1(mem2reg_places, mem2reg_candidates, proc_flags, flags);
	}

	flags &= ~children_flags | backup_flags;

	if (proc_flags_p) {
#ifndef NDEBUG
		for (auto it : *proc_flags_p)
			log_assert((it.second & ~0xff000000) == 0);
#endif
		delete proc_flags_p;
	}
}

bool AstNode::mem2reg_check(pool<AstNode*> &mem2reg_set)
{
	if (type != AST_IDENTIFIER || !id2ast || !mem2reg_set.count(id2ast))
		return false;

	if (children.empty() || !children[0] || children[0]->type != AST_RANGE || GetSize(children[0]->children) != 1) {
		// Whole-array or struct access cannot be flattened. Keep the memory.
		mem2reg_set.erase(id2ast);
		return false;
	}

	return true;
}

void AstNode::mem2reg_remove(pool<AstNode*> &mem2reg_set)
{
	log_assert(mem2reg_set.count(this) == 0);

	if (mem2reg_set.count(id2ast))
		id2ast = nullptr;

	for (size_t i = 0; i < children.size(); i++) {
		if (mem2reg_set.count(children[i].get()) > 0) {
			children.erase(children.begin() + (i--));
		} else {
			children[i]->mem2reg_remove(mem2reg_set);
		}
	}
}

// actually replace memories with registers
bool AstNode::mem2reg_as_needed_pass2(pool<AstNode*> &mem2reg_set, AstNode *mod, AstNode *block, AstNode* async_block)
{
	bool did_something = false;

	if (type == AST_BLOCK)
		block = this;

	if (type == AST_FUNCTION || type == AST_TASK)
		return false;

	if (type == AST_TYPEDEF)
		return false;

	if (type == AST_MEMINIT && id2ast && mem2reg_set.count(id2ast))
	{
		log_assert(children[0]->type == AST_CONSTANT);
		log_assert(children[1]->type == AST_CONSTANT);
		log_assert(children[2]->type == AST_CONSTANT);
		log_assert(children[3]->type == AST_CONSTANT);

		int cursor = children[0]->asInt(false);
		Const data = children[1]->bitsAsConst();
		Const en = children[2]->bitsAsConst();
		int length = children[3]->asInt(false);

		if (length != 0)
		{
			auto block_owned = std::make_unique<AstNode>(location,
				AST_INITIAL, std::make_unique<AstNode>(location,
					AST_BLOCK));
			auto block = block_owned.get();
			mod->children.push_back(std::move(block_owned));
			block = block->children[0].get();

			int wordsz = GetSize(data) / length;

			for (int i = 0; i < length; i++) {
				int pos = 0;
				while (pos < wordsz) {
					if (en[pos] != State::S1) {
						pos++;
					} else {
						int epos = pos + 1;
						while (epos < wordsz && en[epos] == State::S1)
							epos++;
						int clen = epos - pos;
						auto range = std::make_unique<AstNode>(location,
							AST_RANGE, AstNode::mkconst_int(location,
								cursor+i, false));
						if (pos != 0 || epos != wordsz) {
							int left;
							int right;
							auto& mrange = id2ast->children[0];
							if (mrange->range_left < mrange->range_right) {
								right = mrange->range_right - pos;
								left = mrange->range_right - epos + 1;
							} else {
								right = mrange->range_right + pos;
								left = mrange->range_right + epos - 1;
							}
							range = std::make_unique<AstNode>(location,
								AST_MULTIRANGE, std::move(range), std::make_unique<AstNode>(location,
									AST_RANGE,
									AstNode::mkconst_int(location, left, true),
									AstNode::mkconst_int(location, right, true)));
						}
						auto target = std::make_unique<AstNode>(location, AST_IDENTIFIER, std::move(range));
						target->str = str;
						target->id2ast = id2ast;
						target->was_checked = true;
						block->children.push_back(std::make_unique<AstNode>(location,
							AST_ASSIGN_EQ,
							std::move(target),
							mkconst_bits(location,
								data.extract(i*wordsz + pos, clen).to_bits(),
								false)));
						pos = epos;
					}
				}
			}
		}

		auto newNode = std::make_unique<AstNode>(location, AST_NONE);
		newNode->cloneInto(*this);
		did_something = true;
	}

	if (type == AST_ASSIGN && block == nullptr && children[0]->mem2reg_check(mem2reg_set))
	{
		if (async_block == nullptr) {
			auto async_block_owned = std::make_unique<AstNode>(location, AST_ALWAYS, std::make_unique<AstNode>(location, AST_BLOCK));
			async_block = async_block_owned.get();
			mod->children.push_back(std::move(async_block_owned));
		}

		auto newNode = clone();
		newNode->type = AST_ASSIGN_EQ;
		newNode->children[0]->was_checked = true;
		async_block->children[0]->children.push_back(std::move(newNode));

		newNode = std::make_unique<AstNode>(location, AST_NONE);
		newNode->cloneInto(*this);
		did_something = true;
	}

	if ((type == AST_ASSIGN_LE || type == AST_ASSIGN_EQ) && children[0]->mem2reg_check(mem2reg_set) &&
			children[0]->children[0]->children[0]->type != AST_CONSTANT)
	{
		std::stringstream sstr;
		sstr << "$mem2reg_wr$" << children[0]->str << "$" << RTLIL::encode_filename(*location.begin.filename) << ":" << location.begin.line << "$" << (autoidx++);
		std::string id_addr = sstr.str() + "_ADDR", id_data = sstr.str() + "_DATA";

		int mem_width, mem_size, addr_bits;
		bool mem_signed = children[0]->id2ast->is_signed;
		children[0]->id2ast->meminfo(mem_width, mem_size, addr_bits);

		auto wire_addr = std::make_unique<AstNode>(location, AST_WIRE, std::make_unique<AstNode>(location, AST_RANGE, mkconst_int(location, addr_bits-1, true), mkconst_int(location, 0, true)));
		wire_addr->str = id_addr;
		wire_addr->is_reg = true;
		wire_addr->was_checked = true;
		wire_addr->set_attribute(ID::nosync, AstNode::mkconst_int(location, 1, false));
		while (wire_addr->simplify(true, 1, -1, false)) { }
		mod->children.push_back(std::move(wire_addr));

		auto wire_data = std::make_unique<AstNode>(location, AST_WIRE, std::make_unique<AstNode>(location, AST_RANGE, mkconst_int(location, mem_width-1, true), mkconst_int(location, 0, true)));
		wire_data->str = id_data;
		wire_data->is_reg = true;
		wire_data->was_checked = true;
		wire_data->is_signed = mem_signed;
		wire_data->set_attribute(ID::nosync, AstNode::mkconst_int(location, 1, false));
		while (wire_data->simplify(true, 1, -1, false)) { }
		mod->children.push_back(std::move(wire_data));

		log_assert(block != nullptr);
		size_t assign_idx = 0;
		while (assign_idx < block->children.size() && block->children[assign_idx].get() != this)
			assign_idx++;
		log_assert(assign_idx < block->children.size());

		auto assign_addr = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), children[0]->children[0]->children[0]->clone());
		assign_addr->children[0]->str = id_addr;
		assign_addr->children[0]->was_checked = true;
		block->children.insert(block->children.begin()+assign_idx+1, std::move(assign_addr));

		auto case_node = std::make_unique<AstNode>(location, AST_CASE, std::make_unique<AstNode>(location, AST_IDENTIFIER));
		case_node->children[0]->str = id_addr;
		for (int i = 0; i < mem_size; i++) {
			if (children[0]->children[0]->children[0]->type == AST_CONSTANT && int(children[0]->children[0]->children[0]->integer) != i)
				continue;
			auto cond_node = std::make_unique<AstNode>(location, AST_COND, AstNode::mkconst_int(location, i, false, addr_bits), std::make_unique<AstNode>(location, AST_BLOCK));
			auto assign_reg = std::make_unique<AstNode>(location, type, std::make_unique<AstNode>(location, AST_IDENTIFIER), std::make_unique<AstNode>(location, AST_IDENTIFIER));
			if (children[0]->children.size() == 2)
				assign_reg->children[0]->children.push_back(children[0]->children[1]->clone());
			assign_reg->children[0]->str = stringf("%s[%d]", children[0]->str, i);
			assign_reg->children[1]->str = id_data;
			cond_node->children[1]->children.push_back(std::move(assign_reg));
			case_node->children.push_back(std::move(cond_node));
		}

		// fixup on the full hierarchy below case_node
		case_node->fixup_hierarchy_flags(true);

		block->children.insert(block->children.begin()+assign_idx+2, std::move(case_node));

		children[0]->delete_children();
		children[0]->range_valid = false;
		children[0]->id2ast = nullptr;
		children[0]->str = id_data;
		type = AST_ASSIGN_EQ;
		children[0]->was_checked = true;

		fixup_hierarchy_flags();
		did_something = true;
	}

	if (mem2reg_check(mem2reg_set))
	{
		std::unique_ptr<AstNode> bit_part_sel = nullptr;
		if (children.size() == 2)
			bit_part_sel = children[1]->clone();

		if (children[0]->children[0]->type == AST_CONSTANT)
		{
			int id = children[0]->children[0]->integer;
			int left = id2ast->children[1]->children[0]->integer;
			int right = id2ast->children[1]->children[1]->integer;
			bool valid_const_access =
				(left <= id && id <= right) ||
				(right <= id && id <= left);
			if (valid_const_access)
			{
				str = stringf("%s[%d]", str, id);
				delete_children();
				range_valid = false;
				id2ast = nullptr;
			}
			else
			{
				int width;
				if (bit_part_sel)
				{
					// bit_part_sel->dumpAst(nullptr, "? ");
					if (bit_part_sel->children.size() == 1)
						width = 0;
					else
						width = bit_part_sel->children[0]->integer -
							bit_part_sel->children[1]->integer;
					bit_part_sel = nullptr;
				}
				else
				{
					width = id2ast->children[0]->children[0]->integer -
						id2ast->children[0]->children[1]->integer;
				}
				width = abs(width) + 1;

				delete_children();

				std::vector<RTLIL::State> x_bits;
				for (int i = 0; i < width; i++)
					x_bits.push_back(RTLIL::State::Sx);
				auto constant = AstNode::mkconst_bits(location, x_bits, false);
				constant->cloneInto(*this);
			}
		}
		else
		{
			std::stringstream sstr;
			sstr << "$mem2reg_rd$" << str << "$" << RTLIL::encode_filename(*location.begin.filename) << ":" << location.begin.line << "$" << (autoidx++);
			std::string id_addr = sstr.str() + "_ADDR", id_data = sstr.str() + "_DATA";

			int mem_width, mem_size, addr_bits;
			bool mem_signed = id2ast->is_signed;
			id2ast->meminfo(mem_width, mem_size, addr_bits);

			auto wire_addr = std::make_unique<AstNode>(location, AST_WIRE, std::make_unique<AstNode>(location, AST_RANGE, mkconst_int(location, addr_bits-1, true), mkconst_int(location, 0, true)));
			wire_addr->str = id_addr;
			wire_addr->is_reg = true;
			wire_addr->was_checked = true;
			if (block)
				wire_addr->set_attribute(ID::nosync, AstNode::mkconst_int(location, 1, false));
			while (wire_addr->simplify(true, 1, -1, false)) { }
			mod->children.push_back(std::move(wire_addr));

			auto wire_data = std::make_unique<AstNode>(location, AST_WIRE, std::make_unique<AstNode>(location, AST_RANGE, mkconst_int(location, mem_width-1, true), mkconst_int(location, 0, true)));
			wire_data->str = id_data;
			wire_data->is_reg = true;
			wire_data->was_checked = true;
			wire_data->is_signed = mem_signed;
			if (block)
				wire_data->set_attribute(ID::nosync, AstNode::mkconst_int(location, 1, false));
			while (wire_data->simplify(true, 1, -1, false)) { }
			mod->children.push_back(std::move(wire_data));

			auto assign_addr = std::make_unique<AstNode>(location, block ? AST_ASSIGN_EQ : AST_ASSIGN, std::make_unique<AstNode>(location, AST_IDENTIFIER), children[0]->children[0]->clone());
			assign_addr->children[0]->str = id_addr;
			assign_addr->children[0]->was_checked = true;

			auto case_node = std::make_unique<AstNode>(location, AST_CASE, std::make_unique<AstNode>(location, AST_IDENTIFIER));
			case_node->children[0]->str = id_addr;

			for (int i = 0; i < mem_size; i++) {
				if (children[0]->children[0]->type == AST_CONSTANT && int(children[0]->children[0]->integer) != i)
					continue;
				auto cond_node = std::make_unique<AstNode>(location, AST_COND, AstNode::mkconst_int(location, i, false, addr_bits), std::make_unique<AstNode>(location, AST_BLOCK));
				auto assign_reg = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), std::make_unique<AstNode>(location, AST_IDENTIFIER));
				assign_reg->children[0]->str = id_data;
				assign_reg->children[0]->was_checked = true;
				assign_reg->children[1]->str = stringf("%s[%d]", str, i);
				cond_node->children[1]->children.push_back(std::move(assign_reg));
				case_node->children.push_back(std::move(cond_node));
			}

			std::vector<RTLIL::State> x_bits;
			for (int i = 0; i < mem_width; i++)
				x_bits.push_back(RTLIL::State::Sx);

			auto cond_node = std::make_unique<AstNode>(location, AST_COND, std::make_unique<AstNode>(location, AST_DEFAULT), std::make_unique<AstNode>(location, AST_BLOCK));
			auto assign_reg = std::make_unique<AstNode>(location, AST_ASSIGN_EQ, std::make_unique<AstNode>(location, AST_IDENTIFIER), AstNode::mkconst_bits(location, x_bits, false));
			assign_reg->children[0]->str = id_data;
			assign_reg->children[0]->was_checked = true;
			cond_node->children[1]->children.push_back(std::move(assign_reg));
			case_node->children.push_back(std::move(cond_node));

			// fixup on the full hierarchy below case_node
			case_node->fixup_hierarchy_flags(true);

			if (block)
			{
				size_t assign_idx = 0;
				while (assign_idx < block->children.size() && !block->children[assign_idx]->contains(this))
					assign_idx++;
				log_assert(assign_idx < block->children.size());
				block->children.insert(block->children.begin()+assign_idx, std::move(case_node));
				block->children.insert(block->children.begin()+assign_idx, std::move(assign_addr));
			}
			else
			{
				auto proc = std::make_unique<AstNode>(location, AST_ALWAYS, std::make_unique<AstNode>(location, AST_BLOCK, std::move(case_node)));
				mod->children.push_back(std::move(proc));
				mod->children.push_back(std::move(assign_addr));
				mod->fixup_hierarchy_flags();
			}

			delete_children();
			range_valid = false;
			id2ast = nullptr;
			str = id_data;
		}

		if (bit_part_sel) {
			children.push_back(std::move(bit_part_sel));
			fixup_hierarchy_flags();
		}

		did_something = true;
	}

	log_assert(id2ast == nullptr || mem2reg_set.count(id2ast) == 0);

	std::vector<AstNode*> children_list;
	for (auto& child : children)
		children_list.push_back(child.get());

	for (size_t i = 0; i < children_list.size(); i++)
		if (children_list[i]->mem2reg_as_needed_pass2(mem2reg_set, mod, block, async_block))
			did_something = true;

	return did_something;
}

// calculate memory dimensions
void AstNode::meminfo(int &mem_width, int &mem_size, int &addr_bits)
{
	log_assert(type == AST_MEMORY);

	auto ok = [](const std::unique_ptr<AstNode> &child) {
		return child && child->type == AST_RANGE && child->range_valid;
	};
	if (children.empty() || !ok(children[0]))
		children.insert(children.begin(), make_range(location, 0, 0));
	if (children.size() < 2 || !ok(children[1]))
		children.insert(children.begin() + 1, make_range(location, 0, 0));

	mem_width = children[0]->range_left - children[0]->range_right + 1;
	mem_size = children[1]->range_left - children[1]->range_right;

	if (mem_size < 0)
		mem_size *= -1;
	mem_size += min(children[1]->range_left, children[1]->range_right) + 1;

	addr_bits = 1;
	while ((1 << addr_bits) < mem_size)
		addr_bits++;
}

bool AstNode::detect_latch(const std::string &var)
{
	switch (type)
	{
	case AST_ALWAYS:
		for (auto &c : children)
		{
			switch (c->type)
			{
			case AST_POSEDGE:
			case AST_NEGEDGE:
				return false;
			case AST_EDGE:
				break;
			case AST_BLOCK:
				if (!c->detect_latch(var))
					return false;
				break;
			default:
				log_abort();
			}
		}
		return true;
	case AST_BLOCK:
		for (auto &c : children)
			if (!c->detect_latch(var))
				return false;
		return true;
	case AST_CASE:
		{
			bool r = true;
			for (auto &c : children) {
				if (c->type == AST_COND) {
					if (c->children.at(1)->detect_latch(var))
						return true;
					r = false;
				}
				if (c->type == AST_DEFAULT) {
					if (c->children.at(0)->detect_latch(var))
						return true;
					r = false;
				}
			}
			return r;
		}
	case AST_ASSIGN_EQ:
	case AST_ASSIGN_LE:
		if (children.at(0)->type == AST_IDENTIFIER &&
				children.at(0)->children.empty() && children.at(0)->str == var)
			return false;
		return true;
	default:
		return true;
	}
}

bool AstNode::has_const_only_constructs()
{
	if (type == AST_WHILE || type == AST_REPEAT || type == AST_FOR)
		return true;
	for (auto& child : children)
		if (child->has_const_only_constructs())
			return true;
	return false;
}

bool AstNode::is_simple_const_expr()
{
	if (type == AST_IDENTIFIER)
		return false;
	for (auto& child : children)
		if (!child->is_simple_const_expr())
			return false;
	return true;
}

// Replace a node by a constant, keeping the assignment-pattern keys
// (`'{XLEN: x}`: the member name rides on the value node as sv_member /
// sv_typekey; substituting a function argument for `x` must not drop it)
static void replace_keep_pattern_keys(AstNode *node, std::unique_ptr<AstNode> newNode)
{
	for (auto id : {ID(sv_member), ID(sv_typekey)})
		if (node->attributes.count(id))
			newNode->set_attribute(id, node->attributes.at(id)->clone());
	newNode->cloneInto(*node);
}

// helper function for AstNode::eval_const_function()
bool AstNode::replace_variables(std::map<std::string, AstNode::varinfo_t> &variables, AstNode *fcall, bool must_succeed)
{
	// a member of a struct-typed variable (`cfg.XLEN`, `u.a.b`): the member
	// was registered in the function's scope when the variable's type was
	// resolved; read its bits out of the base variable (register v3 D13:
	// config-building constant functions take and return structs)
	if (type == AST_IDENTIFIER && !variables.count(str) && str.find('.') != std::string::npos && current_scope.count(str)) {
		AstNode *item = current_scope.at(str);
		// the base is the longest dotted prefix that is a function variable
		// (generated names embed the source file name, which has dots)
		std::string base;
		for (size_t pos = str.rfind('.'); pos != std::string::npos && pos > 0; pos = str.rfind('.', pos - 1))
			if (variables.count(str.substr(0, pos))) {
				base = str.substr(0, pos);
				break;
			}
		if ((item->type == AST_STRUCT_ITEM || item->type == AST_STRUCT || item->type == AST_UNION) &&
				item->range_valid && !base.empty()) {
			int offset = item->range_right, width = item->range_left - item->range_right + 1;
			if (!children.empty()) {
				if (children.size() != 1 || children.at(0)->type != AST_RANGE) {
					if (!must_succeed)
						return false;
					input_error("Unsupported select on struct member in constant function\n%s: ...called from here.\n",
							fcall->loc_string().c_str());
				}
				if (!children.at(0)->replace_variables(variables, fcall, must_succeed))
					return false;
				while (children.at(0)->simplify(true, 1, -1, false)) { }
				if (!children.at(0)->range_valid || item->dimensions.size() != 1 || item->dimensions[0].range_swapped) {
					if (!must_succeed)
						return false;
					input_error("Non-constant range\n%s: ... called from here.\n", fcall->loc_string().c_str());
				}
				int lo = min(children.at(0)->range_left, children.at(0)->range_right);
				width = std::abs(children.at(0)->range_left - children.at(0)->range_right) + 1;
				offset += lo - item->dimensions[0].range_right;
			}
			const varinfo_t &v = variables.at(base);
			offset -= v.offset;
			std::vector<RTLIL::State> new_bits;
			for (int i = 0; i < width; i++)
				new_bits.push_back(offset + i >= 0 && offset + i < v.val.size() ? v.val[offset + i] : RTLIL::State::Sx);
			auto newNode = mkconst_bits(location, new_bits, item->is_signed);
			replace_keep_pattern_keys(this, std::move(newNode));
			return true;
		}
	}
	if (type == AST_IDENTIFIER && variables.count(str)) {
		int offset = variables.at(str).offset, width = variables.at(str).val.size();
		// `v[i][j]` on a packed multi-dimensional variable (`logic
		// [1:0][3:0] v;`, `T [1:0][1:0] v;`): the simplify below flattens
		// the select to one range, as it does for `v[i]` and for lvalues
		// (was "Memory access in constant function is not supported")
		AstNode *mdecl = current_scope.count(str) ? current_scope.at(str) : nullptr;
		bool packed_mdim = !children.empty() && children.at(0)->type == AST_MULTIRANGE && mdecl &&
				mdecl->type == AST_WIRE && mdecl->unpacked_dimensions == 0 && GetSize(mdecl->dimensions) > 1;
		if (!children.empty()) {
			if (children.size() != 1 || (children.at(0)->type != AST_RANGE && !packed_mdim)) {
				if (!must_succeed)
					return false;
				input_error("Memory access in constant function is not supported\n%s: ...called from here.\n",
						fcall->loc_string().c_str());
			}
			if (!children.at(0)->replace_variables(variables, fcall, must_succeed))
				return false;
			while (simplify(true, 1, -1, false)) { }
			if (children.size() != 1 || children.at(0)->type != AST_RANGE || !children.at(0)->range_valid) {
				if (!must_succeed)
					return false;
				input_error("Non-constant range\n%s: ... called from here.\n",
						fcall->loc_string().c_str());
			}
			offset = min(children.at(0)->range_left, children.at(0)->range_right);
			width = min(std::abs(children.at(0)->range_left - children.at(0)->range_right) + 1, width);
		}
		offset -= variables.at(str).offset;
		if (variables.at(str).range_swapped)
			offset = -offset;
		const RTLIL::Const &val = variables.at(str).val;
		std::vector<RTLIL::State> new_bits;
		new_bits.reserve(width);
		for (int i = 0; i < width; i++)
			new_bits.push_back(val[offset+i]);
		auto newNode = mkconst_bits(location, new_bits, variables.at(str).is_signed);
		replace_keep_pattern_keys(this, std::move(newNode));
		return true;
	}

	for (auto &child : children)
		if (!child->replace_variables(variables, fcall, must_succeed))
			return false;
	return true;
}

// attempt to statically evaluate a functions with all-const arguments
std::unique_ptr<AstNode> AstNode::eval_const_function(AstNode *fcall, bool must_succeed)
{
	// A body that is not a constant expression returns no value. The caller
	// substitutes constant 0 so elaboration can continue.
	if (!getenv("SVA_DEBUG_FCALL"))   // (debug: keep the caller's must_succeed to see why an evaluation fails)
		must_succeed = false;
	std::map<std::string, AstNode*> backup_scope = current_scope;
	std::map<std::string, AstNode::varinfo_t> variables;
	auto block = std::make_unique<AstNode>(location, AST_BLOCK);
	std::unique_ptr<AstNode> result = nullptr;

	size_t argidx = 0;
	for (auto& child : children)
	{
		block->children.push_back(child->clone());
	}
	block->set_in_param_flag(true);
	std::vector<std::unique_ptr<AstNode>> temporary_nodes;
	int const_while_steps = 0;

	while (!block->children.empty())
	{
		auto& stmt = block->children.front();

#if 0
		log("-----------------------------------\n");
		for (auto &it : variables)
			log("%20s %40s\n", it.first, log_signal(it.second.val));
		stmt->dumpAst(nullptr, "stmt> ");
#endif
		if (stmt->type == AST_WIRE)
		{
			while (stmt->simplify(true, 1, -1, false)) { }
			if (!stmt->range_valid) {
				if (!must_succeed)
					goto finished;
				stmt->input_error("Can't determine size of variable %s\n%s: ... called from here.\n",
						stmt->str.c_str(), fcall->loc_string().c_str());
			}
			AstNode::varinfo_t &variable = variables[stmt->str];
			int width = abs(stmt->range_left - stmt->range_right) + 1;
			// if this variable has already been declared as an input, check the
			// sizes match if it already had an explicit size
			if (variable.arg && variable.explicitly_sized && variable.val.size() != width) {
				input_error("Incompatible re-declaration of constant function wire %s.\n", stmt->str);
			}
			variable.val = RTLIL::Const(RTLIL::State::Sx, width);
			variable.offset = stmt->range_swapped ? stmt->range_left : stmt->range_right;
			variable.range_swapped = stmt->range_swapped;
			variable.is_signed = stmt->is_signed;
			variable.explicitly_sized = stmt->children.size() &&
				stmt->children.back()->type == AST_RANGE;
			// identify the argument corresponding to this wire, if applicable
			if (stmt->is_input && argidx < fcall->children.size()) {
				variable.arg = fcall->children.at(argidx++).get();
			} else if (stmt->is_input && stmt->attributes.count(RTLIL::IdString("\\sv_default"))) {
				auto fallback = stmt->attributes.at(RTLIL::IdString("\\sv_default"))->clone();
				while (fallback->simplify(true, 1, -1, false)) { }
				temporary_nodes.push_back(std::move(fallback));
				variable.arg = temporary_nodes.back().get();
			}
			// load the constant arg's value into this variable
			if (variable.arg) {
				if (variable.arg->type == AST_CONSTANT) {
					variable.val = variable.arg->bitsAsConst(width);
				} else {
					log_assert(variable.arg->type == AST_REALVALUE);
					variable.val = variable.arg->realAsConst(width);
				}
			}
			current_scope[stmt->str] = stmt.get();
			temporary_nodes.push_back(std::move(stmt));

			block->children.erase(block->children.begin());
			continue;
		}

		if (stmt->type == AST_TCALL && stmt->str == "\\return") {
			block->children.clear();
			continue;
		}

		// a type local to the function: in scope for the declarations after it
		if (stmt->type == AST_TYPEDEF || stmt->type == AST_ENUM)
		{
			while (stmt->simplify(true, 1, -1, false)) { }
			current_scope[stmt->str] = stmt.get();
			if (stmt->type == AST_ENUM)
				for (auto &item : stmt->children)
					current_scope[item->str] = item.get();
			temporary_nodes.push_back(std::move(stmt));

			block->children.erase(block->children.begin());
			continue;
		}

		if (variables.count(str) == 0) {
			if (!must_succeed)
				goto finished;
			log_error("Constant function `%s' has unsupported statement %s at %s.\n",
				str.c_str(), type2str(stmt->type).c_str(), stmt->location.to_string().c_str());
		}

		if (stmt->type == AST_LOCALPARAM)
		{
			while (stmt->simplify(true, 1, -1, false)) { }

			current_scope[stmt->str] = stmt.get();
			temporary_nodes.push_back(std::move(stmt));

			block->children.erase(block->children.begin());
			continue;
		}

		if (stmt->type == AST_ASSIGN_EQ)
		{
			// a named assignment pattern to a struct variable: put the
			// values in member order now, before substitution and folding
			// replace the value nodes (and with them their member keys);
			// packed below once every value is constant (register v3 D13)
			if (stmt->children.at(1)->type == AST_ASSIGN_PATTERN && stmt->children.at(0)->type == AST_IDENTIFIER &&
					stmt->children.at(0)->children.empty()) {
				AstNode *lhs = stmt->children.at(0).get();
				AstNode *decl = lhs->id2ast ? lhs->id2ast : (current_scope.count(lhs->str) ? current_scope.at(lhs->str) : nullptr);
				AstNode *st = decl ? resolve_wiretype_struct(decl) : nullptr;
				AstNode *pat = stmt->children.at(1).get();
				auto bare = [](std::string n) { return (!n.empty() && n[0] == '\\') ? n.substr(1) : n; };
				bool named = !pat->children.empty();
				for (auto &ch : pat->children)
					if (!ch || !ch->attributes.count(ID(sv_member)))
						named = false;
				if (st && st->type == AST_STRUCT && named) {
					dict<std::string, std::unique_ptr<AstNode>> by_name;
					for (auto &ch : pat->children)
						by_name[bare(ch->attributes.at(ID(sv_member))->str)] = std::move(ch);
					std::vector<std::unique_ptr<AstNode>> ordered;
					bool ok = true;
					for (auto &it : st->children) {
						if (!it || !(it->type == AST_STRUCT_ITEM || it->type == AST_STRUCT || it->type == AST_UNION))
							continue;
						std::string n = bare(it->str);
						if (!by_name.count(n)) {
							ok = false;
							break;
						}
						ordered.push_back(std::move(by_name.at(n)));
						by_name.erase(n);
					}
					if (ok && by_name.empty()) {
						for (auto &ch : ordered)
							ch->attributes.erase(ID(sv_member));
						pat->children = std::move(ordered);
					} else {
						// a missing or unknown key: leave it to the general
						// pattern path (which reports it); restore what we can
						pat->children.clear();
						for (auto &ch : ordered)
							if (ch) pat->children.push_back(std::move(ch));
						for (auto &kv : by_name)
							pat->children.push_back(std::move(kv.second));
					}
				}
			}
			if (stmt->children.at(0)->type == AST_IDENTIFIER && stmt->children.at(0)->children.size() != 0 &&
					stmt->children.at(0)->children.at(0)->type == AST_RANGE)
				if (!stmt->children.at(0)->children.at(0)->replace_variables(variables, fcall, must_succeed))
					goto finished;
			if (!stmt->children.at(1)->replace_variables(variables, fcall, must_succeed))
				goto finished;
			while (stmt->simplify(true, 1, -1, false)) { }

			if (stmt->type != AST_ASSIGN_EQ)
				continue;

			// an assignment pattern of constants to a struct variable
			// (`c = '{XLEN: x, En: 1}` in a config-building function):
			// pack it MSB-first in member order (register v3 D13)
			if (stmt->children.at(1)->type == AST_ASSIGN_PATTERN && stmt->children.at(0)->type == AST_IDENTIFIER &&
					stmt->children.at(0)->children.empty() && stmt->children.at(0)->id2ast) {
				AstNode *st = resolve_wiretype_struct(stmt->children.at(0)->id2ast);
				AstNode *pat = stmt->children.at(1).get();
				std::vector<AstNode*> items;
				if (st && st->type == AST_STRUCT)
					for (auto &it : st->children)
						if (it && it->range_valid && (it->type == AST_STRUCT_ITEM || it->type == AST_STRUCT || it->type == AST_UNION))
							items.push_back(it.get());
				bool ok = !items.empty() && GetSize(items) == GetSize(pat->children);
				for (auto &ch : pat->children)
					if (!ch || ch->type != AST_CONSTANT)
						ok = false;
				if (ok) {
					auto bare = [](std::string n) { return (!n.empty() && n[0] == '\\') ? n.substr(1) : n; };
					dict<std::string, AstNode*> by_name;
					bool named = true;
					for (auto &ch : pat->children) {
						if (!ch->attributes.count(ID(sv_member)))
							named = false;
						else
							by_name[bare(ch->attributes.at(ID(sv_member))->str)] = ch.get();
					}
					std::vector<RTLIL::State> bits;
					// struct items are MSB first; build LSB first from the back
					for (int i = GetSize(items) - 1; i >= 0 && ok; i--) {
						AstNode *val = named ? (by_name.count(bare(items[i]->str)) ? by_name.at(bare(items[i]->str)) : nullptr)
								: pat->children[i].get();
						if (!val) {
							ok = false;
							break;
						}
						int w = items[i]->range_left - items[i]->range_right + 1;
						RTLIL::Const c = val->bitsAsConst(w);
						for (int b = 0; b < w; b++)
							bits.push_back(c[b]);
					}
					if (ok)
						stmt->children[1] = mkconst_bits(stmt->location, bits, false);
				}
			}

			if (stmt->children.at(1)->type != AST_CONSTANT) {
				if (!must_succeed)
					goto finished;
				stmt->input_error("Non-constant expression in constant function\n%s: ... called from here. X\n",
						fcall->loc_string().c_str());
			}

			// a signed struct member on the left (`cfg.NrTriggers = ...`
			// with an `int` member) is wrapped as $signed(base[range]); the
			// sign does not matter to a store
			if (stmt->children.at(0)->type == AST_TO_SIGNED && stmt->children.at(0)->children.size() == 1 &&
					stmt->children.at(0)->children.at(0)->type == AST_IDENTIFIER) {
				auto inner = std::move(stmt->children.at(0)->children.at(0));
				stmt->children.at(0) = std::move(inner);
			}
			if (stmt->children.at(0)->type != AST_IDENTIFIER) {
				if (!must_succeed)
					goto finished;
				stmt->input_error("Unsupported composite left hand side in constant function\n%s: ... called from here.\n",
						fcall->loc_string().c_str());
			}

			if (!variables.count(stmt->children.at(0)->str)) {
				// the result variable spelled by the function's own name, bare
				// or package-qualified (`plus1 = ...` / `u_pkg::plus1 = ...` in
				// a wildcard-imported package function): the declaration was
				// renamed to the prefixed `$result` (str), the reference was
				// not.  Compare the bare base names and redirect (register v3
				// D7c: "Constant function did not evaluate" on every imported
				// constant function, ~95 OpenTitan files).
				auto bare = [](std::string n) {
					if (!n.empty() && n[0] == '\\')
						n = n.substr(1);
					size_t q = n.rfind("::");
					return q == std::string::npos ? n : n.substr(q + 2);
				};
				if (variables.count(str) && bare(stmt->children.at(0)->str) == bare(fcall->str))
					stmt->children.at(0)->str = str;
			}
			if (!variables.count(stmt->children.at(0)->str)) {
				if (!must_succeed)
					goto finished;
				stmt->input_error("Assignment to non-local variable in constant function\n%s: ... called from here.\n",
						fcall->loc_string().c_str());
			}

			if (stmt->children.at(0)->children.empty()) {
				variables[stmt->children.at(0)->str].val = stmt->children.at(1)->bitsAsConst(variables[stmt->children.at(0)->str].val.size());
			} else {
				AstNode *range = stmt->children.at(0)->children.at(0).get();
				if (!range->range_valid) {
					if (!must_succeed)
						goto finished;
					range->input_error("Non-constant range\n%s: ... called from here.\n", fcall->loc_string());
				}
				int offset = min(range->range_left, range->range_right);
				int width = std::abs(range->range_left - range->range_right) + 1;
				// a struct member on the left (`cfg.XLEN = ...`): simplify
				// rewrote it into a member-relative range on the base
				// variable, with the member in the wiretype attribute
				// (genrtlil adds the same base offset)
				if (AstNode *mn = stmt->children.at(0)->get_struct_member())
					offset += mn->range_right;
				varinfo_t &v = variables[stmt->children.at(0)->str];
				RTLIL::Const r = stmt->children.at(1)->bitsAsConst(v.val.size());
				for (int i = 0; i < width; i++) {
					int index = i + offset - v.offset;
					if (v.range_swapped)
						index = -index;
					v.val.set(index, r.at(i));
				}
			}

			block->children.erase(block->children.begin());
			continue;
		}

		if (stmt->type == AST_FOR)
		{
			stmt->type = AST_WHILE;
			log_assert(stmt->children.size() > 2);
			auto yoink0 = std::move(stmt->children.at(0));
			log_assert(stmt->children.size() > 2);
			auto yoink2 = std::move(stmt->children.at(2));
			yoink2->set_attribute(ID(sv_for_step), AstNode::mkconst_int(yoink2->location, 1, false));
			stmt->children.at(3)->children.push_back(std::move(yoink2));
			stmt->children.erase(stmt->children.begin() + 2);
			stmt->children.erase(stmt->children.begin());
			block->children.insert(block->children.begin(), std::move(yoink0));
			log_assert(stmt->children.size() == 2);
			continue;
		}

		if (stmt->type == AST_WHILE)
		{
			// IEEE 1800-2009 12.7: do-while runs the body before testing the condition.
			if (stmt->get_bool_attribute(ID(sv_do)) && stmt->integer == 0) {
				stmt->integer = 1;
				block->children.insert(block->children.begin(), stmt->children.at(1)->clone());
				continue;
			}
			auto cond = stmt->children.at(0)->clone();
			if (!cond->replace_variables(variables, fcall, must_succeed))
				goto finished;
			cond->set_in_param_flag(true);
			while (cond->simplify(true, 1, -1, false)) { }

			if (cond->type != AST_CONSTANT) {
				if (!must_succeed)
					goto finished;
				stmt->input_error("Non-constant expression in constant function\n%s: ... called from here.\n",
						fcall->loc_string().c_str());
			}

			if (cond->asBool()) {
				if (++const_while_steps > 16384) {
					degrade(fcall->location.to_loc(), "Constant function loop stopped after 16384 iterations.\n");
					block->children.erase(block->children.begin());
					continue;
				}
				block->children.insert(block->children.begin(), stmt->children.at(1)->clone());
			} else {
				block->children.erase(block->children.begin());
			}
			continue;
		}

		if (stmt->type == AST_REPEAT)
		{
			auto num = stmt->children.at(0)->clone();
			if (!num->replace_variables(variables, fcall, must_succeed))
				goto finished;
			num->set_in_param_flag(true);
			while (num->simplify(true, 1, -1, false)) { }

			if (num->type != AST_CONSTANT) {
				if (!must_succeed)
					goto finished;
				stmt->input_error("Non-constant expression in constant function\n%s: ... called from here.\n",
						fcall->loc_string().c_str());
			}

			temporary_nodes.push_back(std::move(stmt));
			block->children.erase(block->children.begin());
			for (int i = 0; i < num->bitsAsConst().as_int(); i++)
				block->children.insert(block->children.begin(), temporary_nodes.back()->children.at(1)->clone());

			continue;
		}

		if (stmt->type == AST_TCALL && (stmt->str == "\\break" || stmt->str == "\\continue"))
		{
			size_t loop_at = block->children.size();
			for (size_t i = 1; i < block->children.size(); i++) {
				auto t = block->children[i]->type;
				if (t == AST_WHILE || t == AST_REPEAT || t == AST_FOR) {
					loop_at = i;
					break;
				}
			}
			size_t erase_upto = loop_at;
			if (stmt->str == "\\break" && loop_at < block->children.size())
				erase_upto = loop_at + 1;
			else if (stmt->str == "\\continue" && loop_at > 0 && loop_at < block->children.size() &&
					block->children[loop_at - 1]->get_bool_attribute(ID(sv_for_step)))
				erase_upto = loop_at - 1;
			else if (loop_at == block->children.size())
				erase_upto = 1;
			block->children.erase(block->children.begin(), block->children.begin() + erase_upto);
			continue;
		}

		if (stmt->type == AST_CASE)
		{
			auto expr = stmt->children.at(0)->clone();
			if (!expr->replace_variables(variables, fcall, must_succeed))
				goto finished;
			expr->set_in_param_flag(true);
			while (expr->simplify(true, 1, -1, false)) { }

			AstNode *sel_case = nullptr;
			std::unique_ptr<AstNode> sel_case_copy = nullptr;
			for (size_t i = 1; i < stmt->children.size(); i++)
			{
				bool found_match = false;
				log_assert(stmt->children.at(i)->type == AST_COND || stmt->children.at(i)->type == AST_CONDX || stmt->children.at(i)->type == AST_CONDZ);

				if (stmt->children.at(i)->children.front()->type == AST_DEFAULT) {
					sel_case = stmt->children.at(i)->children.back().get();
					continue;
				}

				for (size_t j = 0; j+1 < stmt->children.at(i)->children.size() && !found_match; j++)
				{
					auto cond = stmt->children.at(i)->children.at(j)->clone();
					if (!cond->replace_variables(variables, fcall, must_succeed))
						goto finished;

					cond = std::make_unique<AstNode>(location, AST_EQ, expr->clone(), std::move(cond));
					cond->set_in_param_flag(true);
					while (cond->simplify(true, 1, -1, false)) { }

					if (cond->type != AST_CONSTANT) {
						if (!must_succeed)
							goto finished;
						stmt->input_error("Non-constant expression in constant function\n%s: ... called from here.\n",
								fcall->loc_string().c_str());
					}

					found_match = cond->asBool();
				}

				if (found_match) {
					sel_case = stmt->children.at(i)->children.back().get();
					break;
				}
			}
			if (sel_case)
				sel_case_copy = sel_case->clone();

			block->children.erase(block->children.begin());
			if (sel_case_copy)
				block->children.insert(block->children.begin(), std::move(sel_case_copy));
			continue;
		}

		// a function-local `bit x = expr;` reaches here as the declaration
		// plus `initial x <= expr;`: inside a function it is an ordinary
		// assignment, executed in place (register v3 D13: cva6 build_config
		// opens with such declarations)
		if (stmt->type == AST_INITIAL)
		{
			std::vector<std::unique_ptr<AstNode>> body;
			std::function<void(AstNode *)> lower = [&](AstNode *n) {
				if (n->type == AST_ASSIGN_LE)
					n->type = AST_ASSIGN_EQ;
				for (auto &c : n->children)
					if (c && (c->type == AST_BLOCK || c->type == AST_ASSIGN_LE))
						lower(c.get());
			};
			for (auto &c : stmt->children) {
				lower(c.get());
				body.push_back(std::move(c));
			}
			block->children.erase(block->children.begin());
			block->children.insert(block->children.begin(), std::make_move_iterator(body.begin()), std::make_move_iterator(body.end()));
			block->fixup_hierarchy_flags();
			continue;
		}

		if (stmt->type == AST_BLOCK)
		{
			if (!stmt->str.empty())
				stmt->expand_genblock(stmt->str + ".");
			auto* stmt_leaky = stmt.get();
			temporary_nodes.push_back(std::move(stmt));
			block->children.erase(block->children.begin());
			block->children.reserve(block->children.size() + stmt_leaky->children.size());
			block->children.insert(block->children.begin(),
				std::make_move_iterator(stmt_leaky->children.begin()),
				std::make_move_iterator(stmt_leaky->children.end()));
			stmt_leaky->children.clear();
			block->fixup_hierarchy_flags();
			continue;
		}

		// log("C\n");
		if (getenv("SVA_DEBUG_FCALL"))
			stmt->dumpAst(nullptr, "unsupported> ");
		if (!must_succeed)
			goto finished;
		stmt->input_error("Unsupported language construct in constant function\n%s: ... called from here.\n",
				fcall->loc_string().c_str());
		log_abort();
	}

	result = AstNode::mkconst_bits(location, variables.at(str).val.to_bits(), variables.at(str).is_signed);

finished:
	current_scope = backup_scope;
	return result;
}

void AstNode::allocateDefaultEnumValues()
{
	log_assert(type==AST_ENUM);
	log_assert(children.size() > 0);
	if (children.front()->attributes.count(ID::enum_base_type))
		return; // already elaborated
	int last_enum_int = -1;
	for (auto& node : children) {
		log_assert(node->type==AST_ENUM_ITEM);
		node->set_attribute(ID::enum_base_type, mkconst_str(node->location, str));
		for (size_t i = 0; i < node->children.size(); i++) {
			switch (node->children[i]->type) {
			case AST_NONE:
				// replace with auto-incremented constant
				node->children[i] = AstNode::mkconst_int(node->location, ++last_enum_int, true);
				break;
			case AST_CONSTANT:
				// explicit constant (or folded expression)
				// TODO: can't extend 'x or 'z item
				last_enum_int = node->children[i]->integer;
				break;
			default:
				// ignore ranges
				break;
			}
			// TODO: range check
		}
	}
}

bool AstNode::is_recursive_function() const
{
	std::set<const AstNode *> visited;
	std::function<bool(const AstNode *node)> visit = [&](const AstNode *node) {
		if (visited.count(node))
			return node == this;
		visited.insert(node);
		if (node->type == AST_FCALL) {
			auto it = current_scope.find(node->str);
			if (it != current_scope.end() && visit(it->second))
				return true;
		}
		for (auto& child : node->children) {
			if (visit(child.get()))
				return true;
		}
		return false;
	};

	log_assert(type == AST_FUNCTION);
	return visit(this);
}

std::pair<AstNode*, AstNode*> AstNode::get_tern_choice()
{
	if (children.empty() || !children[0] || !children[0]->isConst())
		return {};

	bool found_sure_true = false;
	bool found_maybe_true = false;

	if (children[0]->type == AST_CONSTANT)
		for (auto &bit : children[0]->bits) {
			if (bit == RTLIL::State::S1)
				found_sure_true = true;
			if (bit > RTLIL::State::S1)
				found_maybe_true = true;
		}
	else
		found_sure_true = children[0]->asReal(true) != 0;

	AstNode *choice = nullptr, *not_choice = nullptr;
	if (found_sure_true)
		choice = children[1].get(), not_choice = children[2].get();
	else if (!found_maybe_true)
		choice = children[2].get(), not_choice = children[1].get();

	return {choice, not_choice};
}

std::string AstNode::try_pop_module_prefix() const
{
	AstNode *current_scope_ast = (current_ast_mod == nullptr) ? current_ast : current_ast_mod;
	size_t pos = str.find('.', 1);
	if (str[0] == '\\' && pos != std::string::npos) {
		std::string new_str = "\\" + str.substr(pos + 1);
		if (current_scope.count(new_str)) {
			std::string prefix = str.substr(0, pos);
			auto it = current_scope_ast->attributes.find(ID::hdlname);
			if ((it != current_scope_ast->attributes.end() && it->second->str == prefix.substr(1))
					|| prefix == current_scope_ast->str)
				return new_str;
		}
	}
	return str;
}

YOSYS_NAMESPACE_END
