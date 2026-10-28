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
 *  The Verilog frontend.
 *
 *  This frontend is using the AST frontend library (see frontends/ast/).
 *  Thus this frontend does not generate RTLIL code directly but creates an
 *  AST directly from the Verilog parse tree and then passes this AST to
 *  the AST frontend library.
 *
 *  ---
 *
 *  This is the actual bison parser for Verilog code. The AST ist created directly
 *  from the bison reduce functions here. Note that this code uses a few global
 *  variables to hold the state of the AST generator and therefore this parser is
 *  not reentrant.
 *
 */

%require "3.6"
%language "c++"
%define api.value.type variant
%define api.prefix {frontend_verilog_yy}
%define api.token.constructor
%define api.location.type {Location}

%param { YOSYS_NAMESPACE_PREFIX VERILOG_FRONTEND::VerilogLexer* lexer }
%parse-param { YOSYS_NAMESPACE_PREFIX VERILOG_FRONTEND::ParseState* extra }
%parse-param { YOSYS_NAMESPACE_PREFIX VERILOG_FRONTEND::ParseMode* mode }


%code requires {
	#include "kernel/yosys_common.h"
	#include "frontends/verilog/verilog_error.h"
	#include "frontends/verilog/verilog_location.h"
	YOSYS_NAMESPACE_BEGIN
	namespace VERILOG_FRONTEND {
		struct ParseState;
		struct ParseMode;
		class VerilogLexer;
	};
	YOSYS_NAMESPACE_END
}

%code provides {
	USING_YOSYS_NAMESPACE;
	using namespace AST;
	using namespace VERILOG_FRONTEND;
	using parser = frontend_verilog_yy::parser;
	YOSYS_NAMESPACE_BEGIN
	namespace VERILOG_FRONTEND {
		typedef std::map<std::string, AST::AstNode*> UserTypeMap;
		struct ParseState {
			int port_counter;
			dict<std::string, int> port_stubs;
			std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> attr_list;
			dict<IdString, std::unique_ptr<AstNode>> default_attr_list;
			std::stack<std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>>> attr_list_stack;
			std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> albuf;
			std::vector<UserTypeMap> user_type_stack;
			dict<std::string, AstNode*> pkg_user_types;
			std::vector<AstNode*> ast_stack;
			std::unique_ptr<AstNode> astbuf1, astbuf2, astbuf3;
			// struct_decl: the hidden typedef of an anonymous struct whose
			// variables are unpacked arrays (empty until the first one)
			std::string anon_struct_type;
			int anon_struct_count = 0;
			size_t bind_mark = 0;
			AstNode* cell_hack;
			AstNode* member_hack;
			struct AstNode *current_function_or_task;
			struct AstNode *current_ast, *current_ast_mod;
			int current_function_or_task_port_id;
			std::vector<char> case_type_stack;
			bool do_not_require_port_stubs;
			bool current_wire_rand, current_wire_const, current_wire_automatic;
			bool current_modport_input, current_modport_output;
			bool default_nettype_wire = true;
			// inside an always_comb / always_latch body: a procedural event
			// control is a syntax error there (IEEE 1800 9.2.2.2; upstream
			// rejects it at the '@', tests/various/svalways.sh)
			bool in_comb_body = false;
			std::vector<bool> sv_keyword_stack;
			std::istream* lexin;
			// IEEE 1800-2009 default clocking / default disable iff (clause 16).
			std::unique_ptr<AstNode> sva_default_clock;
			// IEEE 14.14: the module's `global clocking` event; `@($global_clock)`
			// in a concurrent assertion means this event
			std::unique_ptr<AstNode> sva_global_clock;
			std::unique_ptr<AstNode> sva_default_disable;
			// IEEE 1800-2009 17.2: checker formal ports (identifier with an
			// optional default-expression child) collected while parsing a
			// checker header, plus the enclosing default clocking/disable
			// restored at endchecker.
			std::vector<std::unique_ptr<AstNode>> checker_formals;
			std::unique_ptr<AstNode> checker_clk_backup;
			std::unique_ptr<AstNode> checker_dis_backup;
			// Nonzero while parsing a checker body: assertions get tagged
			// so checker expansion can apply the checker's default clocking.
			int in_checker = 0;
			// Condition of the immediate assertion whose else-statement is being parsed.
			std::unique_ptr<AstNode> pending_assert_cond;
			bool collecting_let_ports = false;
			// inside a property declaration body: IEEE 1800-2017 16.15 applies
			// `default disable iff` to the assertion statement, not to the
			// declarations it instantiates
			bool in_property_decl = false;
			// property/sequence locals (IEEE 1800-2017 16.10): each gets its
			// own wire of the declared type, named uniquely; the body's
			// references are renamed when the declaration completes.  They
			// were one 32-bit signed wire per NAME, shared by every
			// declaration and aliasing a module signal of that name (a
			// `logic [39:0]` local compared 32 bits; a typedef local was an
			// implicit 1-bit wire)
			dict<std::string, std::string> sva_local_renames;
			int sva_local_uid = 0;
			int sva_local_kind = 0;   // 0 logic [range], 1 int/integer, 2 user type
			std::unique_ptr<AstNode> sva_local_range;
			std::string sva_local_tname;
			std::vector<std::string> let_port_names;
			pool<std::string> interface_names;
			// struct/union member default values (`bit lo = c;`), collected
			// while parsing a struct body and injected as initial assignments
			// at each instance declaration
			std::vector<std::vector<std::pair<std::string, std::unique_ptr<AstNode>>>> struct_defaults_stack;
			std::vector<std::pair<std::string, std::unique_ptr<AstNode>>> pending_struct_defaults;
			std::map<std::string, std::vector<std::pair<std::string, std::unique_ptr<AstNode>>>> struct_defaults_by_type;

			AstNode* saveChild(std::unique_ptr<AstNode> child);
			AstNode* pushChild(std::unique_ptr<AstNode> child);
			void addWiretypeNode(std::string *name, AstNode* node);
			void addTypedefNode(std::string *name, std::unique_ptr<AstNode> node);
			void addTypeParamNode(std::string *name, std::unique_ptr<AstNode> node);
			void emitStructDefaults(const parser::location_type &loc, AstNode *parent, const std::string &varname, const std::vector<std::pair<std::string, std::unique_ptr<AstNode>>> &defs);
			void enterTypeScope();
			void exitTypeScope();
			bool isInLocalScope(const std::string *name);
			void rewriteGenForDeclInit(AstNode *loop);
			void ensureAsgnExprAllowed(const parser::location_type loc, bool sv_mode);
			const AstNode *addIncOrDecStmt(std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> stmt_attr,
									std::unique_ptr<AstNode> lhs,
									std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> op_attr, AST::AstNodeType op,
									parser::location_type loc);
			std::unique_ptr<AstNode> addIncOrDecExpr(std::unique_ptr<AstNode> lhs,
								 std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> attr,
								 AST::AstNodeType op, parser::location_type loc, bool undo, bool sv_mode);
			// add a binary operator assignment statement, e.g., a += b
			std::unique_ptr<AstNode> addAsgnBinopStmt(std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> attr,
								  std::unique_ptr<AstNode> eq_lhs, AST::AstNodeType op, std::unique_ptr<AstNode> rhs);
		};
		struct ParseMode {
			bool noassert = false;
			bool noassume = false;
			bool norestrict = false;
			bool sv = false;
			bool formal = false;
			bool lib = false;
			bool specify = false;
			bool assume_asserts = false;
			bool assert_assumes = false;
			// 0: no IEEE 1800 year selected (-sv is this permissive mode).
			// 2009, 2012, 2017, 2023: -std=1800-YYYY. Later revisions are
			// a superset of earlier ones for features this frontend gates.
			int std_year = 0;
		};
	};
	YOSYS_NAMESPACE_END
}

%code {
	#include <list>
	#include <stack>
	#include <memory>
	#include <string.h>
	#include "kernel/log.h"
	#include "frontends/verilog/verilog_lexer.h"

	USING_YOSYS_NAMESPACE
	using namespace AST;
	using namespace VERILOG_FRONTEND;

	// Silly little C adapter between C++ bison and C++ flex
	auto frontend_verilog_yylex(YOSYS_NAMESPACE_PREFIX VERILOG_FRONTEND::VerilogLexer* lexer) {
		return lexer->nextToken();
	}

	#define SET_AST_NODE_LOC(WHICH, BEGIN, END) (WHICH)->location = location_range(BEGIN, END)

	#define SET_RULE_LOC(LHS, BEGIN, END) \
		do { (LHS).begin = BEGIN.begin; \
		(LHS).end = (END).end; } while(0)

	YOSYS_NAMESPACE_BEGIN
	namespace VERILOG_FRONTEND {

		static Location location_range(Location begin, Location end) {
			return Location(begin.begin, end.end);
		}

		static void append_attr(AstNode *ast, std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> al)
		{
			for (auto &it : *al) {
				ast->attributes[it.first] = std::move(it.second);
			}
		}

		static void append_attr_clone(AstNode *ast, std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> &al)
		{
			for (auto &it : *al) {
				ast->attributes[it.first] = it.second->clone();
			}
		}

		static std::unique_ptr<AstNode> makeRange(parser::location_type loc, int msb = 31, int lsb = 0, bool isSigned = true)
		{
			auto range = std::make_unique<AstNode>(loc, AST_RANGE);
			range->children.push_back(AstNode::mkconst_int(loc, msb, true));
			range->children.push_back(AstNode::mkconst_int(loc, lsb, true));
			range->is_signed = isSigned;
			return range;
		}

		static void addRange(AstNode *parent, int msb = 31, int lsb = 0, bool isSigned = true)
		{
			auto range = makeRange(parent->location, msb, lsb, isSigned);
			parent->children.push_back(std::move(range));
		}

		static std::unique_ptr<AstNode> checkRange(AstNode *type_node, std::unique_ptr<AstNode> range_node)
		{
			if (type_node->range_left >= 0 && type_node->range_right >= 0) {
				// type already restricts the range
				if (range_node) {
					err_at_loc(type_node->location, "integer/genvar types cannot have packed dimensions.");
				}
				else {
					range_node = makeRange(type_node->location, type_node->range_left, type_node->range_right, false);
				}
			}

			if (range_node) {
				bool valid = true;
				if (range_node->type == AST_RANGE) {
					valid = range_node->children.size() == 2;
				} else {  // AST_MULTIRANGE
					for (auto& child : range_node->children) {
						valid = valid && child->children.size() == 2;
					}
				}
				if (!valid)
					err_at_loc(type_node->location, "wire/reg/logic packed dimension must be of the form [<expr>:<expr>]");
			}

			return range_node;
		}

		static void rewriteRange(AstNode *rangeNode)
		{
			if (rangeNode->type == AST_RANGE && rangeNode->children.size() == 1) {
				// SV array size [n], rewrite as [0:n-1]
				rangeNode->children.push_back(std::make_unique<AstNode>(rangeNode->location, AST_SUB, std::move(rangeNode->children[0]), AstNode::mkconst_int(rangeNode->location, 1, true)));
				rangeNode->children[0] = AstNode::mkconst_int(rangeNode->location, 0, false);
			}
		}

		static void rewriteAsMemoryNode(AstNode *node, std::unique_ptr<AstNode> rangeNode)
		{
			node->type = AST_MEMORY;
			if (rangeNode->type == AST_MULTIRANGE) {
				for (auto& child : rangeNode->children)
					rewriteRange(child.get());
			} else
				rewriteRange(rangeNode.get());
			node->children.push_back(std::move(rangeNode));
		}

		static void checkLabelsMatch(const frontend_verilog_yy::parser::location_type& loc, const char *element, const std::string* before, const std::string *after)
		{
			if (!before && after)
				err_at_loc(loc, "%s missing where end label (%s) was given.",
					element, after->c_str() + 1);
			if (before && after && *before != *after)
				err_at_loc(loc, "%s (%s) and end label (%s) don't match.",
					element, before->c_str() + 1, after->c_str() + 1);
		}

		// register v3 A2: a simulation-only statement the formal/synthesis
		// flow does not model is a frontend degradation like any other (an
		// error, or under -permissive a warning plus (* sv_degraded *) on
		// the module being parsed)
		static void note_dropped_delay(ParseState *extra, const frontend_verilog_yy::parser::location_type& loc)
		{
			AstNode *mod = extra->current_ast_mod;
			if (mod == nullptr || mod->type != AST_MODULE)
				return;
			auto l = loc.to_loc();
			std::string where = stringf("%s:%d", l.filename.c_str(), l.start_line);
			IdString attr = IdString("\\sim_dropped_delays");
			auto it = mod->attributes.find(attr);
			std::string cur = (it != mod->attributes.end() && it->second) ? it->second->str : "";
			mod->attributes[attr] = AstNode::mkconst_str(mod->location, cur.empty() ? where : cur + " " + where);
		}

		static void parse_degrade(ParseState *extra, const frontend_verilog_yy::parser::location_type& loc, const char *what,
				bool upstream_leniency = false)
		{
			struct Restore {
				AstNode *mod = AST_INTERNAL::current_ast_mod;
				RTLIL::Module *rtl = AST_INTERNAL::current_module;
				~Restore() { AST_INTERNAL::current_ast_mod = mod; AST_INTERNAL::current_module = rtl; }
			} restore;
			AST_INTERNAL::current_ast_mod = extra->current_ast_mod;
			AST_INTERNAL::current_module = nullptr;
			// a leniency stock yosys has too (statement delays) warns and tags
			// but is never an error, like implicit nets (AST::degrade_warn)
			if (upstream_leniency)
				AST::degrade_warn(loc.to_loc(), "%s (simulation-only; not modeled).", what);
			else
				AST::degrade(loc.to_loc(), "%s (simulation-only; not modeled).", what);
		}

		AstNode* ParseState::saveChild(std::unique_ptr<AstNode> child) {
			auto* child_leaky = child.get();
			ast_stack.back()->children.push_back(std::move(child));
			return child_leaky;
		}
		AstNode* ParseState::pushChild(std::unique_ptr<AstNode> child) {
			auto* child_leaky = saveChild(std::move(child));
			ast_stack.push_back(child_leaky);
			return child_leaky;
		}

		void ParseState::addWiretypeNode(std::string *name, AstNode* node)
		{
			log_assert(node);
			node->is_custom_type = true;
			node->children.push_back(std::make_unique<AstNode>(node->location, AST_WIRETYPE));
			node->children.back()->str = *name;
		}

		// `parameter type T = ...` / `#(type T = ...)`: a typedef that is also
		// a parameter.  The typedef carries the default type; the mark makes
		// the AST module advertise T to hierarchy (avail_parameters) so an
		// instance's `.T(<type>)` override reaches derive, which splices the
		// named type in (register v3 D0).
		void ParseState::addTypeParamNode(std::string *name, std::unique_ptr<AstNode> node)
		{
			auto loc = node->location;
			addTypedefNode(name, std::move(node));
			AstNode *tnode = user_type_stack.back().at(*name);
			tnode->set_attribute(ID(type_param), AstNode::mkconst_int(loc, 1, false));
		}

		void ParseState::addTypedefNode(std::string *name, std::unique_ptr<AstNode> node)
		{
			log_assert((bool)node);
			AstNode* tnode = saveChild(std::make_unique<AstNode>(node->location, AST_TYPEDEF, std::move(node)));
			log_assert((bool)name);
			tnode->str = *name;
			auto &user_types = user_type_stack.back();
			user_types[*name] = tnode;
			if (current_ast_mod && current_ast_mod->type == AST_PACKAGE) {
				// typedef inside a package so we need the qualified name
				auto qname = current_ast_mod->str + "::" + (*name).substr(1);
				pkg_user_types[qname] = tnode;
			}
			// typedef inside a class body: register `class::name` so that
			// references from outside the class (e.g. in out-of-block method
			// definitions) lex as package-qualified user types.
			for (auto it = ast_stack.rbegin(); it != ast_stack.rend(); ++it) {
				AstNode *scope = *it;
				if (scope->type == AST_CLASS) {
					auto qname = scope->str + "::" + (*name).substr(1);
					pkg_user_types[qname] = tnode;
					break;
				}
			}
		}

		void ParseState::emitStructDefaults(const parser::location_type &loc, AstNode *parent, const std::string &varname, const std::vector<std::pair<std::string, std::unique_ptr<AstNode>>> &defs)
		{
			if (defs.empty())
				return;
			// member defaults initialize each instance (IEEE 1800-2009 7.2.2):
			// model as an initial block assigning var.member = default
			auto init = std::make_unique<AstNode>(loc, AST_INITIAL);
			auto block = std::make_unique<AstNode>(loc, AST_BLOCK);
			for (auto &def : defs) {
				auto lhs = std::make_unique<AstNode>(loc, AST_IDENTIFIER);
				lhs->str = varname + "." + def.first;
				auto asgn = std::make_unique<AstNode>(loc, AST_ASSIGN_EQ, std::move(lhs), def.second->clone());
				SET_AST_NODE_LOC(asgn.get(), loc, loc);
				block->children.push_back(std::move(asgn));
			}
			init->children.push_back(std::move(block));
			parent->children.push_back(std::move(init));
		}

		void ParseState::enterTypeScope()
		{
			user_type_stack.push_back(UserTypeMap());
		}

		void ParseState::exitTypeScope()
		{
			user_type_stack.pop_back();
		}

		bool ParseState::isInLocalScope(const std::string *name)
		{
			// tests if a name was declared in the current block scope
			auto &user_types = user_type_stack.back();
			return (user_types.count(*name) > 0);
		}

		// This transforms a loop like
		//   for (genvar i = 0; i < 10; i++) begin : blk
		// to
		//   genvar _i;
		//   for (_i = 0; _i < 10; _i++) begin : blk
		//     localparam i = _i;
		// where `_i` is actually some auto-generated name.
		void ParseState::rewriteGenForDeclInit(AstNode *loop)
		{
			// check if this generate for loop contains an inline declaration
			log_assert(loop->type == AST_GENFOR);
			auto& decl = loop->children[0];
			if (decl->type == AST_ASSIGN_EQ)
				return;

			log_assert(decl->type == AST_GENVAR);
			log_assert(loop->children.size() == 5);

			// identify each component of the loop
			AstNode *init = loop->children[1].get();
			AstNode *cond = loop->children[2].get();
			AstNode *incr = loop->children[3].get();
			AstNode *body = loop->children[4].get();
			log_assert(init->type == AST_ASSIGN_EQ);
			log_assert(incr->type == AST_ASSIGN_EQ);
			log_assert(body->type == AST_GENBLOCK);

			// create a unique name for the genvar
			std::string old_str = decl->str;
			std::string new_str = stringf("$genfordecl$%d$%s", autoidx++, old_str);

			// rename and move the genvar declaration to the containing description
			decl->str = new_str;
			log_assert(current_ast_mod != nullptr);
			current_ast_mod->children.push_back(std::move(decl));

			// create a new localparam with old name so that the items in the loop
			// can simply use the old name and shadow it as necessary
			auto indirect = std::make_unique<AstNode>(loop->location, AST_LOCALPARAM);
			indirect->str = old_str;
			auto ident = std::make_unique<AstNode>(loop->location, AST_IDENTIFIER);
			ident->str = new_str;
			indirect->children.push_back(std::move(ident));

			body->children.insert(body->children.begin(), std::move(indirect));

			// only perform the renaming for the initialization, guard, and
			// incrementation to enable proper shadowing of the synthetic localparam
			std::function<void(AstNode*)> substitute = [&](AstNode *node) {
				if (node->type == AST_IDENTIFIER && node->str == old_str)
					node->str = new_str;
				for (auto& child : node->children)
					substitute(child.get());
			};
			substitute(init);
			substitute(cond);
			substitute(incr);
			loop->children.erase(loop->children.begin());
		}

		void ParseState::ensureAsgnExprAllowed(const parser::location_type loc, bool sv_mode)
		{
			if (!sv_mode)
				err_at_loc(loc, "Assignments within expressions are only supported in SystemVerilog mode.");
			for (auto it = ast_stack.rbegin(); it != ast_stack.rend(); ++it) {
				auto t = (*it)->type;
				if (t == AST_BLOCK || t == AST_FUNCTION || t == AST_TASK)
					return;
				// transparent frames such as a function call under
				// construction (AST_FCALL) may sit on top of the stack;
				// the side-effect statement attaches to the enclosing
				// procedure (see addIncOrDecStmt)
				if (t == AST_FCALL || t == AST_TCALL)
					continue;
				break;
			}
			err_at_loc(loc, "Assignments within expressions are only permitted within procedures.");
		}

		// add a pre/post-increment/decrement statement
		const AstNode *ParseState::addIncOrDecStmt(std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> stmt_attr,
							std::unique_ptr<AstNode> lhs,
							std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> op_attr, AST::AstNodeType op,
							Location loc)
		{
			auto one = AstNode::mkconst_int(loc, 1, true);
			auto rhs = std::make_unique<AstNode>(loc, op, lhs->clone(), std::move(one));
			if (op_attr)
				append_attr(rhs.get(), std::move(op_attr));
			auto stmt_owned = std::make_unique<AstNode>(loc, AST_ASSIGN_EQ, std::move(lhs), std::move(rhs));
			auto* stmt = stmt_owned.get();
			// transparent frames (a function call under construction)
			// are skipped so the side-effect statement lands in the
			// enclosing statement container rather than the call's
			// argument list
			AstNode *parent = ast_stack.back();
			for (auto it = ast_stack.rbegin(); it != ast_stack.rend(); ++it) {
				if ((*it)->type == AST_FCALL || (*it)->type == AST_TCALL)
					continue;
				parent = *it;
				break;
			}
			parent->children.push_back(std::move(stmt_owned));
			if (stmt_attr)
				append_attr(stmt, std::move(stmt_attr));
			return stmt;
		}

		// create a pre/post-increment/decrement expression, and add the corresponding statement
		std::unique_ptr<AstNode> ParseState::addIncOrDecExpr(std::unique_ptr<AstNode> lhs,
								     std::unique_ptr<dict<IdString,
								     std::unique_ptr<AstNode>>> attr,
								     AST::AstNodeType op, Location loc, bool undo, bool sv_mode)
		{
			ensureAsgnExprAllowed(loc, sv_mode);
			const AstNode *stmt = addIncOrDecStmt(nullptr, std::move(lhs), std::move(attr), op, loc);
			log_assert(stmt->type == AST_ASSIGN_EQ);
			auto expr = stmt->children[0]->clone();
			if (undo) {
				auto one = AstNode::mkconst_int(loc, 1, false, 1);
				auto minus_one = std::make_unique<AstNode>(loc, AST_NEG, std::move(one));
				expr = std::make_unique<AstNode>(loc, op, std::move(expr), std::move(minus_one));
			}
			return expr;
		}

		// add a binary operator assignment statement, e.g., a += b
		std::unique_ptr<AstNode> ParseState::addAsgnBinopStmt(std::unique_ptr<dict<IdString, std::unique_ptr<AstNode>>> attr,
								      std::unique_ptr<AstNode> eq_lhs, AST::AstNodeType op, std::unique_ptr<AstNode> rhs)
		{
			Location loc = location_range(eq_lhs->location, rhs->location);
			if (op == AST_SHIFT_LEFT || op == AST_SHIFT_RIGHT ||
				op == AST_SHIFT_SLEFT || op == AST_SHIFT_SRIGHT) {
				rhs = std::make_unique<AstNode>(rhs->location, AST_TO_UNSIGNED, std::move(rhs));
			}
			auto binop_lhs = eq_lhs->clone();
			auto eq_rhs_owned = std::make_unique<AstNode>(loc, op, std::move(binop_lhs), std::move(rhs));
			auto ret_lhs = eq_lhs->clone();
			auto stmt_owned = std::make_unique<AstNode>(loc, AST_ASSIGN_EQ, std::move(eq_lhs), std::move(eq_rhs_owned));
			auto* stmt = stmt_owned.get();
			ast_stack.back()->children.push_back(std::move(stmt_owned));
			if (attr)
				append_attr(stmt, std::move(attr));
			return ret_lhs;
		}
	};
	YOSYS_NAMESPACE_END

	static std::unique_ptr<AstNode> mk_sva(Location loc, const char *op)
	{
		auto n = std::make_unique<AstNode>(loc, AST_SVA);
		n->str = op;
		return n;
	}

	// Detach a begin/end statement block that just registered itself as the
	// last child of the enclosing context, so it can be handed to an
	// assertion as its action instead of leaking into the module.
	static std::unique_ptr<AstNode> sva_action_detach_last(ParseState *extra)
	{
		auto &siblings = extra->ast_stack.back()->children;
		if (siblings.empty())
			return nullptr;
		auto owned = std::move(siblings.back());
		siblings.pop_back();
		return owned;
	}

	static std::unique_ptr<AstNode> sva_add(std::unique_ptr<AstNode> n, std::unique_ptr<AstNode> child)
	{
		if (child)
			n->children.push_back(std::move(child));
		return n;
	}

	static std::unique_ptr<AstNode> sva_bin(Location loc, const char *op, std::unique_ptr<AstNode> a, std::unique_ptr<AstNode> b)
	{
		auto n = mk_sva(loc, op);
		n->children.push_back(std::move(a));
		n->children.push_back(std::move(b));
		return n;
	}

	static std::unique_ptr<AstNode> sva_num(const parser::location_type &loc, const std::string &text, const std::vector<char> &case_stack)
	{
		ConstParser p{loc};
		auto n = p.const2ast(text, case_stack.empty() ? 0 : case_stack.back(), true);
		if (!n)
			log_error("Value conversion failed: `%s'\n", text.c_str());
		SET_AST_NODE_LOC(n.get(), loc, loc);
		return n;
	}

	static void sva_local_wire(ParseState *extra, const parser::location_type &loc, const std::string &name)
	{
		std::string mangled = name + stringf("$sva_local$%d", extra->sva_local_uid++);
		extra->sva_local_renames[name] = mangled;
		auto wire = std::make_unique<AstNode>(loc, AST_WIRE);
		wire->str = mangled;
		wire->is_logic = true;
		wire->is_reg = true;
		if (extra->sva_local_kind == 2) {
			extra->addWiretypeNode(&extra->sva_local_tname, wire.get());
		} else if (extra->sva_local_kind == 1) {
			wire->is_signed = true;
			wire->range_valid = true;
			wire->range_left = 31;
			wire->range_right = 0;
			wire->children.push_back(makeRange(loc, 31, 0, true));
		} else if (extra->sva_local_range) {
			wire->children.push_back(extra->sva_local_range->clone());
		}
		extra->saveChild(std::move(wire));
	}

	static void sva_rename_locals(ParseState *extra, AstNode *n)
	{
		if (!n || extra->sva_local_renames.empty())
			return;
		if (n->type == AST_IDENTIFIER) {
			auto it = extra->sva_local_renames.find(n->str);
			if (it != extra->sva_local_renames.end())
				n->str = it->second;
		}
		for (auto &c : n->children)
			sva_rename_locals(extra, c.get());
	}

	static void sva_formal_wire(ParseState *extra, const parser::location_type &loc, const std::string &name)
	{
		AstNode *scope = extra->current_ast_mod ? extra->current_ast_mod : extra->current_ast;
		if (scope) {
			for (auto &child : scope->children) {
				if (!child || child->str != name)
					continue;
				if (child->type == AST_WIRE || child->type == AST_MEMORY ||
						child->type == AST_PARAMETER || child->type == AST_LOCALPARAM) {
					if (extra->collecting_let_ports)
						extra->let_port_names.push_back(name);
					return;
				}
			}
		}
		if (extra->collecting_let_ports)
			extra->let_port_names.push_back(name);
		auto wire = std::make_unique<AstNode>(loc, AST_WIRE);
		wire->str = name;
		wire->is_logic = true;
		wire->is_reg = true;
		wire->is_signed = true;
		wire->range_valid = true;
		wire->range_left = 31;
		wire->range_right = 0;
		wire->children.push_back(makeRange(loc, 31, 0, true));
		extra->saveChild(std::move(wire));
	}

	// IEEE 1800-2009 17.2: a checker body holds assertion, declaration, and
	// generate items only. The body is parsed via module_body, so items that
	// are legal in modules but not in checkers are rejected here with a
	// pointed message instead of being silently mis-elaborated. Checker
	// instantiations are tagged so that simplify can tell an unresolved
	// checker name apart from an illegal module instance.
	// An AST_INITIAL whose block holds only assignment statements is a
	// variable initializer from `logic x = e;`, not procedural modeling
	// code; those elaborate like a module-level initial value.
	static bool checker_is_var_init(AstNode *n)
	{
		if (n->type != AST_INITIAL || n->children.size() != 1 ||
				n->children[0]->type != AST_BLOCK)
			return false;
		for (auto &stmt : n->children[0]->children)
			if (stmt->type != AST_ASSIGN_LE && stmt->type != AST_ASSIGN_EQ)
				return false;
		return true;
	}

	static void checker_body_check(AstNode *node)
	{
		for (auto &child : node->children) {
			AstNode *n = child.get();
			if (!n)
				continue;
			switch (n->type) {
			case AST_ALWAYS:
				err_at_loc(n->location, "Procedural modeling code (always/initial/final) in a checker is not supported.");
				break;
			case AST_INITIAL:
				if (checker_is_var_init(n))
					break;
				err_at_loc(n->location, "Procedural modeling code (always/initial/final) in a checker is not supported.");
				break;
			case AST_ASSIGN:
				err_at_loc(n->location, "Continuous assignments in a checker are not supported.");
				break;
			case AST_PRIMITIVE:
				err_at_loc(n->location, "Primitive instances in a checker are not supported.");
				break;
			case AST_DEFPARAM:
				err_at_loc(n->location, "defparam in a checker is not supported.");
				break;
			case AST_TASK:
				err_at_loc(n->location, "Task declarations in a checker are not supported (functions are allowed).");
				break;
			case AST_CLASS:
				err_at_loc(n->location, "Class declarations in a checker are not supported.");
				break;
			case AST_CELLARRAY:
				err_at_loc(n->location, "Checker instance arrays are not supported.");
				break;
			case AST_CELL:
				// module instantiations inside checkers are rejected in
				// simplify once the name turns out not to be a checker
				n->set_attribute(ID(sv_in_checker), AstNode::mkconst_int(n->location, 1, false));
				break;
			case AST_GENBLOCK:
			case AST_GENIF:
			case AST_GENFOR:
			case AST_GENCASE:
				checker_body_check(n);
				break;
			default:
				break;
			}
		}
	}

	void frontend_verilog_yy::parser::error(const frontend_verilog_yy::parser::location_type& loc, const std::string& msg)
	{
		err_at_loc(loc, "%s", msg);
	}
}

%code requires {
	#include <map>
	#include <string>
	#include <memory>
	#include "frontends/verilog/verilog_frontend.h"

	struct specify_target {
		char polarity_op;
		std::unique_ptr<YOSYS_NAMESPACE_PREFIX AST::AstNode> dst, dat;
		specify_target& operator=(specify_target&& other) noexcept {
			if (this != &other) {
				dst = std::move(other.dst);
				dat = std::move(other.dat);
				polarity_op = other.polarity_op;
			}
			return *this;
		}
	};

	struct specify_triple {
		std::unique_ptr<YOSYS_NAMESPACE_PREFIX AST::AstNode> t_min, t_avg, t_max;
		specify_triple& operator=(specify_triple&& other) noexcept {
			if (this != &other) {
				t_min = std::move(other.t_min);
				t_avg = std::move(other.t_avg);
				t_max = std::move(other.t_max);
			}
			return *this;
		}
	};

	struct specify_rise_fall {
		specify_triple rise;
		specify_triple fall;
	};

	using string_t = std::unique_ptr<std::string>;
	using ast_t = std::unique_ptr<YOSYS_NAMESPACE_PREFIX AST::AstNode>;
	using al_t = std::unique_ptr<YOSYS_NAMESPACE_PREFIX dict<YOSYS_NAMESPACE_PREFIX RTLIL::IdString, std::unique_ptr<YOSYS_NAMESPACE_PREFIX AST::AstNode>>>;
	using specify_target_ptr_t = std::unique_ptr<struct specify_target>;
	using specify_triple_ptr_t = std::unique_ptr<struct specify_triple>;
	using specify_rise_fall_ptr_t = std::unique_ptr<struct specify_rise_fall>;
	using boolean_t = bool;
	using ch_t = char;
	using integer_t = int;
	using ast_node_type_t = YOSYS_NAMESPACE_PREFIX AST::AstNodeType;
}

%token <string_t> string_t "string"
%token <ast_t> ast_t
%token <al_t> al_t
%token <specify_target_ptr_t> specify_target_ptr_t "specify target"
%token <specify_triple_ptr_t> specify_triple_ptr_t "specify triple"
%token <specify_rise_fall_ptr_t> specify_rise_fall_ptr_t "specify rise and fall"
%token <boolean_t> boolean_t "boolean"
%token <ch_t> ch_t "invalid token"
%token <integer_t> integer_t "integer"
%token <ast_node_type_t> ast_node_type_t

%token <string_t> TOK_STRING TOK_ID TOK_CONSTVAL TOK_REALVAL TOK_PRIMITIVE TOK_PULLUP
%token <string_t> TOK_SVA_LABEL TOK_SPECIFY_OPER TOK_MSG_TASKS
%token <string_t> TOK_BASE TOK_BASED_CONSTVAL TOK_UNBASED_UNSIZED_CONSTVAL
%token <string_t> TOK_USER_TYPE TOK_PKG_USER_TYPE TOK_SCOPED_ID
%token TOK_ASSERT TOK_ASSUME TOK_RESTRICT TOK_COVER TOK_FINAL
%token ATTR_BEGIN ATTR_END DEFATTR_BEGIN DEFATTR_END
%token TOK_MODULE TOK_ENDMODULE TOK_PARAMETER TOK_LOCALPARAM TOK_DEFPARAM
%token TOK_PROGRAM TOK_ENDPROGRAM TOK_GLOBAL
%token TOK_PACKAGE TOK_ENDPACKAGE TOK_PACKAGESEP
%token TOK_INTERFACE TOK_ENDINTERFACE TOK_MODPORT TOK_VAR TOK_WILDCARD_CONNECT
%token TOK_INPUT TOK_OUTPUT TOK_INOUT TOK_WIRE TOK_WAND TOK_WOR TOK_REG TOK_LOGIC
%token TOK_INTEGER TOK_SIGNED TOK_ASSIGN TOK_ALIAS TOK_ALWAYS TOK_INITIAL
%token TOK_ALWAYS_FF TOK_ALWAYS_COMB TOK_ALWAYS_LATCH
%token TOK_BEGIN TOK_END TOK_IF TOK_ELSE TOK_IFNONE TOK_FOR TOK_WHILE TOK_REPEAT TOK_DO TOK_BREAK TOK_CONTINUE
%token TOK_DPI_FUNCTION TOK_EXPORT TOK_BIND TOK_RETURN TOK_STATIC TOK_FORK TOK_JOIN TOK_JOIN_ANY TOK_JOIN_NONE TOK_FOREACH TOK_FOREVER TOK_TYPE TOK_ARROW TOK_POSEDGE TOK_NEGEDGE TOK_OR TOK_AUTOMATIC TOK_WAIT TOK_FORCE TOK_RELEASE TOK_DEASSIGN TOK_REF TOK_CONST_REF TOK_NB_ARROW
%token TOK_CASE TOK_CASEX TOK_CASEZ TOK_ENDCASE TOK_DEFAULT
%token TOK_FUNCTION TOK_ENDFUNCTION TOK_TASK TOK_ENDTASK TOK_SPECIFY
%token TOK_IGNORED_SPECIFY TOK_ENDSPECIFY TOK_SPECPARAM TOK_SPECIFY_AND TOK_IGNORED_SPECIFY_AND
%token TOK_GENERATE TOK_ENDGENERATE TOK_GENVAR TOK_REAL
%token TOK_SYNOPSYS_FULL_CASE TOK_SYNOPSYS_PARALLEL_CASE
%token TOK_SUPPLY0 TOK_SUPPLY1 TOK_TO_SIGNED TOK_TO_UNSIGNED
%token TOK_POS_INDEXED TOK_NEG_INDEXED TOK_PROPERTY TOK_ENUM TOK_TYPEDEF
%token TOK_RAND TOK_CONST TOK_CHECKER TOK_ENDCHECKER TOK_EVENTUALLY TOK_S_EVENTUALLY
%token TOK_SEQUENCE TOK_ENDSEQUENCE TOK_ENDPROPERTY TOK_INTERSECT TOK_THROUGHOUT TOK_WITHIN
%token TOK_FIRST_MATCH TOK_UNTIL TOK_UNTIL_WITH TOK_S_UNTIL TOK_S_UNTIL_WITH TOK_IMPLIES TOK_IFF
%token TOK_NEXTTIME TOK_S_NEXTTIME TOK_S_ALWAYS TOK_ACCEPT_ON TOK_REJECT_ON
%token TOK_SYNC_ACCEPT_ON TOK_SYNC_REJECT_ON TOK_STRONG TOK_WEAK TOK_DISABLE
%token TOK_CLOCKING TOK_ENDCLOCKING TOK_UNTYPED TOK_LET TOK_EXPECT TOK_INSIDE TOK_MATCHES TOK_WITH TOK_WITH_SLICE TOK_TIME TOK_TIMEUNIT TOK_TIMEPRECISION TOK_RANDCASE TOK_RANDSEQUENCE TOK_SOFT TOK_DIST
%token TOK_CLASS TOK_ENDCLASS TOK_EXTENDS TOK_IMPLEMENTS TOK_VIRTUAL TOK_PURE TOK_LOCAL TOK_PROTECTED TOK_EXTERN TOK_MAILBOX TOK_SEMAPHORE TOK_RANDC
%token TOK_SVA_OV_IMPL TOK_SVA_NONOV_IMPL TOK_SVA_OV_FOL TOK_SVA_NONOV_FOL TOK_SVA_HASH2
%token TOK_SVA_REP_CONSEC TOK_SVA_REP_PLUS TOK_SVA_REP_GOTO TOK_SVA_REP_NONCONS
%token TOK_INCREMENT TOK_DECREMENT TOK_UNIQUE TOK_UNIQUE0 TOK_PRIORITY
%token TOK_STRUCT TOK_PACKED TOK_UNSIGNED TOK_INT TOK_BYTE TOK_SHORTINT TOK_LONGINT TOK_VOID TOK_UNION TOK_TAGGED
%token TOK_TRI TOK_TRI0 TOK_TRI1 TOK_TRIAND TOK_TRIOR TOK_TRIREG TOK_UWIRE TOK_VECTORED TOK_SCALARED TOK_NETTYPE
%token TOK_EDGE
%token TOK_BIT_OR_ASSIGN TOK_BIT_AND_ASSIGN TOK_BIT_XOR_ASSIGN TOK_ADD_ASSIGN
%token TOK_SUB_ASSIGN TOK_DIV_ASSIGN TOK_MOD_ASSIGN TOK_MUL_ASSIGN
%token TOK_SHL_ASSIGN TOK_SHR_ASSIGN TOK_SSHL_ASSIGN TOK_SSHR_ASSIGN
%token TOK_TIME_SCALE
%token TOK_IMPORT

%token TOK_EXCL "'!'"
%token TOK_HASH "'#'"
%token TOK_PERC "'%'"
%token TOK_AMP "'&'"
%token TOK_LPAREN "'('"
%token TOK_RPAREN "')'"
%token TOK_ASTER "'*'"
%token TOK_PLUS "'+'"
%token TOK_COMMA "','"
%token TOK_MINUS "'-'"
%token TOK_DOT "'.'"
%token TOK_SLASH "'/'"
%token TOK_COL "':'"
%token TOK_SEMICOL "';'"
%token TOK_LT "'<'"
%token TOK_EQ "'='"
%token TOK_GT "'>'"
%token TOK_QUE "'?'"
%token TOK_AT "'@'"
%token TOK_LBRA "'['"
%token TOK_RBRA "']'"
%token TOK_CARET "'^'"
%token TOK_UNDER "'_'"
%token TOK_LCURL "'{'"
%token TOK_PIPE "'|'"
%token TOK_RCURL "'}'"
%token TOK_TILDE "'~'"
%token TOK_n "'n'"
%token TOK_p "'p'"
%token TOK_x "'x'"
%token TOK_z "'z'"

%type <string_t> module_name_id
%type <ast_t> range range_or_multirange non_opt_range non_opt_multirange param_unpacked param_packed wire_name_dims timing_event timing_events
%type <ast_t> wire_type expr basic_expr concat_list assignment_pattern_list member_pattern_list rvalue lvalue lvalue_concat_list non_io_wire_type io_wire_type alias_lvalues match_filter type_operand opt_type_cast
%type <string_t> sva_seq_method opt_label opt_sva_label tok_prim_wrapper hierarchical_id hierarchical_type_id integral_number
%type <string_t> type_name bind_name static_method_name scoped_type_name foreach_dotted_opt class_name rs_ident
%type <ast_t> opt_enum_init enum_type struct_type enum_struct_type func_return_type typedef_base_type
%type <ast_t> param_type_rhs
%type <ast_t> opt_chained_method chained_method rvalue_call_stmt call_args_tail
%type <boolean_t> opt_property always_comb_or_latch always_or_always_ff
%type <boolean_t> opt_signedness_default_signed opt_signedness_default_unsigned
%type <integer_t> integer_atom_type integer_vector_type
%type <al_t> attr if_attr case_attr
%type <ast_t> struct_union
%type <ast_node_type_t> asgn_binop inc_or_dec_op
%type <ast_t> genvar_identifier port_unpacked
%type <ch_t> sva_dir
%type <ast_t> sva_property_spec sva_at_clock sva_clock_opt sva_dis_opt sva_event sva_event_item sva_prop sva_impl sva_live sva_live_op
%type <ast_t> sva_until sva_iff sva_or sva_and sva_not sva_intersect sva_within sva_throughout
%type <ast_t> sva_concat sva_delay_tail sva_repeat sva_bool sva_bexpr sva_primary sva_cycle sva_bounds sva_bounds_tail sva_msg sva_msg_args sva_action
%type <ast_t> sva_case_items sva_matches sva_inside_items
%type <ast_t> checker_port_default

%type <specify_target_ptr_t> specify_target
%type <specify_triple_ptr_t> specify_triple specify_opt_triple
%type <specify_rise_fall_ptr_t> specify_rise_fall
%type <ast_t> specify_if specify_condition
%type <ch_t> specify_edge

// operator precedence from low to high
%left TOK_RBRA
%left TOK_ARROW TOK_IFF
%left OP_LOR
%left OP_LAND
%left TOK_PIPE OP_NOR
%left TOK_CARET OP_XNOR
%left TOK_AMP OP_NAND
%left OP_EQ OP_NE OP_EQX OP_NEX OP_WEQ OP_WNE TOK_INSIDE TOK_MATCHES
%left TOK_LT OP_LE OP_GE TOK_GT
%left OP_SHL OP_SHR OP_SSHL OP_SSHR
%left TOK_PLUS TOK_MINUS
%left TOK_ASTER TOK_SLASH TOK_PERC
%left OP_POW
%precedence UNARY_OPS
%precedence OP_CAST

%define parse.error detailed
%define parse.lac full
// State 261 shifts `<->' (TOK_IFF) rather than ending the expression.
// Class-member conflicts all resolve by shift, which is the intended read:
// qualifier tokens continue class_member_quals, ATTR_BEGIN starts a member
// attribute group, TOK_FUNCTION/TOK_TASK select class_method_body over the
// empty attr prefix of task_func_decl, TOK_EXTERN/TOK_PURE pick a
// proto_quals alternative that yields the same nonterminal either way,
// TOK_CLASS after class_member_quals starts a nested class, and a
// parameterized return type shifts the type name rather than reducing an
// empty opt_automatic (both derivations are identical).
// State shifts "'{'" after `with (expr)' (the randomize inline-constraint
// block) rather than reducing `with (expr)' alone; shifts `type ( expr )'
// onto the optional-cast production; and shifts `-'/`+'/`->'/`=' onto the
// SVA repetition tokens (an `[*]'/`[+]'/`[->'/`[=]' bracket followed by an
// expression is a repetition, by `]' an unbounded one, both kept over the
// associative-dimension reading).
// +9 (2026-10-21, sva_live_op after nexttime / not): the operand of a
// prefixed always / eventually extends as far right as possible (shift),
// the same reading `always a until b` already had
%expect 140

%precedence FAKE_THEN
%precedence TOK_ELSE

%debug
%locations

%%

input: {
	(void)yynerrs_;
	extra->ast_stack.clear();
	extra->ast_stack.push_back(extra->current_ast);
} design {
	extra->ast_stack.pop_back();
	log_assert(GetSize(extra->ast_stack) == 0);
	extra->default_attr_list.clear();
};

design:
	module design |
	program design |
	defattr design |
	task_func_decl design |
	TOK_VIRTUAL task_func_decl design {
		// DV class fragments are sometimes read standalone, where a
		// `virtual task/function ... endtask' method appears at top level.
		// The qualifier is meaningless outside a class; drop it.
	} |
	TOK_VIRTUAL nested_class_decl design {
		// `virtual class ...' at compilation-unit scope: the qualifier is
		// meaningless outside an inheritance hierarchy; drop it.
	} |
	nested_class_decl design |
	param_decl design |
	localparam_decl design |
	typedef_decl design |
	package design |
	import_stmt design |
	interface design |
	bind_stmt design |
	wire_decl design |
	assign_stmt design |
	always_stmt design |
	assert design |
	cell_stmt design |
	struct_decl design |
	enum_decl design |
	%empty;

attr:
	{
		if (extra->attr_list)
			extra->attr_list_stack.push(std::move(extra->attr_list));
		extra->attr_list = std::make_unique<dict<IdString, std::unique_ptr<AstNode>>>();
		for (auto &it : extra->default_attr_list)
			(*extra->attr_list)[it.first] = it.second->clone();
	} attr_opt {
		$$ = std::move(extra->attr_list);
		if (!extra->attr_list_stack.empty()) {
			extra->attr_list = std::move(extra->attr_list_stack.top());
			extra->attr_list_stack.pop();
		} else
			extra->attr_list = nullptr;
	};

attr_opt:
	attr_opt ATTR_BEGIN opt_attr_list ATTR_END {
		SET_RULE_LOC(@$, @2, @$);
	}|
	%empty;

defattr:
	DEFATTR_BEGIN {
		if (extra->attr_list != nullptr)
			extra->attr_list_stack.push(std::move(extra->attr_list));
		extra->attr_list = std::make_unique<dict<IdString, std::unique_ptr<AstNode>>>();
		extra->default_attr_list.clear();
	} opt_attr_list {
		extra->attr_list->swap(extra->default_attr_list);
		extra->attr_list.reset();
		if (!extra->attr_list_stack.empty()) {
			extra->attr_list = std::move(extra->attr_list_stack.top());
			extra->attr_list_stack.pop();
		} else
			extra->attr_list = nullptr;
	} DEFATTR_END;

opt_attr_list:
	attr_list | %empty;

attr_list:
	attr_assign |
	attr_list TOK_COMMA attr_assign;

attr_assign:
	hierarchical_id {
		(*extra->attr_list)[*$1] = AstNode::mkconst_int(@1, 1, false);
	} |
	hierarchical_id TOK_EQ expr {
		(*extra->attr_list)[*$1] = std::move($3);
	};

hierarchical_id:
	TOK_ID {
		$$ = std::move($1);
	} |
	hierarchical_id TOK_PACKAGESEP TOK_ID {
		if ($3->compare(0, 1, "\\") == 0)
			*$1 += "::" + $3->substr(1);
		else
			*$1 += "::" + *$3;
		$$ = std::move($1);
	} |
	hierarchical_id TOK_DOT TOK_ID {
		if ($3->compare(0, 1, "\\") == 0)
			*$1 += "." + $3->substr(1);
		else
			*$1 += "." + *$3;
		$$ = std::move($1);
	} |
	TOK_SCOPED_ID {
		$$ = std::move($1);
	};

hierarchical_type_id:
	TOK_USER_TYPE {$$ = std::move($1); }
	| TOK_PKG_USER_TYPE {$$ = std::move($1); } // package qualified type name
	| TOK_LPAREN TOK_USER_TYPE TOK_RPAREN	{ $$ = std::move($2); }		// non-standard grammar
	;

// `ref` is a contextual keyword (tf-port direction); a module may be
// named `ref`, so accept the token as a plain identifier here.
module_name_id:
	TOK_ID { $$ = std::move($1); } |
	TOK_REF { $$ = std::make_unique<std::string>("\\ref"); };

module:
	attr TOK_MODULE {
		extra->enterTypeScope();
	} module_name_id {
		extra->do_not_require_port_stubs = false;
		AstNode* mod = extra->pushChild(std::make_unique<AstNode>(@$, AST_MODULE));
		extra->current_ast_mod = mod;
		extra->port_stubs.clear();
		extra->port_counter = 0;
		// default / global clocking are per module (IEEE 14.12, 14.14): a
		// default clocking of an earlier module used to clock this module's
		// unclocked assertions (an undriven implicit clock net)
		extra->sva_default_clock.reset();
		extra->sva_global_clock.reset();
		mod->str = *$4;
		append_attr(mod, std::move($1));
	} module_header_imports module_para_opt module_args_opt TOK_SEMICOL module_body TOK_ENDMODULE opt_label opt_semi {
		if (extra->port_stubs.size() != 0)
			err_at_loc(@9, "Missing details for module port `%s'.",
					extra->port_stubs.begin()->first.c_str());
		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @$);
		extra->ast_stack.pop_back();
		log_assert(extra->ast_stack.size() == 1);
		checkLabelsMatch(@12, "Module name", $4.get(), $12.get());
		extra->current_ast_mod = nullptr;
		extra->exitTypeScope();
	};

program:
	attr TOK_PROGRAM {
		extra->enterTypeScope();
	} TOK_ID {
		extra->do_not_require_port_stubs = false;
		// A program (IEEE 1800 24.3) is an aggregation of threads with
		// module-like port semantics; for elaboration it is modeled as a
		// module so instances hook up their ports the same way. The
		// reactive-region scheduling itself is simulation-only and not
		// modeled.
		AstNode* mod = extra->pushChild(std::make_unique<AstNode>(@$, AST_MODULE));
		extra->current_ast_mod = mod;
		extra->port_stubs.clear();
		extra->port_counter = 0;
		// default / global clocking are per module (IEEE 14.12, 14.14): a
		// default clocking of an earlier module used to clock this module's
		// unclocked assertions (an undriven implicit clock net)
		extra->sva_default_clock.reset();
		extra->sva_global_clock.reset();
		mod->str = *$4;
		append_attr(mod, std::move($1));
	} module_header_imports module_para_opt module_args_opt TOK_SEMICOL module_body TOK_ENDPROGRAM opt_label opt_semi {
		if (extra->port_stubs.size() != 0)
			err_at_loc(@9, "Missing details for program port `%s'.",
					extra->port_stubs.begin()->first.c_str());
		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @$);
		extra->ast_stack.pop_back();
		log_assert(extra->ast_stack.size() == 1);
		checkLabelsMatch(@12, "Program name", $4.get(), $12.get());
		extra->current_ast_mod = nullptr;
		extra->exitTypeScope();
	};

port_unpacked:
	%empty { $$ = nullptr; } |
	non_opt_range { $$ = std::move($1); } |
	non_opt_multirange { $$ = std::move($1); };

module_para_opt:
	TOK_HASH TOK_LPAREN module_para_list TOK_RPAREN | %empty;

module_para_list:
	single_module_para | module_para_list TOK_COMMA single_module_para;

single_module_para:
	%empty |
	attr TOK_PARAMETER {
		extra->astbuf1 = std::make_unique<AstNode>(@$, AST_PARAMETER);
		extra->astbuf1->children.push_back(AstNode::mkconst_int(@2, 0, true));
		append_attr(extra->astbuf1.get(), std::move($1));
	} param_type single_param_decl |
	attr TOK_LOCALPARAM {
		extra->astbuf1 = std::make_unique<AstNode>(@$, AST_LOCALPARAM);
		extra->astbuf1->children.push_back(AstNode::mkconst_int(@2, 0, true));
		append_attr(extra->astbuf1.get(), std::move($1));
	} param_type single_param_decl |
	attr TOK_PARAMETER TOK_TYPE TOK_ID TOK_EQ typedef_base_type range_or_multirange {
		auto base = std::move($6);
		if ($7) {
			auto range = checkRange(base.get(), std::move($7));
			if (range)
				base->children.push_back(std::move(range));
		}
		extra->addTypeParamNode($4.get(), std::move(base));
	} |
	attr TOK_PARAMETER TOK_TYPE TOK_ID TOK_EQ TOK_SCOPED_ID {
		auto base = std::make_unique<AstNode>(@6, AST_WIRE);
		base->is_logic = true;
		extra->addWiretypeNode($6.get(), base.get());
		extra->addTypeParamNode($4.get(), std::move(base));
	} |
	attr TOK_PARAMETER TOK_TYPE TOK_ID TOK_EQ enum_struct_type {
		extra->addTypeParamNode($4.get(), std::move($6));
	} |
	attr TOK_PARAMETER TOK_TYPE TOK_ID TOK_EQ TOK_TYPE TOK_LPAREN type_operand TOK_RPAREN {
		// `parameter type T = type(...)' (IEEE 1800-2009 6.23): the type
		// operator yields a type, registered as an opaque alias
		auto base = std::make_unique<AstNode>(@$, AST_WIRE);
		base->is_logic = true;
		extra->addTypeParamNode($4.get(), std::move(base));
	} |
	attr TOK_LOCALPARAM TOK_TYPE TOK_ID TOK_EQ enum_struct_type {
		extra->addTypedefNode($4.get(), std::move($6));
	} |
	attr TOK_TYPE TOK_ID TOK_EQ typedef_base_type range_or_multirange {
		// `#(type T = data_type)' without the parameter keyword
		// (IEEE A.1.3 parameter_port_declaration)
		auto base = std::move($5);
		if ($6) {
			auto range = checkRange(base.get(), std::move($6));
			if (range)
				base->children.push_back(std::move(range));
		}
		extra->addTypeParamNode($3.get(), std::move(base));
	} |
	attr TOK_LOCALPARAM TOK_TYPE TOK_ID TOK_EQ typedef_base_type range_or_multirange {
		auto base = std::move($6);
		if ($7) {
			auto range = checkRange(base.get(), std::move($7));
			if (range)
				base->children.push_back(std::move(range));
		}
		extra->addTypedefNode($4.get(), std::move(base));
	} |
	attr TOK_USER_TYPE TOK_ID TOK_EQ expr {
		auto node = std::make_unique<AstNode>(@$, AST_PARAMETER);
		node->str = *$3;
		node->children.push_back(std::move($5));
		extra->addWiretypeNode($2.get(), node.get());
		append_attr(node.get(), std::move($1));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	attr TOK_SCOPED_ID TOK_ID TOK_EQ expr {
		auto node = std::make_unique<AstNode>(@$, AST_PARAMETER);
		node->str = *$3;
		node->children.push_back(std::move($5));
		extra->addWiretypeNode($2.get(), node.get());
		append_attr(node.get(), std::move($1));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	integer_atom_type TOK_UNSIGNED TOK_ID TOK_EQ expr {
		auto node = std::make_unique<AstNode>(@$, AST_PARAMETER);
		node->str = *$3;
		node->is_signed = false;
		node->range_left = $1 - 1;
		node->range_right = 0;
		node->children.push_back(std::move($5));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	integer_atom_type TOK_SIGNED TOK_ID TOK_EQ expr {
		auto node = std::make_unique<AstNode>(@$, AST_PARAMETER);
		node->str = *$3;
		node->is_signed = true;
		node->range_left = $1 - 1;
		node->range_right = 0;
		node->children.push_back(std::move($5));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	integer_atom_type TOK_ID TOK_EQ expr {
		auto node = std::make_unique<AstNode>(@$, AST_PARAMETER);
		node->str = *$2;
		node->is_signed = true;
		node->range_left = $1 - 1;
		node->range_right = 0;
		node->children.push_back(std::move($4));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	single_param_decl;

module_args_opt:
	TOK_LPAREN TOK_RPAREN | %empty | TOK_LPAREN module_args optional_comma TOK_RPAREN;

module_args:
	module_arg | module_args TOK_COMMA module_arg;

optional_comma:
	TOK_COMMA | %empty;

module_arg_opt_assignment:
	TOK_EQ expr {
		if (extra->ast_stack.back()->children.size() > 0 && extra->ast_stack.back()->children.back()->type == AST_WIRE) {
			if (extra->ast_stack.back()->children.back()->is_input) {
				auto& n = extra->ast_stack.back()->children.back();
				n->attributes[ID::defaultvalue] = std::move($2);
			} else {
				auto wire = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
				wire->str = extra->ast_stack.back()->children.back()->str;
				if (extra->ast_stack.back()->children.back()->is_reg || extra->ast_stack.back()->children.back()->is_logic)
					extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_INITIAL, std::make_unique<AstNode>(@$, AST_BLOCK, std::make_unique<AstNode>(@$, AST_ASSIGN_LE, std::move(wire), std::move($2)))));
				else
					extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_ASSIGN, std::move(wire), std::move($2)));
			}
		} else
			err_at_loc(@2, "SystemVerilog interface in module port list cannot have a default value.");
	} |
	%empty;

module_arg:
	TOK_ID {
		if (extra->ast_stack.back()->children.size() > 0 && extra->ast_stack.back()->children.back()->type == AST_WIRE) {
			auto node = extra->ast_stack.back()->children.back()->clone();
			node->str = *$1;
			node->port_id = ++extra->port_counter;
			SET_AST_NODE_LOC(node.get(), @1, @1);
			extra->ast_stack.back()->children.push_back(std::move(node));
		} else {
			if (extra->port_stubs.count(*$1) != 0)
				err_at_loc(@1, "Duplicate module port `%s'.", *$1);
			extra->port_stubs[*$1] = ++extra->port_counter;
		}
	} module_arg_opt_assignment |
	TOK_ID {
		extra->astbuf1 = std::make_unique<AstNode>(@$, AST_INTERFACEPORT);
		extra->astbuf1->children.push_back(std::make_unique<AstNode>(@$, AST_INTERFACEPORTTYPE));
		extra->astbuf1->children[0]->str = *$1;
	} TOK_ID {  /* SV interfaces */
		if (!mode->sv)
			err_at_loc(@3, "Interface found in port list (%s). This is not supported unless read_verilog is called with -sv!", *$3);
		extra->astbuf2 = extra->astbuf1->clone(); // really only needed if multiple instances of same type.
		extra->astbuf2->str = *$3;
		extra->astbuf2->port_id = ++extra->port_counter;
	} port_unpacked {
		if ($5 != nullptr)
			extra->astbuf2->children.push_back(std::move($5));
		extra->ast_stack.back()->children.push_back(std::move(extra->astbuf2));
	} module_arg_opt_assignment |
	attr wire_type range_or_multirange TOK_ID port_unpacked {
		auto node = std::move($2);
		node->str = *$4;
		SET_AST_NODE_LOC(node.get(), @4, @4);
		node->port_id = ++extra->port_counter;
		std::string iface_type;
		for (auto &ch : node->children) {
			if (ch && ch->type == AST_WIRETYPE)
				iface_type = ch->str;
		}
		// A type name that is not declared anywhere in this compilation
		// unit (no typedef on the user-type stack, no package type) on a
		// DIRECTIONLESS port is an interface port whose interface lives in
		// another file (upstream semantics; `hierarchy -libdir` loads it on
		// demand).  The lexer's typedef lookahead turns every `ident ident`
		// into a user type, so without this `module m (iface intf)` became
		// a plain wire and the interface connection was lost
		// (tests/svinterfaces load_and_derive, resolve_types).  Directed
		// ports keep the missing-package-typedef leniency.
		bool type_known = false;
		if (!iface_type.empty()) {
			for (auto &m : extra->user_type_stack)
				if (m.count(iface_type))
					type_known = true;
			if (extra->pkg_user_types.count(iface_type))
				type_known = true;
			// wildcard-imported package types are registered only under
			// their qualified name (\pkg::name); `import pkg::*` makes the
			// bare name a data type, not an interface
			if (!type_known && iface_type.size() > 1) {
				std::string bare = iface_type.substr(1);
				for (auto &kv : extra->pkg_user_types) {
					size_t pos = kv.first.rfind("::");
					if (pos != std::string::npos && kv.first.substr(pos + 2) == bare) {
						type_known = true;
						break;
					}
				}
			}
		}
		if (!iface_type.empty() && (extra->interface_names.count(iface_type) ||
				(!type_known && !node->is_input && !node->is_output))) {
			// IEEE 1800-2009 25.3: a port whose type is an interface is an interface port.
			node->type = AST_INTERFACEPORT;
			node->is_input = false;
			node->is_output = false;
			node->is_reg = false;
			node->is_logic = false;
			node->is_signed = false;
			node->is_custom_type = false;
			node->range_valid = false;
			node->children.clear();
			auto ty = std::make_unique<AstNode>(@4, AST_INTERFACEPORTTYPE);
			ty->str = iface_type;
			node->children.push_back(std::move(ty));
		}
		auto range = checkRange(node.get(), std::move($3));
		if (range != nullptr)
			node->children.push_back(std::move(range));
		if ($5 != nullptr) {
			// Unpacked array ports stay wires with an unpacked range child;
			// the shared array-access machinery flattens them. Rewriting the
			// port into an AST_MEMORY would drop its port-ness before RTLIL.
			// With a type name the unpacked range would sit where a packed
			// one goes; mark the port so type resolution reads it as an
			// unpacked declaration (`st_t p [2]` is 2 elements, not packed
			// [2:2]; `st_t [1:0] p [2]` keeps its packed dimension).
			if (!iface_type.empty() && node->type != AST_INTERFACEPORT)
				node->set_attribute(ID(sv_port_unpacked), AstNode::mkconst_int(@5, 1, false));
			node->children.push_back(std::move($5));
			node->fixup_hierarchy_flags();
		}
		if (node->type != AST_INTERFACEPORT && !node->is_input && !node->is_output) {
			// A later ANSI port inherits the previous direction.
			for (auto it = extra->ast_stack.back()->children.rbegin(); it != extra->ast_stack.back()->children.rend(); ++it) {
				if ((*it)->port_id != 0 && ((*it)->is_input || (*it)->is_output)) {
					node->is_input = (*it)->is_input;
					node->is_output = (*it)->is_output;
					break;
				}
			}
			if (!node->is_input && !node->is_output && mode->sv)
				node->is_input = true;
		}
		if (node->type != AST_INTERFACEPORT && !node->is_input && !node->is_output)
			err_at_loc(@4, "Module port `%s' is neither input nor output.", *$4);
		if (node->is_reg && node->is_input && !node->is_output && !mode->sv)
			err_at_loc(@4, "Input port `%s' is declared as register.", *$4);
		append_attr(node.get(), std::move($1));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} module_arg_opt_assignment |
	TOK_DOT TOK_DOT TOK_DOT {
		extra->do_not_require_port_stubs = true;
	};

package:
	attr TOK_PACKAGE {
		extra->enterTypeScope();
	} TOK_ID {
		AstNode* mod = extra->pushChild(std::make_unique<AstNode>(@$, AST_PACKAGE));
		extra->current_ast_mod = mod;
		mod->str = *$4;
		append_attr(mod, std::move($1));
	} TOK_SEMICOL package_body TOK_ENDPACKAGE opt_label {
		extra->ast_stack.pop_back();
		checkLabelsMatch(@9, "Package name", $4.get(), $9.get());
		extra->current_ast_mod = nullptr;
		extra->exitTypeScope();
	};

package_body:
	package_body package_body_stmt | %empty;

module_header_imports:
	%empty |
	module_header_imports import_stmt;

package_body_stmt:
	typedef_decl | localparam_decl | param_decl | task_func_decl | import_stmt |
	dpi_export_decl | sva_sequence_decl | sva_property_decl | sva_let_decl |
	wire_decl | class_decl | timeunit_decl;

// SystemVerilog class declarations (IEEE 1800-2017 8). Classes are
// simulation-only: the declaration is parsed into an AST_CLASS node whose
// children ride along, but elaboration treats the node as opaque.
//
// Member qualifiers (static/virtual/local/protected/rand/randc/const) and
// attribute groups are folded into a single class_member_quals loop so that
// a class item has exactly one empty-prefix position; this keeps the grammar
// LALR(1)-conflict-free. Qualifiers and member attributes are dropped (class
// bodies are never elaborated). Nested classes reuse class_member_quals for
// their `virtual` prefix, so nested_class_decl carries no prefix of its own.
class_name:
	TOK_ID { $$ = std::move($1); } |
	TOK_USER_TYPE { $$ = std::move($1); } ;

class_decl:
	attr TOK_CLASS class_name class_param_list_opt class_extends_opt class_implements_opt TOK_SEMICOL {
		auto node = std::make_unique<AstNode>(@$, AST_CLASS);
		node->str = *$3;
		append_attr(node.get(), std::move($1));
		extra->user_type_stack.back()[*$3] = node.get();
		extra->pushChild(std::move(node));
	} class_items TOK_ENDCLASS opt_label {
		extra->ast_stack.pop_back();
	} |
	attr TOK_VIRTUAL TOK_CLASS class_name class_param_list_opt class_extends_opt class_implements_opt TOK_SEMICOL {
		auto node = std::make_unique<AstNode>(@$, AST_CLASS);
		node->str = *$4;
		append_attr(node.get(), std::move($1));
		extra->user_type_stack.back()[*$4] = node.get();
		extra->pushChild(std::move(node));
	} class_items TOK_ENDCLASS opt_label {
		extra->ast_stack.pop_back();
	} |
	attr TOK_INTERFACE TOK_CLASS class_name class_param_list_opt class_extends_opt class_implements_opt TOK_SEMICOL {
		// interface class (IEEE 1800-2017 8.26): opaque AST_CLASS, like
		// the nested_class_decl variant above.
		auto node = std::make_unique<AstNode>(@$, AST_CLASS);
		node->str = *$4;
		node->set_attribute(ID(sv_interface_class), AstNode::mkconst_int(@$, 1, false));
		append_attr(node.get(), std::move($1));
		extra->user_type_stack.back()[*$4] = node.get();
		extra->pushChild(std::move(node));
	} class_items TOK_ENDCLASS opt_label {
		extra->ast_stack.pop_back();
	};

nested_class_decl:
	TOK_CLASS class_name class_param_list_opt class_extends_opt class_implements_opt TOK_SEMICOL {
		auto node = std::make_unique<AstNode>(@$, AST_CLASS);
		node->str = *$2;
		extra->user_type_stack.back()[*$2] = node.get();
		extra->pushChild(std::move(node));
	} class_items TOK_ENDCLASS opt_label {
		extra->ast_stack.pop_back();
	} |
	TOK_INTERFACE TOK_CLASS class_name class_param_list_opt class_extends_opt class_implements_opt TOK_SEMICOL {
		// interface class (IEEE 1800-2017 8.26): parsed as an opaque
		// AST_CLASS like regular classes; the methods inside are prototypes.
		auto node = std::make_unique<AstNode>(@$, AST_CLASS);
		node->str = *$3;
		node->set_attribute(ID(sv_interface_class), AstNode::mkconst_int(@$, 1, false));
		extra->user_type_stack.back()[*$3] = node.get();
		extra->pushChild(std::move(node));
	} class_items TOK_ENDCLASS opt_label {
		extra->ast_stack.pop_back();
	};

class_param_list_opt:
	%empty |
	TOK_HASH TOK_LPAREN class_params TOK_RPAREN;

class_params:
	class_param |
	class_params TOK_COMMA class_param;

// class parameter values are discarded: class bodies are never elaborated
class_param:
	TOK_PARAMETER TOK_ID class_param_default |
	TOK_PARAMETER TOK_TYPE TOK_ID class_param_default |
	TOK_PARAMETER non_io_wire_type TOK_ID class_param_default |
	TOK_LOCALPARAM non_io_wire_type TOK_ID class_param_default |
	TOK_TYPE TOK_ID class_param_default |
	non_io_wire_type TOK_ID class_param_default |
	TOK_ID class_param_default;

class_param_default:
	%empty |
	TOK_EQ TOK_VIRTUAL TOK_INTERFACE TOK_ID |
	TOK_EQ TOK_VIRTUAL hierarchical_id |
	TOK_EQ expr |
	TOK_EQ TOK_INT |
	TOK_EQ TOK_INTEGER |
	TOK_EQ TOK_LOGIC |
	TOK_EQ TOK_BYTE |
	TOK_EQ hierarchical_type_id TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN |
	TOK_EQ TOK_ID TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN;

class_extends_opt:
	%empty |
	TOK_EXTENDS class_type_refs class_extends_args;

class_extends_args:
	%empty |
	TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN |
	TOK_LPAREN arg_list optional_comma TOK_RPAREN;

class_implements_opt:
	%empty |
	TOK_IMPLEMENTS class_type_refs;

class_type_refs:
	class_type_ref |
	class_type_refs TOK_COMMA class_type_ref;

class_type_ref:
	TOK_ID | TOK_USER_TYPE | TOK_PKG_USER_TYPE | TOK_SCOPED_ID |
	TOK_ID TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN |
	TOK_USER_TYPE TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN |
	TOK_PKG_USER_TYPE TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN |
	TOK_SCOPED_ID TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN;

class_items:
	%empty |
	class_items class_item;

class_member_quals:
	%empty |
	class_member_quals TOK_STATIC |
	class_member_quals TOK_VIRTUAL |
	class_member_quals TOK_LOCAL |
	class_member_quals TOK_PROTECTED |
	class_member_quals TOK_EXTERN |
	class_member_quals TOK_PURE |
	class_member_quals TOK_RAND |
	class_member_quals TOK_RANDC |
	class_member_quals TOK_CONST |
	class_member_quals ATTR_BEGIN {
		if (extra->attr_list)
			extra->attr_list_stack.push(std::move(extra->attr_list));
		extra->attr_list = std::make_unique<dict<IdString, std::unique_ptr<AstNode>>>();
		for (auto &it : extra->default_attr_list)
			(*extra->attr_list)[it.first] = it.second->clone();
	} opt_attr_list ATTR_END {
		// member attributes are dropped: class bodies are opaque
		extra->attr_list.reset();
		if (!extra->attr_list_stack.empty()) {
			extra->attr_list = std::move(extra->attr_list_stack.top());
			extra->attr_list_stack.pop();
		}
	};

// Method prototypes (header only, no body) are reached through distinct
// qualifier prefixes so that they never share LALR states with the full-body
// alternatives: the ';'-versus-body decision after a method header is not
// LR(1) decidable, so the two forms must not occupy the same states. Only
// `extern' and `pure virtual' prefixes are unambiguous: any qualifier that
// can also precede a member variable (local, protected, static, ...) would
// be stolen into the prototype path by shift/reduce resolution.
proto_quals:
	TOK_EXTERN class_member_quals |
	TOK_PURE TOK_VIRTUAL class_member_quals;

class_item:
	class_member_quals task_func_decl |
	class_member_quals class_method_body |
	proto_quals class_method_proto |
	class_member_quals wire_decl |
	class_member_quals param_decl |
	class_member_quals localparam_decl |
	class_member_quals typedef_decl |
	class_member_quals enum_decl |
	class_member_quals sva_let_decl |
	class_member_quals nested_class_decl |
	TOK_SEMICOL;

class_method_proto:
	TOK_FUNCTION opt_automatic func_return_type TOK_ID {
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_FUNCTION));
		extra->current_function_or_task->str = *$4;
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	TOK_FUNCTION opt_automatic TOK_ID {
		// no return type declared (e.g. `function new(...);` constructors)
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_FUNCTION));
		extra->current_function_or_task->str = *$3;
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	TOK_FUNCTION opt_automatic TOK_VOID TOK_ID {
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_TASK));
		extra->current_function_or_task->str = *$4;
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	TOK_TASK opt_automatic TOK_ID {
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_TASK));
		extra->current_function_or_task->str = *$3;
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	};

class_method_body:
	TOK_FUNCTION opt_automatic func_return_type TOK_ID {
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_FUNCTION));
		extra->current_function_or_task->str = *$4;
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDFUNCTION opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	TOK_FUNCTION opt_automatic TOK_ID {
		// no return type declared (e.g. `function new(...);` constructors)
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_FUNCTION));
		extra->current_function_or_task->str = *$3;
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDFUNCTION opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	TOK_FUNCTION opt_automatic TOK_VOID TOK_ID {
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_TASK));
		extra->current_function_or_task->str = *$4;
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDFUNCTION opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	TOK_TASK opt_automatic TOK_ID {
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_TASK));
		extra->current_function_or_task->str = *$3;
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDTASK opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	};

scoped_import_list:
	TOK_SCOPED_ID {
		auto text = *$1;
		if (!text.empty() && text[0] == '\\')
			text.erase(0, 1);
		auto sep = text.find("::");
		auto import_node = std::make_unique<AstNode>(@$, AST_IMPORT);
		if (sep == std::string::npos)
			import_node->str = *$1;
		else
			import_node->str = "\\" + text.substr(0, sep);
		extra->ast_stack.back()->children.push_back(std::move(import_node));
	} |
	TOK_PKG_USER_TYPE {
		auto text = *$1;
		if (!text.empty() && text[0] == '\\')
			text.erase(0, 1);
		auto sep = text.find("::");
		auto import_node = std::make_unique<AstNode>(@$, AST_IMPORT);
		if (sep == std::string::npos)
			import_node->str = *$1;
		else
			import_node->str = "\\" + text.substr(0, sep);
		extra->ast_stack.back()->children.push_back(std::move(import_node));
	} |
	scoped_import_list TOK_COMMA TOK_PKG_USER_TYPE {
		auto text = *$3;
		if (!text.empty() && text[0] == '\\')
			text.erase(0, 1);
		auto sep = text.find("::");
		auto import_node = std::make_unique<AstNode>(@$, AST_IMPORT);
		if (sep == std::string::npos)
			import_node->str = *$3;
		else
			import_node->str = "\\" + text.substr(0, sep);
		extra->ast_stack.back()->children.push_back(std::move(import_node));
	} |
	scoped_import_list TOK_COMMA TOK_SCOPED_ID {
		auto text = *$3;
		if (!text.empty() && text[0] == '\\')
			text.erase(0, 1);
		auto sep = text.find("::");
		auto import_node = std::make_unique<AstNode>(@$, AST_IMPORT);
		if (sep == std::string::npos)
			import_node->str = *$3;
		else
			import_node->str = "\\" + text.substr(0, sep);
		extra->ast_stack.back()->children.push_back(std::move(import_node));
	};

wildcard_import_list:
	TOK_ID TOK_PACKAGESEP TOK_ASTER {
		auto import_node = std::make_unique<AstNode>(@$, AST_IMPORT);
		import_node->str = *$1;
		extra->ast_stack.back()->children.push_back(std::move(import_node));
	} |
	wildcard_import_list TOK_COMMA TOK_ID TOK_PACKAGESEP TOK_ASTER {
		auto import_node = std::make_unique<AstNode>(@$, AST_IMPORT);
		import_node->str = *$3;
		extra->ast_stack.back()->children.push_back(std::move(import_node));
	};

import_stmt:
	TOK_IMPORT scoped_import_list TOK_SEMICOL |
	TOK_IMPORT wildcard_import_list TOK_SEMICOL |
	TOK_IMPORT TOK_ID TOK_PACKAGESEP {
		// Start a specific import: create and push the AST_IMPORT node
		AstNode* import_node = extra->pushChild(std::make_unique<AstNode>(@$, AST_IMPORT));
		import_node->str = *$2;
	} import_item_list TOK_SEMICOL {
		// Done collecting specific items, pop the AST_IMPORT node
		extra->ast_stack.pop_back();
	};

import_item_list:
	import_item |
	import_item_list TOK_COMMA import_item ;

import_item:
	TOK_ID {
		// Append this specific import name under the current AST_IMPORT
		auto item_node = std::make_unique<AstNode>(@$, AST_NONE);
		item_node->str = *$1;
		extra->ast_stack.back()->children.push_back(std::move(item_node));
	};

interface:
	TOK_INTERFACE {
		extra->enterTypeScope();
	} TOK_ID {
		extra->do_not_require_port_stubs = false;
		AstNode* intf = extra->pushChild(std::make_unique<AstNode>(@$, AST_INTERFACE));
		extra->current_ast_mod = intf;
		extra->port_stubs.clear();
		extra->port_counter = 0;
		// default / global clocking are per module (IEEE 14.12, 14.14): a
		// default clocking of an earlier module used to clock this module's
		// unclocked assertions (an undriven implicit clock net)
		extra->sva_default_clock.reset();
		extra->sva_global_clock.reset();
		intf->str = *$3;
		extra->interface_names.insert(*$3);
	} module_header_imports module_para_opt module_args_opt TOK_SEMICOL interface_body TOK_ENDINTERFACE opt_label opt_semi {
		if (extra->port_stubs.size() != 0)
			err_at_loc(@8, "Missing details for module port `%s'.",
				extra->port_stubs.begin()->first.c_str());
		extra->ast_stack.pop_back();
		log_assert(extra->ast_stack.size() == 1);
		extra->current_ast_mod = nullptr;
		extra->exitTypeScope();
	};

interface_body:
	interface_body interface_body_stmt |
	interface_body gen_stmt |
	interface_body gen_block |
	interface_body TOK_SEMICOL |
	%empty;

interface_body_stmt:
	param_decl | localparam_decl | typedef_decl | defparam_decl | wire_decl | always_stmt | assign_stmt |
	modport_stmt | import_stmt | assert | cell_stmt | clocking_decl | task_func_decl | bind_stmt |
	sva_sequence_decl | sva_property_decl | sva_let_decl | sva_default_disable;

// bind_target matches the target of the bind (everything before
// bind_instantiation in the IEEE 1800 spec).
//
// We can't use the BNF from the spec directly because it's ambiguous:
// something like "bind foo bar_i (.*)" can either be interpreted with "foo" as
// a module or interface identifier (matching bind_target_scope in the spec) or
// by considering foo as a degenerate hierarchical identifier with no TOK_DOT
// characters, followed by no bit select (which matches bind_target_instance in
// the spec).
//
// Instead, we resolve everything as an instance name and then deal with the
// ambiguity when converting to RTLIL / in the hierarchy pass.
bind_name:
	TOK_ID { $$ = std::move($1); } |
	TOK_USER_TYPE { $$ = std::move($1); } |
	TOK_PKG_USER_TYPE { $$ = std::move($1); } |
	TOK_SCOPED_ID { $$ = std::move($1); };

// IEEE 1800-2017 23.11: the bound instances belong to the TARGET module
// (their port expressions resolve there).  The parser tags each instance
// with the target name (attribute sv_bind_target); AST::process moves it
// into the target module's AST before that module is elaborated, re-
// elaborating a target read by an earlier read_verilog.  (The instances
// used to stay where the bind was written: at file level they became a
// "global" appended to every later module -- duplicate cells -- or to
// none, silently dropping the bound checker.)
bind_stmt:
	TOK_BIND bind_name bind_name {
		extra->astbuf1 = std::make_unique<AstNode>(@$, AST_CELL);
		extra->astbuf1->children.push_back(std::make_unique<AstNode>(@$, AST_CELLTYPE));
		extra->astbuf1->children[0]->str = *$3;
		extra->bind_mark = extra->ast_stack.back()->children.size();
	} cell_parameter_list_opt cell_list TOK_SEMICOL {
		(void)extra->astbuf1.reset();
		auto &bound = extra->ast_stack.back()->children;
		for (size_t i = extra->bind_mark; i < bound.size(); i++)
			if (bound[i] && bound[i]->type == AST_CELL)
				bound[i]->set_attribute(ID(sv_bind_target), AstNode::mkconst_str(@2, *$2));
	};

bind_target:
	bind_target_instance opt_bind_target_instance_list;

// An optional list of target instances for a bind statement, introduced by a
// colon.
opt_bind_target_instance_list:
	TOK_COL bind_target_instance_list |
	%empty;

bind_target_instance_list:
	bind_target_instance |
	bind_target_instance_list TOK_COMMA bind_target_instance;

// A single target instance for a bind statement. The top of extra->ast_stack will be
// the bind node where we should add it.
bind_target_instance:
	hierarchical_id {
		auto node = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		node->str = *$1;
		extra->ast_stack.back()->children.push_back(std::move(node));
	};

mintypmax_expr:
	expr { } |
	expr TOK_COL expr TOK_COL expr { };

non_opt_delay:
	TOK_HASH TOK_ID { } |
	TOK_HASH TOK_CONSTVAL { } |
	TOK_HASH TOK_REALVAL { } |
	// our `expr` doesn't have time_scale, so we need the parenthesized variant
	TOK_HASH TOK_TIME_SCALE |
	TOK_HASH TOK_LPAREN mintypmax_expr TOK_RPAREN |
	TOK_HASH TOK_LPAREN mintypmax_expr TOK_COMMA mintypmax_expr TOK_RPAREN |
	TOK_HASH TOK_LPAREN mintypmax_expr TOK_COMMA mintypmax_expr TOK_COMMA mintypmax_expr TOK_RPAREN;

delay:
	non_opt_delay | %empty;

io_wire_type:
	{ extra->astbuf3 = std::make_unique<AstNode>(@$, AST_WIRE); extra->current_wire_rand = false; extra->current_wire_const = false; extra->current_wire_automatic = false; }
	wire_type_token_io wire_type_const_rand opt_wire_type_token wire_type_signedness
	{ $$ = std::move(extra->astbuf3); SET_RULE_LOC(@$, @2, @$); };

non_io_wire_type:
	{ extra->astbuf3 = std::make_unique<AstNode>(@$, AST_WIRE); extra->current_wire_rand = false; extra->current_wire_const = false; extra->current_wire_automatic = false; }
	opt_lifetime wire_type_const_rand wire_type_token wire_type_signedness
	{
		if (extra->current_wire_automatic)
			extra->astbuf3->set_attribute(ID::nosync, AstNode::mkconst_int(extra->astbuf3->location, 1, false));
		$$ = std::move(extra->astbuf3); SET_RULE_LOC(@$, @2, @$);
	};

wire_type:
	io_wire_type { $$ = std::move($1); }  |
	non_io_wire_type { $$ = std::move($1); };

wire_type_token_io:
	TOK_INPUT {
		extra->astbuf3->is_input = true;
	} |
	TOK_OUTPUT {
		extra->astbuf3->is_output = true;
	} |
	TOK_INOUT {
		extra->astbuf3->is_input = true;
		extra->astbuf3->is_output = true;
	} |
	TOK_REF {
		if (!mode->sv)
			err_at_loc(@1, "ref ports are only supported in SystemVerilog mode.");
		extra->astbuf3->is_input = true;
		extra->astbuf3->is_output = true;
		extra->astbuf3->set_attribute(ID(sv_ref), AstNode::mkconst_int(@1, 1, false));
	} |
	TOK_CONST_REF {
		// `const ref` argument direction (IEEE 1800-2009 13.5.2): treated
		// like an ordinary ref port
		if (!mode->sv)
			err_at_loc(@1, "ref ports are only supported in SystemVerilog mode.");
		extra->astbuf3->is_input = true;
		extra->astbuf3->is_output = true;
		extra->astbuf3->set_attribute(ID(sv_ref), AstNode::mkconst_int(@1, 1, false));
	};

wire_type_signedness:
	TOK_SIGNED   { extra->astbuf3->is_signed = true;  } |
	TOK_UNSIGNED { extra->astbuf3->is_signed = false; } |
	%empty;

wire_type_const_rand:
	TOK_RAND TOK_CONST {
	    extra->current_wire_rand = true;
	    extra->current_wire_const = true;
	} |
	TOK_CONST {
	    extra->current_wire_const = true;
	} |
	TOK_RAND {
	    extra->current_wire_rand = true;
	} |
	%empty;

opt_wire_type_token:
	wire_type_token | %empty;

wire_type_token:
	// nets
	net_type opt_vectoredness {
	} |
	net_type logic_type opt_vectoredness {
	} |
	TOK_VECTORED {
	} |
	TOK_SCALARED {
	} |
	// regs
	TOK_REG {
		extra->astbuf3->is_reg = true;
	} |
	TOK_VAR TOK_REG {
		extra->astbuf3->is_reg = true;
	} |
	// logics
	TOK_VAR {
		extra->astbuf3->is_logic = true;
	} |
	TOK_VAR logic_type {
		extra->astbuf3->is_logic = true;
	} |
	logic_type {
		extra->astbuf3->is_logic = true;
	} |
	TOK_GENVAR {
		extra->astbuf3->type = AST_GENVAR;
		extra->astbuf3->is_reg = true;
		extra->astbuf3->is_signed = true;
		extra->astbuf3->range_left = 31;
		extra->astbuf3->range_right = 0;
	} |
	// IEEE: time variables hold 64-bit unsigned values.
	TOK_TIME {
		extra->astbuf3->is_reg = true;
		extra->astbuf3->range_left = 63;
		extra->astbuf3->range_right = 0;
	} |
	TOK_REAL {
		extra->astbuf3->children.push_back(std::make_unique<AstNode>(@$, AST_REALVALUE));
	} |
	TOK_MAILBOX class_type_params |
	TOK_SEMAPHORE class_type_params {
		// mailbox #(T) / semaphore variables: simulation-only, modeled as
		// plain storage
		extra->astbuf3->is_reg = true;
	};

net_type:
	TOK_WOR {
		extra->astbuf3->is_wor = true;
	} |
	TOK_WAND {
		extra->astbuf3->is_wand = true;
	} |
	TOK_TRI | TOK_TRI0 | TOK_TRI1 | TOK_TRIAND | TOK_TRIOR | TOK_TRIREG | TOK_UWIRE |
	TOK_WIRE;

logic_type:
	TOK_LOGIC {
	} |
	integer_atom_type {
		extra->astbuf3->range_left = $1 - 1;
		extra->astbuf3->range_right = 0;
		extra->astbuf3->is_signed = true;
	} |
	hierarchical_type_id {
		extra->addWiretypeNode($1.get(), extra->astbuf3.get());
	} |
	TOK_SCOPED_ID {
		extra->addWiretypeNode($1.get(), extra->astbuf3.get());
	} |
	hierarchical_type_id TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN {
		// parameterized class type (e.g. an argument of type `uvm_visitor #(T)`)
		extra->addWiretypeNode($1.get(), extra->astbuf3.get());
	} |
	TOK_SCOPED_ID TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN {
		extra->addWiretypeNode($1.get(), extra->astbuf3.get());
	};

opt_vectoredness:
	%empty |
	TOK_VECTORED |
	TOK_SCALARED;

type_operand:
	expr { $$ = std::move($1); } |
	typedef_base_type range_or_multirange { $$ = std::move($1); };

opt_type_cast:
	%empty { $$ = nullptr; } |
	OP_CAST TOK_LPAREN expr TOK_RPAREN { $$ = std::move($3); };

integer_atom_type:
	TOK_INTEGER	{ $$ = 32; } |
	TOK_INT		{ $$ = 32; } |
	TOK_SHORTINT	{ $$ = 16; } |
	TOK_LONGINT	{ $$ = 64; } |
	TOK_BYTE	{ $$ =  8; } ;

integer_vector_type:
	TOK_LOGIC { $$ = token::TOK_LOGIC; } |
	TOK_REG   { $$ = token::TOK_REG; } ;

non_opt_range:
	TOK_LBRA expr TOK_COL expr TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(std::move($2));
		$$->children.push_back(std::move($4));
	} |
	TOK_LBRA expr TOK_POS_INDEXED expr TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		auto expr = std::make_unique<AstNode>(@$, AST_SELFSZ, std::move($2));
		$$->children.push_back(std::make_unique<AstNode>(@$, AST_SUB, std::make_unique<AstNode>(@$, AST_ADD, expr->clone(), std::move($4)), AstNode::mkconst_int(@$, 1, true)));
		$$->children.push_back(std::make_unique<AstNode>(@$, AST_ADD, std::move(expr), AstNode::mkconst_int(@$, 0, true)));
	} |
	TOK_LBRA expr TOK_NEG_INDEXED expr TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		auto expr = std::make_unique<AstNode>(@$, AST_SELFSZ, std::move($2));
		$$->children.push_back(std::make_unique<AstNode>(@$, AST_ADD, expr->clone(), AstNode::mkconst_int(@$, 0, true)));
		$$->children.push_back(std::make_unique<AstNode>(@$, AST_SUB, std::make_unique<AstNode>(@$, AST_ADD, std::move(expr), AstNode::mkconst_int(@$, 1, true)), std::move($4)));
	} |
	TOK_LBRA expr TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(std::move($2));
	} |
	TOK_LBRA TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA integer_atom_type TOK_RBRA {
		// associative-array style dimension, e.g. `uvm_resource_base rsrc[int]`:
		// approximated as a single unconstrained dimension
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA integer_atom_type TOK_SIGNED TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA integer_atom_type TOK_UNSIGNED TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA integer_atom_type TOK_LBRA expr TOK_COL expr TOK_RBRA TOK_RBRA {
		// associative-array style dimension with packed index type,
		// e.g. `mem[int [15:0]]`
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA TOK_LOGIC TOK_LBRA expr TOK_COL expr TOK_RBRA TOK_RBRA {
		// e.g. `mem[bit [31:0]]` / `arr[logic [P.XLEN-1:0]]`
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA TOK_LOGIC TOK_SIGNED TOK_LBRA expr TOK_COL expr TOK_RBRA TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA TOK_LOGIC TOK_UNSIGNED TOK_LBRA expr TOK_COL expr TOK_RBRA TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA TOK_REG TOK_LBRA expr TOK_COL expr TOK_RBRA TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA TOK_TIME TOK_RBRA {
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA TOK_LOGIC TOK_RBRA {
		// associative-array style dimension with a bare vector index type,
		// e.g. `arr[bit]` / `arr[logic]`
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} |
	TOK_LBRA TOK_LOGIC TOK_LBRA expr TOK_COL expr TOK_RBRA TOK_LBRA expr TOK_COL expr TOK_RBRA TOK_RBRA {
		// e.g. `arr[bit[31:0][k:0]]`
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	};

non_opt_multirange:
	non_opt_range non_opt_range {
		$$ = std::make_unique<AstNode>(@$, AST_MULTIRANGE, std::move($1), std::move($2));
	} |
	non_opt_multirange non_opt_range {
		$$ = std::move($1);
		$$->children.push_back(std::move($2));
	};

range:
	non_opt_range {
		$$ = std::move($1);
	} |
	%empty {
		$$ = nullptr;
	};

range_or_multirange:
	range { $$ = std::move($1); } |
	non_opt_multirange { $$ = std::move($1); };

module_body:
	module_body module_body_stmt |
	/* the following line makes the generate..endgenrate keywords optional */
	module_body gen_stmt |
	module_body gen_block |
	module_body TOK_SEMICOL |
	%empty;

timeunit_decl:
	TOK_TIMEUNIT TOK_TIME_SCALE TOK_SEMICOL |
	TOK_TIMEPRECISION TOK_TIME_SCALE TOK_SEMICOL |
	TOK_TIMEUNIT TOK_TIME_SCALE TOK_SLASH TOK_TIME_SCALE TOK_SEMICOL;

module_body_stmt:
	task_func_decl | specify_block | param_decl | localparam_decl | typedef_decl | defparam_decl | specparam_declaration | wire_decl | assign_stmt | cell_stmt |
	timeunit_decl |
	enum_decl | struct_decl |
	always_stmt | TOK_GENERATE module_gen_body TOK_ENDGENERATE | defattr | assert | checker_decl | ignored_specify_block |
	sva_sequence_decl | sva_property_decl | sva_let_decl | clocking_decl | import_stmt | sva_default_disable |
	dpi_export_decl | bind_stmt | class_decl;

checker_port_default:
	TOK_EQ expr {
		$$ = std::move($2);
	} |
	%empty {
		$$ = nullptr;
	};

checker_port_tok:
	TOK_UNTYPED | TOK_SEQUENCE | TOK_PROPERTY | TOK_INTEGER | TOK_INT |
	TOK_LOGIC | TOK_REG |
	%empty;

checker_port_item:
	checker_port_tok TOK_ID checker_port_default {
		// IEEE 1800-2009 17.2: the formal's type (property/sequence/untyped)
		// does not affect the synchronous compiled semantics; expansion is a
		// textual substitution of the actual for the formal (like named
		// property/sequence instances), so only the name and the optional
		// default expression are kept.
		auto formal = std::make_unique<AstNode>(@2, AST_IDENTIFIER);
		formal->str = *$2;
		if ($3)
			formal->children.push_back(std::move($3));
		extra->checker_formals.push_back(std::move(formal));
	};

checker_port_list:
	checker_port_item |
	checker_port_list TOK_COMMA checker_port_item;

checker_ports_opt:
	%empty |
	TOK_LPAREN checker_port_list optional_comma TOK_RPAREN;

checker_decl:
	TOK_CHECKER TOK_ID {
		extra->checker_formals.clear();
	} checker_ports_opt TOK_SEMICOL {
		auto id = std::make_unique<AstNode>(@2, AST_CELLTYPE);
		id->str = *$2;
		auto node = mk_sva(location_range(@1, @5), "checker_decl");
		node->children.push_back(std::move(id));
		// children[1]: body container the module_body items parse into
		AstNode *n = extra->pushChild(std::move(node));
		(void)n;
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_GENBLOCK));
		// A checker's own default clocking/disable applies to assertions
		// inside it and is restored at endchecker; a checker without one
		// inherits the enclosing scope's via the parser state.
		extra->checker_clk_backup = std::move(extra->sva_default_clock);
		extra->checker_dis_backup = std::move(extra->sva_default_disable);
		extra->in_checker++;
	} module_body TOK_ENDCHECKER opt_label {
		checkLabelsMatch(@8, "Checker label", $2.get(), $9.get());
		extra->ast_stack.pop_back();
		AstNode *node = extra->ast_stack.back();
		log_assert(node->type == AST_SVA && node->str == "checker_decl");
		for (auto &formal : extra->checker_formals)
			node->children.push_back(std::move(formal));
		extra->checker_formals.clear();
		// Record the effective default clocking/disable (own or inherited)
		// so expansion can retrofit assertions parsed before the
		// `default clocking` declaration.
		if (extra->sva_default_clock)
			node->children.push_back(sva_add(mk_sva(@$, "checker_default_clock"), extra->sva_default_clock->clone()));
		if (extra->sva_default_disable)
			node->children.push_back(sva_add(mk_sva(@$, "checker_default_disable"), extra->sva_default_disable->clone()));
		extra->sva_default_clock = std::move(extra->checker_clk_backup);
		extra->sva_default_disable = std::move(extra->checker_dis_backup);
		log_assert(GetSize(node->children) >= 2 && node->children[1]->type == AST_GENBLOCK);
		checker_body_check(node->children[1].get());
		extra->in_checker--;
		extra->ast_stack.pop_back();
	};

dpi_export_decl:
	TOK_EXPORT TOK_STRING TOK_TASK TOK_ID TOK_SEMICOL |
	TOK_EXPORT TOK_STRING TOK_FUNCTION TOK_ID TOK_SEMICOL |
	TOK_EXPORT TOK_STRING TOK_EXTERN TOK_TASK TOK_ID TOK_SEMICOL |
	TOK_EXPORT TOK_STRING TOK_EXTERN TOK_FUNCTION TOK_ID TOK_SEMICOL |
	TOK_EXPORT TOK_SCOPED_ID TOK_SEMICOL {
		// IEEE 1800-2017 26.6: `export B::name;` in package A makes the
		// imported name visible as `A::name` (OpenTitan spi_device_pkg
		// re-exports spi_device_reg_pkg depths). Declare an alias in A:
		// a typedef for a type, otherwise a localparam. Wildcard exports
		// (`export B::*`, `export *::*`) stay unmodelled.
		AstNode *pkg = extra->current_ast_mod;
		std::string q = *$2;
		size_t sep = q.rfind("::");
		std::string leaf = sep == std::string::npos ? "" : q.substr(sep + 2);
		if (pkg && pkg->type == AST_PACKAGE && !leaf.empty() && leaf != "*") {
			std::string name = "\\" + leaf;
			if (extra->pkg_user_types.count(q)) {
				auto wire = std::make_unique<AstNode>(@2, AST_WIRE);
				wire->is_logic = true;
				auto wt = std::make_unique<AstNode>(@2, AST_WIRETYPE);
				wt->str = q;
				wire->children.push_back(std::move(wt));
				wire->is_custom_type = true;
				auto td = std::make_unique<AstNode>(@2, AST_TYPEDEF, std::move(wire));
				td->str = name;
				AstNode *tnode = extra->saveChild(std::move(td));
				extra->pkg_user_types[pkg->str + "::" + leaf] = tnode;
			} else {
				auto id = std::make_unique<AstNode>(@2, AST_IDENTIFIER);
				id->str = q;
				auto lp = std::make_unique<AstNode>(@2, AST_LOCALPARAM, std::move(id));
				lp->str = name;
				lp->set_attribute(ID(sv_export_alias), AstNode::mkconst_int(@2, 1, false));
				extra->saveChild(std::move(lp));
			}
		}
	};

task_func_decl:
	attr TOK_DPI_FUNCTION TOK_ID dpi_signing TOK_ID {
		extra->current_function_or_task = extra->saveChild(std::make_unique<AstNode>(@$, AST_DPI_FUNCTION, AstNode::mkconst_str(@3, *$3), AstNode::mkconst_str(@5, *$5)));
		extra->current_function_or_task->str = *$5;
		append_attr(extra->current_function_or_task, std::move($1));
	} opt_dpi_function_args TOK_SEMICOL {
		extra->current_function_or_task = nullptr;
	} |
	attr TOK_DPI_FUNCTION TOK_ID TOK_EQ TOK_ID TOK_ID {
		extra->current_function_or_task = extra->saveChild(std::make_unique<AstNode>(@$, AST_DPI_FUNCTION, AstNode::mkconst_str(@5, *$5), AstNode::mkconst_str(@3, *$3)));
		extra->current_function_or_task->str = *$6;
		append_attr(extra->current_function_or_task, std::move($1));
	} opt_dpi_function_args TOK_SEMICOL {
		extra->current_function_or_task = nullptr;
	} |
	attr TOK_DPI_FUNCTION TOK_ID TOK_COL TOK_ID TOK_EQ TOK_ID TOK_ID {
		extra->current_function_or_task = extra->saveChild(std::make_unique<AstNode>(@$, AST_DPI_FUNCTION, AstNode::mkconst_str(@7, *$7), AstNode::mkconst_str(location_range(@3, @5), *$3 + ":" + RTLIL::unescape_id(*$5))));
		extra->current_function_or_task->str = *$8;
		append_attr(extra->current_function_or_task, std::move($1));
	} opt_dpi_function_args TOK_SEMICOL {
		extra->current_function_or_task = nullptr;
	} |
	attr TOK_TASK opt_automatic TOK_ID {
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_TASK));
		extra->current_function_or_task->str = *$4;
		append_attr(extra->current_function_or_task, std::move($1));
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDTASK opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	attr TOK_FUNCTION opt_automatic TOK_VOID TOK_ID {
		// The difference between void functions and tasks is that
		// always_comb's implicit sensitivity list behaves as if functions were
		// inlined, but ignores signals read only in tasks. This only matters
		// for event based simulation, and for synthesis we can treat a void
		// function like a task.
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_TASK));
		extra->current_function_or_task->str = *$5;
		append_attr(extra->current_function_or_task, std::move($1));
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDFUNCTION opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	attr TOK_FUNCTION opt_automatic func_return_type TOK_ID {
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_FUNCTION));
		extra->current_function_or_task->str = *$5;
		append_attr(extra->current_function_or_task, std::move($1));
		auto outreg = std::make_unique<AstNode>(@$, AST_WIRE);
		outreg->str = *$5;
		outreg->is_signed = false;
		outreg->is_reg = true;
		if ($4 != nullptr) {
			outreg->is_signed = $4->is_signed;
			$4->is_signed = false;
			outreg->is_custom_type = $4->type == AST_WIRETYPE;
			std::unique_ptr<AstNode> packed;
			if ($4->type == AST_WIRETYPE && !$4->children.empty() && $4->children[0] &&
					($4->children[0]->type == AST_RANGE || $4->children[0]->type == AST_MULTIRANGE)) {
				packed = std::move($4->children[0]);
				$4->children.clear();
			}
			outreg->children.push_back(std::move($4));
			if (packed)
				outreg->children.push_back(std::move(packed));
		}
		extra->current_function_or_task->children.push_back(std::move(outreg));
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDFUNCTION opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	attr TOK_FUNCTION opt_automatic TOK_ID {
		// IEEE 1800-2009 13.4: with no return type declared a function
		// returns implicit 1-bit logic. Distinguishable from the typed rule
		// above by what follows the identifier: `f(...)` names the function.
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_FUNCTION));
		extra->current_function_or_task->str = *$4;
		append_attr(extra->current_function_or_task, std::move($1));
		auto outreg = std::make_unique<AstNode>(@$, AST_WIRE);
		outreg->str = *$4;
		outreg->is_signed = false;
		outreg->is_reg = true;
		extra->current_function_or_task->children.push_back(std::move(outreg));
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDFUNCTION opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	attr TOK_FUNCTION opt_automatic func_return_type TOK_SCOPED_ID {
		// out-of-block class method definition (e.g.
		// `function void uvm_object::copy(uvm_object rhs); ... endfunction`).
		// The body is kept as a child of the package scope; it is tagged so
		// that elaboration can treat it as opaque like the class itself.
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_FUNCTION));
		extra->current_function_or_task->str = *$5;
		extra->current_function_or_task->set_attribute(ID(sv_class_method_def), AstNode::mkconst_int(@$, 1, false));
		append_attr(extra->current_function_or_task, std::move($1));
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDFUNCTION opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	attr TOK_FUNCTION opt_automatic TOK_VOID TOK_SCOPED_ID {
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_TASK));
		extra->current_function_or_task->str = *$5;
		extra->current_function_or_task->set_attribute(ID(sv_class_method_def), AstNode::mkconst_int(@$, 1, false));
		append_attr(extra->current_function_or_task, std::move($1));
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDFUNCTION opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	attr TOK_FUNCTION opt_automatic TOK_SCOPED_ID {
		// out-of-block constructor definition (e.g. `function uvm_object::new(...)`)
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_FUNCTION));
		extra->current_function_or_task->str = *$4;
		extra->current_function_or_task->set_attribute(ID(sv_class_method_def), AstNode::mkconst_int(@$, 1, false));
		append_attr(extra->current_function_or_task, std::move($1));
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDFUNCTION opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	} |
	attr TOK_TASK opt_automatic TOK_SCOPED_ID {
		extra->current_function_or_task = extra->pushChild(std::make_unique<AstNode>(@$, AST_TASK));
		extra->current_function_or_task->str = *$4;
		extra->current_function_or_task->set_attribute(ID(sv_class_method_def), AstNode::mkconst_int(@$, 1, false));
		append_attr(extra->current_function_or_task, std::move($1));
		extra->current_function_or_task_port_id = 1;
	} task_func_args_opt TOK_SEMICOL task_func_body TOK_ENDTASK opt_label {
		extra->current_function_or_task = nullptr;
		extra->ast_stack.pop_back();
	};

func_return_type:
	hierarchical_type_id {
		$$ = std::make_unique<AstNode>(@$, AST_WIRETYPE);
		$$->str = *$1;
	} |
	hierarchical_type_id non_opt_range {
		$$ = std::make_unique<AstNode>(@$, AST_WIRETYPE);
		$$->str = *$1;
		$$->children.push_back(std::move($2));
	} |
	hierarchical_type_id non_opt_multirange {
		$$ = std::make_unique<AstNode>(@$, AST_WIRETYPE);
		$$->str = *$1;
		$$->children.push_back(std::move($2));
	} |
	TOK_SCOPED_ID {
		$$ = std::make_unique<AstNode>(@$, AST_WIRETYPE);
		$$->str = *$1;
	} |
	TOK_SCOPED_ID non_opt_range {
		$$ = std::make_unique<AstNode>(@$, AST_WIRETYPE);
		$$->str = *$1;
		$$->children.push_back(std::move($2));
	} |
	TOK_SCOPED_ID non_opt_multirange {
		$$ = std::make_unique<AstNode>(@$, AST_WIRETYPE);
		$$->str = *$1;
		$$->children.push_back(std::move($2));
	} |
	hierarchical_type_id TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN {
		// parameterized class return type (e.g. `function uvm_visitor #(T) f();`)
		$$ = std::make_unique<AstNode>(@$, AST_WIRETYPE);
		$$->str = *$1;
	} |
	TOK_ID TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN {
		// same, with the parameterized type not yet registered
		$$ = std::make_unique<AstNode>(@$, AST_WIRETYPE);
		$$->str = *$1;
	} |
	TOK_SCOPED_ID TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN {
		$$ = std::make_unique<AstNode>(@$, AST_WIRETYPE);
		$$->str = *$1;
	} |
	opt_type_vec opt_signedness_default_unsigned {
		$$ = makeRange(@$, 0, 0, $2);
	} |
	opt_type_vec opt_signedness_default_unsigned param_packed {
		$$ = std::move($3);
		$$->is_signed = $2;
	} |
	integer_atom_type opt_signedness_default_signed {
		$$ = makeRange(@$, $1 - 1, 0, $2);
	} |
	TOK_TIME {
		$$ = makeRange(@$, 63, 0, false);
	} |
	TOK_REAL {
		$$ = std::make_unique<AstNode>(@$, AST_WIRE);
		$$->children.push_back(std::make_unique<AstNode>(@$, AST_REALVALUE));
	};

opt_type_vec:
	  %empty
	| TOK_REG
	| TOK_LOGIC
	;

opt_signedness_default_signed:
	  %empty	{ $$ = true; }
	| TOK_SIGNED	{ $$ = true; }
	| TOK_UNSIGNED	{ $$ = false; }
	;
opt_signedness_default_unsigned:
	  %empty	{ $$ = false; }
	| TOK_SIGNED	{ $$ = true; }
	| TOK_UNSIGNED	{ $$ = false; }
	;

dpi_signing:
	TOK_SIGNED | TOK_UNSIGNED | %empty;

dpi_function_arg:
	TOK_ID TOK_ID {
		extra->current_function_or_task->children.push_back(AstNode::mkconst_str(@1, *$1));
	} |
	TOK_ID {
		extra->current_function_or_task->children.push_back(AstNode::mkconst_str(@1, *$1));
	} |
	wire_type range_or_multirange TOK_ID {
		extra->current_function_or_task->children.push_back(AstNode::mkconst_str(@3, *$3));
		extra->astbuf3.reset();
	} dpi_open_array_suffix;

// DPI formals may be open/dynamic arrays: `input bit [7:0] msg[]'.
// yosys does not implement DPI calls; the suffix is parsed and dropped.
dpi_open_array_suffix:
	dpi_open_array_suffix TOK_LBRA TOK_RBRA |
	%empty;

opt_dpi_function_args:
	TOK_LPAREN dpi_function_args TOK_RPAREN |
	%empty;

dpi_function_args:
	dpi_function_args TOK_COMMA dpi_function_arg |
	dpi_function_args TOK_COMMA |
	dpi_function_arg |
	%empty;

opt_automatic:
	TOK_AUTOMATIC |
	TOK_STATIC |
	%empty;

opt_lifetime:
	TOK_AUTOMATIC { extra->current_wire_automatic = true; } |
	TOK_STATIC |
	%empty;

task_func_args_opt:
	TOK_LPAREN TOK_RPAREN | %empty | TOK_LPAREN {
		extra->albuf = nullptr;
		extra->astbuf1 = nullptr;
		extra->astbuf2 = nullptr;
	} task_func_args optional_comma {
		(void)extra->astbuf1.reset();
		if (extra->astbuf2 != nullptr)
			(void)extra->astbuf2.reset();
		extra->albuf.reset();
	} TOK_RPAREN;

task_func_args:
	task_func_port | task_func_args TOK_COMMA task_func_port;

task_func_port:
	attr wire_type range_or_multirange {
		bool prev_was_input = true;
		bool prev_was_output = false;
		if (extra->albuf) {
			prev_was_input = extra->astbuf1->is_input;
			prev_was_output = extra->astbuf1->is_output;
			(void)extra->astbuf1.reset();
			if (extra->astbuf2 != nullptr)
				(void)extra->astbuf2.reset();
			extra->albuf.reset();
		}
		extra->albuf = std::move($1);
		extra->astbuf1 = std::move($2);
		extra->astbuf2 = checkRange(extra->astbuf1.get(), std::move($3));
		if (!extra->astbuf1->is_input && !extra->astbuf1->is_output) {
			if (!mode->sv)
				err_at_loc(@2, "task/function argument direction missing");
			extra->astbuf1->is_input = prev_was_input;
			extra->astbuf1->is_output = prev_was_output;
		}
	} wire_name port_default |
	{
		if (!extra->astbuf1) {
			if (!mode->sv)
				err_at_loc(@$, "task/function argument direction missing");
			extra->albuf = std::make_unique<dict<IdString, std::unique_ptr<AstNode>>>();
			extra->astbuf1 = std::make_unique<AstNode>(@$, AST_WIRE);
			extra->current_wire_rand = false;
			extra->current_wire_const = false;
			extra->astbuf1->is_input = true;
			extra->astbuf2 = nullptr;
		}
	} wire_name port_default;

port_default:
	%empty |
	TOK_EQ expr {
		if (extra->current_function_or_task == nullptr || extra->current_function_or_task->children.empty())
			err_at_loc(@1, "Default argument value is not on a subroutine port.");
		extra->current_function_or_task->children.back()->attributes[RTLIL::IdString("\\sv_default")] = std::move($2);
	};

task_func_body:
	task_func_body behavioral_stmt |
	task_func_body attr TOK_AT TOK_LPAREN {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} always_events TOK_RPAREN TOK_SEMICOL {
		extra->ast_stack.pop_back();
		extra->ast_stack.back()->children.pop_back();
	} |
	%empty;

/*************************** specify parser ***************************/

specify_block:
	TOK_SPECIFY specify_item_list TOK_ENDSPECIFY;

specify_item_list:
	specify_item specify_item_list |
	%empty;

specify_item:
	specify_if TOK_LPAREN specify_edge expr TOK_SPECIFY_OPER specify_target TOK_RPAREN TOK_EQ specify_rise_fall TOK_SEMICOL {
		auto en_expr = std::move($1);
		char specify_edge = $3;
		auto src_expr = std::move($4);
		string *oper = $5.get();
		specify_target_ptr_t target = std::move($6);
		specify_rise_fall_ptr_t timing = std::move($9);

		if (specify_edge != 0 && target->dat == nullptr)
			err_at_loc(@3, "Found specify edge but no data spec.");

		auto cell_owned = std::make_unique<AstNode>(@$, AST_CELL);
		auto cell = cell_owned.get();
		extra->ast_stack.back()->children.push_back(std::move(cell_owned));
		cell->str = stringf("$specify$%d", autoidx++);
		cell->children.push_back(std::make_unique<AstNode>(@$, AST_CELLTYPE));
		cell->children.back()->str = target->dat ? "$specify3" : "$specify2";
		SET_AST_NODE_LOC(cell, en_expr.get() ? @1 : @2, @10);

		char oper_polarity = 0;
		char oper_type = oper->at(0);

		if (oper->size() == 3) {
			oper_polarity = oper->at(0);
			oper_type = oper->at(1);
		}

		cell->children.push_back(std::make_unique<AstNode>(@5, AST_PARASET, AstNode::mkconst_int(@5, oper_type == '*', false, 1)));
		cell->children.back()->str = "\\FULL";

		cell->children.push_back(std::make_unique<AstNode>(@5, AST_PARASET, AstNode::mkconst_int(@5, oper_polarity != 0, false, 1)));
		cell->children.back()->str = "\\SRC_DST_PEN";

		cell->children.push_back(std::make_unique<AstNode>(@5, AST_PARASET, AstNode::mkconst_int(@5, oper_polarity == '+', false, 1)));
		cell->children.back()->str = "\\SRC_DST_POL";

		cell->children.push_back(std::make_unique<AstNode>(@9, AST_PARASET, std::move(timing->rise.t_min)));
		cell->children.back()->str = "\\T_RISE_MIN";

		cell->children.push_back(std::make_unique<AstNode>(@9, AST_PARASET, std::move(timing->rise.t_avg)));
		cell->children.back()->str = "\\T_RISE_TYP";

		cell->children.push_back(std::make_unique<AstNode>(@9, AST_PARASET, std::move(timing->rise.t_max)));
		cell->children.back()->str = "\\T_RISE_MAX";

		cell->children.push_back(std::make_unique<AstNode>(@9, AST_PARASET, std::move(timing->fall.t_min)));
		cell->children.back()->str = "\\T_FALL_MIN";

		cell->children.push_back(std::make_unique<AstNode>(@9, AST_PARASET, std::move(timing->fall.t_avg)));
		cell->children.back()->str = "\\T_FALL_TYP";

		cell->children.push_back(std::make_unique<AstNode>(@9, AST_PARASET, std::move(timing->fall.t_max)));
		cell->children.back()->str = "\\T_FALL_MAX";

		cell->children.push_back(std::make_unique<AstNode>(@1, AST_ARGUMENT, en_expr ? std::move(en_expr) : AstNode::mkconst_int(@1, 1, false, 1)));
		cell->children.back()->str = "\\EN";

		cell->children.push_back(std::make_unique<AstNode>(@4, AST_ARGUMENT, std::move(src_expr)));
		cell->children.back()->str = "\\SRC";

		cell->children.push_back(std::make_unique<AstNode>(@6, AST_ARGUMENT, std::move(target->dst)));
		cell->children.back()->str = "\\DST";

		if (target->dat)
		{
			cell->children.push_back(std::make_unique<AstNode>(@3, AST_PARASET, AstNode::mkconst_int(@3, specify_edge != 0, false, 1)));
			cell->children.back()->str = "\\EDGE_EN";

			cell->children.push_back(std::make_unique<AstNode>(@3, AST_PARASET, AstNode::mkconst_int(@3, specify_edge == 'p', false, 1)));
			cell->children.back()->str = "\\EDGE_POL";

			cell->children.push_back(std::make_unique<AstNode>(@6, AST_PARASET, AstNode::mkconst_int(@6, target->polarity_op != 0, false, 1)));
			cell->children.back()->str = "\\DAT_DST_PEN";

			cell->children.push_back(std::make_unique<AstNode>(@6, AST_PARASET, AstNode::mkconst_int(@6, target->polarity_op == '+', false, 1)));
			cell->children.back()->str = "\\DAT_DST_POL";

			cell->children.push_back(std::make_unique<AstNode>(@6, AST_ARGUMENT, std::move(target->dat)));
			cell->children.back()->str = "\\DAT";
		}
	} |
	TOK_ID TOK_LPAREN specify_edge expr specify_condition TOK_COMMA specify_edge expr specify_condition TOK_COMMA specify_triple specify_opt_triple TOK_RPAREN TOK_SEMICOL {
		if (*$1 != "$setup" && *$1 != "$hold" && *$1 != "$setuphold" && *$1 != "$removal" && *$1 != "$recovery" &&
				*$1 != "$recrem" && *$1 != "$skew" && *$1 != "$timeskew" && *$1 != "$fullskew" && *$1 != "$nochange")
			err_at_loc(@1, "Unsupported specify rule type: %s", *$1);

		auto src_pen = AstNode::mkconst_int(@3, $3 != 0, false, 1);
		auto src_pol = AstNode::mkconst_int(@3, $3 == 'p', false, 1);
		auto src_expr = std::move($4), src_en = $5 ? std::move($5) : AstNode::mkconst_int(@5, 1, false, 1);

		auto dst_pen = AstNode::mkconst_int(@7, $7 != 0, false, 1);
		auto dst_pol = AstNode::mkconst_int(@7, $7 == 'p', false, 1);
		auto dst_expr = std::move($8), dst_en = $9 ? std::move($9) : AstNode::mkconst_int(@5, 1, false, 1);

		specify_triple_ptr_t limit = std::move($11);
		specify_triple_ptr_t limit2 = std::move($12);

		auto cell_owned = std::make_unique<AstNode>(@$, AST_CELL);
		auto cell = cell_owned.get();
		extra->ast_stack.back()->children.push_back(std::move(cell_owned));
		cell->str = stringf("$specify$%d", autoidx++);
		cell->children.push_back(std::make_unique<AstNode>(@$, AST_CELLTYPE));
		cell->children.back()->str = "$specrule";
		SET_AST_NODE_LOC(cell, @1, @14);

		cell->children.push_back(std::make_unique<AstNode>(@1, AST_PARASET, AstNode::mkconst_str(@1, *$1)));
		cell->children.back()->str = "\\TYPE";

		cell->children.push_back(std::make_unique<AstNode>(@11, AST_PARASET, std::move(limit->t_min)));
		cell->children.back()->str = "\\T_LIMIT_MIN";

		cell->children.push_back(std::make_unique<AstNode>(@11, AST_PARASET, std::move(limit->t_avg)));
		cell->children.back()->str = "\\T_LIMIT_TYP";

		cell->children.push_back(std::make_unique<AstNode>(@11, AST_PARASET, std::move(limit->t_max)));
		cell->children.back()->str = "\\T_LIMIT_MAX";

		cell->children.push_back(std::make_unique<AstNode>(@12, AST_PARASET, limit2 ? std::move(limit2->t_min) : AstNode::mkconst_int(@12, 0, true)));
		cell->children.back()->str = "\\T_LIMIT2_MIN";

		cell->children.push_back(std::make_unique<AstNode>(@12, AST_PARASET, limit2 ? std::move(limit2->t_avg) : AstNode::mkconst_int(@12, 0, true)));
		cell->children.back()->str = "\\T_LIMIT2_TYP";

		cell->children.push_back(std::make_unique<AstNode>(@12, AST_PARASET, limit2 ? std::move(limit2->t_max) : AstNode::mkconst_int(@12, 0, true)));
		cell->children.back()->str = "\\T_LIMIT2_MAX";

		cell->children.push_back(std::make_unique<AstNode>(@3, AST_PARASET, std::move(src_pen)));
		cell->children.back()->str = "\\SRC_PEN";

		cell->children.push_back(std::make_unique<AstNode>(@3, AST_PARASET, std::move(src_pol)));
		cell->children.back()->str = "\\SRC_POL";

		cell->children.push_back(std::make_unique<AstNode>(@3, AST_PARASET, std::move(dst_pen)));
		cell->children.back()->str = "\\DST_PEN";

		cell->children.push_back(std::make_unique<AstNode>(@7, AST_PARASET, std::move(dst_pol)));
		cell->children.back()->str = "\\DST_POL";

		cell->children.push_back(std::make_unique<AstNode>(@5, AST_ARGUMENT, std::move(src_en)));
		cell->children.back()->str = "\\SRC_EN";

		cell->children.push_back(std::make_unique<AstNode>(@4, AST_ARGUMENT, std::move(src_expr)));
		cell->children.back()->str = "\\SRC";

		cell->children.push_back(std::make_unique<AstNode>(@9, AST_ARGUMENT, std::move(dst_en)));
		cell->children.back()->str = "\\DST_EN";

		cell->children.push_back(std::make_unique<AstNode>(@8, AST_ARGUMENT, std::move(dst_expr)));
		cell->children.back()->str = "\\DST";
	};

specify_opt_triple:
	TOK_COMMA specify_triple {
		$$ = std::move($2);
	} |
	%empty {
		$$ = nullptr;
	};

specify_if:
	TOK_IF TOK_LPAREN expr TOK_RPAREN {
		$$ = std::move($3);
	} |
	%empty {
		$$ = nullptr;
	};

specify_condition:
	TOK_SPECIFY_AND expr {
		$$ = std::move($2);
	} |
	%empty {
		$$ = nullptr;
	};

specify_target:
	expr {
		$$ = std::make_unique<struct specify_target>();
		$$->polarity_op = 0;
		$$->dst = std::move($1);
		$$->dat = nullptr;
	} |
	TOK_LPAREN expr TOK_COL expr TOK_RPAREN{
		$$ = std::make_unique<struct specify_target>();
		$$->polarity_op = 0;
		$$->dst = std::move($2);
		$$->dat = std::move($4);
	} |
	TOK_LPAREN expr TOK_NEG_INDEXED expr TOK_RPAREN{
		$$ = std::make_unique<struct specify_target>();
		$$->polarity_op = '-';
		$$->dst = std::move($2);
		$$->dat = std::move($4);
	} |
	TOK_LPAREN expr TOK_POS_INDEXED expr TOK_RPAREN{
		$$ = std::make_unique<struct specify_target>();
		$$->polarity_op = '+';
		$$->dst = std::move($2);
		$$->dat = std::move($4);
	};

specify_edge:
	TOK_POSEDGE { $$ = 'p'; } |
	TOK_NEGEDGE { $$ = 'n'; } |
	%empty { $$ = 0; };

specify_rise_fall:
	specify_triple {
		$$ = std::make_unique<struct specify_rise_fall>();
		$$->fall.t_min = $1->t_min->clone();
		$$->fall.t_avg = $1->t_avg->clone();
		$$->fall.t_max = $1->t_max->clone();
		$$->rise = std::move(*$1);
	} |
	TOK_LPAREN specify_triple TOK_COMMA specify_triple TOK_RPAREN {
		$$ = std::make_unique<struct specify_rise_fall>();
		$$->rise = std::move(*$2);
		$$->fall = std::move(*$4);
	} |
	TOK_LPAREN specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_RPAREN {
		$$ = std::make_unique<struct specify_rise_fall>();
		$$->rise = std::move(*$2);
		$$->fall = std::move(*$4);
		warn_at_loc(@$, "Path delay expressions beyond rise/fall not currently supported. Ignoring.");
	} |
	TOK_LPAREN specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_RPAREN {
		$$ = std::make_unique<struct specify_rise_fall>();
		$$->rise = std::move(*$2);
		$$->fall = std::move(*$4);
		warn_at_loc(@$, "Path delay expressions beyond rise/fall not currently supported. Ignoring.");
	} |
	TOK_LPAREN specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_COMMA specify_triple TOK_RPAREN {
		$$ = std::make_unique<struct specify_rise_fall>();
		$$->rise = std::move(*$2);
		$$->fall = std::move(*$4);
		warn_at_loc(@$, "Path delay expressions beyond rise/fall not currently supported. Ignoring.");
	}

specify_triple:
	expr {
		$$ = std::make_unique<struct specify_triple>();
		$$->t_avg = $1->clone();
		$$->t_max = $1->clone();
		$$->t_min = std::move($1);
	} |
	expr TOK_COL expr TOK_COL expr {
		$$ = std::make_unique<struct specify_triple>();
		$$->t_min = std::move($1);
		$$->t_avg = std::move($3);
		$$->t_max = std::move($5);
	};

/******************** ignored specify parser **************************/

ignored_specify_block:
	TOK_IGNORED_SPECIFY ignored_specify_item_opt TOK_ENDSPECIFY |
	TOK_IGNORED_SPECIFY TOK_ENDSPECIFY ;

ignored_specify_item_opt:
	ignored_specify_item_opt ignored_specify_item |
	ignored_specify_item ;

ignored_specify_item:
	specparam_declaration
	// | pulsestyle_declaration
	// | showcancelled_declaration
	| path_declaration
	| system_timing_declaration
	;

specparam_declaration:
	TOK_SPECPARAM list_of_specparam_assignments TOK_SEMICOL |
	TOK_SPECPARAM specparam_range list_of_specparam_assignments TOK_SEMICOL ;

// IEEE 1364-2005 calls this sinmply 'range' but the current 'range' rule allows empty match
// and the 'non_opt_range' rule allows index ranges not allowed by 1364-2005
// exxxxtending this for SV specparam would change this anyhow
specparam_range:
	TOK_LBRA ignspec_constant_expression TOK_COL ignspec_constant_expression TOK_RBRA ;

list_of_specparam_assignments:
	specparam_assignment | list_of_specparam_assignments TOK_COMMA specparam_assignment;

specparam_assignment:
	ignspec_id TOK_EQ ignspec_expr ;

path_declaration :
	simple_path_declaration TOK_SEMICOL
	// | edge_sensitive_path_declaration
	| state_dependent_path_declaration TOK_SEMICOL
	;

simple_path_declaration :
	parallel_path_description TOK_EQ path_delay_value |
	full_path_description TOK_EQ path_delay_value
	;

state_dependent_path_declaration:
	TOK_IF TOK_LPAREN module_path_expression TOK_RPAREN simple_path_declaration
	// | TOK_IF TOK_LPAREN module_path_expression TOK_RPAREN edge_sensitive_path_declaration
	| TOK_IFNONE simple_path_declaration
	;

module_path_expression:
	module_path_primary
	// Flatten out unary_operator to avoid shift/reduce conflict
	| TOK_EXCL attr module_path_primary
	| TOK_TILDE attr module_path_primary
	| TOK_AMP attr module_path_primary
	| OP_NAND attr module_path_primary
	| TOK_PIPE attr module_path_primary
	| OP_NOR attr module_path_primary
	| TOK_CARET attr module_path_primary
	| OP_XNOR attr module_path_primary
	// Flatten out binary_operator to avoid shift/reduce conflict
	| module_path_expression OP_EQ attr module_path_expression
	| module_path_expression OP_NE attr module_path_expression
	| module_path_expression OP_LAND attr module_path_expression
	| module_path_expression OP_LOR attr module_path_expression
	| module_path_expression TOK_AMP attr module_path_expression
	| module_path_expression TOK_PIPE attr module_path_expression
	| module_path_expression TOK_CARET attr module_path_expression
	| module_path_expression OP_XNOR attr module_path_expression
	// | module_path_conditional_expression
	;

module_path_primary:
	number
	| TOK_ID
	// Deviate from specification: Normally string would not be allowed, however they are necessary for the ecp5 tests
	| TOK_STRING
	// | module_path_concatenation
	// | module_path_multiple_concatenation
	// | function_subroutine_call
	// | TOK_LPAREN module_path_minmax_expression TOK_RPAREN
	;

number:
	integral_number
	| TOK_REALVAL
	;

path_delay_value :
	TOK_LPAREN ignspec_expr list_of_path_delay_extra_expressions TOK_RPAREN
	|     ignspec_expr
	|     ignspec_expr list_of_path_delay_extra_expressions
	;

list_of_path_delay_extra_expressions :
	TOK_COMMA ignspec_expr
	| TOK_COMMA ignspec_expr list_of_path_delay_extra_expressions
	;

specify_edge_identifier :
	TOK_POSEDGE | TOK_NEGEDGE ;

parallel_path_description :
	TOK_LPAREN specify_input_terminal_descriptor opt_polarity_operator TOK_EQ TOK_GT specify_output_terminal_descriptor TOK_RPAREN |
	TOK_LPAREN specify_edge_identifier specify_input_terminal_descriptor TOK_EQ TOK_GT TOK_LPAREN specify_output_terminal_descriptor opt_polarity_operator TOK_COL ignspec_expr TOK_RPAREN TOK_RPAREN |
	TOK_LPAREN specify_edge_identifier specify_input_terminal_descriptor TOK_EQ TOK_GT TOK_LPAREN specify_output_terminal_descriptor TOK_POS_INDEXED ignspec_expr TOK_RPAREN TOK_RPAREN ;

full_path_description :
	TOK_LPAREN list_of_path_inputs TOK_ASTER TOK_GT list_of_path_outputs TOK_RPAREN |
	TOK_LPAREN specify_edge_identifier list_of_path_inputs TOK_ASTER TOK_GT TOK_LPAREN list_of_path_outputs opt_polarity_operator TOK_COL ignspec_expr TOK_RPAREN TOK_RPAREN |
	TOK_LPAREN specify_edge_identifier list_of_path_inputs TOK_ASTER TOK_GT TOK_LPAREN list_of_path_outputs TOK_POS_INDEXED ignspec_expr TOK_RPAREN TOK_RPAREN ;

// This was broken into 2 rules to solve shift/reduce conflicts
list_of_path_inputs :
	specify_input_terminal_descriptor                  opt_polarity_operator  |
	specify_input_terminal_descriptor more_path_inputs opt_polarity_operator ;

more_path_inputs :
    TOK_COMMA specify_input_terminal_descriptor |
    more_path_inputs TOK_COMMA specify_input_terminal_descriptor ;

list_of_path_outputs :
	specify_output_terminal_descriptor |
	list_of_path_outputs TOK_COMMA specify_output_terminal_descriptor ;

opt_polarity_operator :
	TOK_PLUS | TOK_MINUS | %empty;

// Good enough for the time being
specify_input_terminal_descriptor :
	ignspec_id ;

// Good enough for the time being
specify_output_terminal_descriptor :
	ignspec_id ;

system_timing_declaration :
	ignspec_id TOK_LPAREN system_timing_args TOK_RPAREN TOK_SEMICOL ;

system_timing_arg :
	TOK_POSEDGE ignspec_id |
	TOK_NEGEDGE ignspec_id |
	ignspec_expr ;

system_timing_args :
	system_timing_arg |
	system_timing_args TOK_IGNORED_SPECIFY_AND system_timing_arg |
	system_timing_args TOK_COMMA system_timing_arg ;

// for the time being this is OK, but we may write our own expr here.
// as I'm not sure it is legal to use a full expr here (probably not)
// On the other hand, other rules requiring constant expressions also use 'expr'
// (such as param assignment), so we may leave this as-is, perhaps adding runtime checks for constant-ness
ignspec_constant_expression:
	expr {  };

ignspec_expr:
	expr {  } |
	expr TOK_COL expr TOK_COL expr {
	};

ignspec_id:
	TOK_ID {  }
	range_or_multirange {  };

/**********************************************************************/

param_signed:
	TOK_SIGNED {
		extra->astbuf1->is_signed = true;
	} | TOK_UNSIGNED {
		extra->astbuf1->is_signed = false;
	} | %empty;

param_integer:
	type_atom {
		extra->astbuf1->is_reg = false;
	};

param_real:
	TOK_REAL {
		extra->astbuf1->children.push_back(std::make_unique<AstNode>(@$, AST_REALVALUE));
	};

param_range:
	range {
		if ($1 != nullptr) {
			extra->astbuf1->children.push_back(std::move($1));
		}
	};

param_integer_type: param_integer param_signed;
param_packed:
	non_opt_range { $$ = std::move($1); } |
	param_packed non_opt_range {
		if ($1->type == AST_MULTIRANGE) {
			$$ = std::move($1);
			$$->children.push_back(std::move($2));
		} else {
			$$ = std::make_unique<AstNode>(@$, AST_MULTIRANGE, std::move($1), std::move($2));
		}
	};

param_range_type:
	type_vec param_signed {
		addRange(extra->astbuf1.get(), 0, 0);
	} |
	type_vec param_signed param_packed {
		extra->astbuf1->children.push_back(std::move($3));
	};
param_implicit_type: param_signed param_range;

param_type:
	param_integer_type | param_real | param_range_type | param_implicit_type |
	hierarchical_type_id range_or_multirange {
		extra->addWiretypeNode($1.get(), extra->astbuf1.get());
		if ($2)
			extra->astbuf1->children.push_back(std::move($2));
	} |
	TOK_SCOPED_ID range_or_multirange {
		extra->addWiretypeNode($1.get(), extra->astbuf1.get());
		if ($2)
			extra->astbuf1->children.push_back(std::move($2));
	};

param_type_rhs:
	typedef_base_type range_or_multirange {
		auto base = std::move($1);
		if ($2) {
			auto range = checkRange(base.get(), std::move($2));
			if (range)
				base->children.push_back(std::move(range));
		}
		$$ = std::move(base);
	} |
	enum_struct_type {
		$$ = std::move($1);
	} |
	TOK_SCOPED_ID {
		$$ = std::make_unique<AstNode>(@$, AST_WIRE);
		$$->is_logic = true;
		extra->addWiretypeNode($1.get(), $$.get());
	} |
	TOK_ID {
		// type name without typedef-usage lookahead (e.g. after `='), so
		// the lexer delivered a plain identifier
		$$ = std::make_unique<AstNode>(@$, AST_WIRE);
		$$->is_logic = true;
		extra->addWiretypeNode($1.get(), $$.get());
	} |
	TOK_TYPE TOK_LPAREN type_operand TOK_RPAREN {
		// `type(...)' (IEEE 1800-2009 6.23): opaque alias
		$$ = std::make_unique<AstNode>(@$, AST_WIRE);
		$$->is_logic = true;
	} |
	TOK_PKG_USER_TYPE TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN |
	TOK_USER_TYPE TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN |
	TOK_SCOPED_ID TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN {
		// parameterized class type as a parameter default: opaque alias
		$$ = std::make_unique<AstNode>(@$, AST_WIRE);
		$$->is_logic = true;
	};

param_type_decl_list:
	TOK_ID TOK_EQ param_type_rhs {
		extra->addTypeParamNode($1.get(), std::move($3));
	} |
	param_type_decl_list TOK_COMMA TOK_ID TOK_EQ param_type_rhs {
		extra->addTypeParamNode($3.get(), std::move($5));
	};

param_decl:
	attr TOK_PARAMETER {
		extra->astbuf1 = std::make_unique<AstNode>(@$, AST_PARAMETER);
		extra->astbuf1->children.push_back(AstNode::mkconst_int(@$, 0, true));
		append_attr(extra->astbuf1.get(), std::move($1));
	} param_type param_decl_list TOK_SEMICOL {
		(void)extra->astbuf1.reset();
	} |
	attr TOK_PARAMETER TOK_TYPE param_type_decl_list TOK_SEMICOL {
	} |
	attr TOK_PARAMETER TOK_TIME TOK_ID TOK_EQ expr TOK_SEMICOL {
		auto node = std::make_unique<AstNode>(@$, AST_PARAMETER);
		node->str = *$4;
		node->children.push_back(std::move($6));
		append_attr(node.get(), std::move($1));
		extra->ast_stack.back()->children.push_back(std::move(node));
	};

localparam_decl:
	attr TOK_LOCALPARAM {
		extra->astbuf1 = std::make_unique<AstNode>(@$, AST_LOCALPARAM);
		extra->astbuf1->children.push_back(AstNode::mkconst_int(@$, 0, true));
		append_attr(extra->astbuf1.get(), std::move($1));
	} param_type param_decl_list TOK_SEMICOL {
		(void)extra->astbuf1.reset();
	} |
	attr TOK_LOCALPARAM TOK_TYPE param_type_decl_list TOK_SEMICOL {
	};

param_decl_list:
	single_param_decl | param_decl_list TOK_COMMA single_param_decl;

single_param_decl:
	single_param_decl_ident TOK_EQ expr {
		AstNode *decl = extra->ast_stack.back()->children.back().get();
		log_assert(decl->type == AST_PARAMETER || decl->type == AST_LOCALPARAM);
		decl->children[0] = std::move($3);
	} |
	single_param_decl_ident {
		AstNode *decl = extra->ast_stack.back()->children.back().get();
		if (decl->type != AST_PARAMETER) {
			log_assert(decl->type == AST_LOCALPARAM);
			err_at_loc(@1, "localparam initialization is missing!");
		}
		if (!mode->sv)
			err_at_loc(@1, "Parameter defaults can only be omitted in SystemVerilog mode!");
		// Keep a placeholder value when a packed range is also stored.
		if (decl->children.size() <= 1)
			decl->children.erase(decl->children.begin());
	};

param_unpacked:
	%empty { $$ = nullptr; } |
	non_opt_range { $$ = std::move($1); } |
	non_opt_multirange { $$ = std::move($1); };

single_param_decl_ident:
	TOK_ID param_unpacked {
		std::unique_ptr<AstNode> node_owned;
		if (extra->astbuf1 == nullptr) {
			if (!mode->sv)
				err_at_loc(@1, "In pure Verilog (not SystemVerilog), parameter/localparam with an initializer must use the parameter/localparam keyword");
			node_owned = std::make_unique<AstNode>(@$, AST_PARAMETER);
			node_owned->children.push_back(AstNode::mkconst_int(@$, 0, true));
		} else {
			node_owned = extra->astbuf1->clone();
		}
		node_owned->str = *$1;
		auto node = node_owned.get();
		extra->ast_stack.back()->children.push_back(std::move(node_owned));
		SET_AST_NODE_LOC(node, @1, @1);
		// unpacked dimensions (`localparam logic [7:0] M [4:6] = '{..}`):
		// simplify turns them into the node's dimensions, which the
		// array-read resolver and $low/$high/$size read
		if ($2)
			node->set_attribute(ID(sv_unpacked_dims), std::move($2));
	};

defparam_decl:
	TOK_DEFPARAM defparam_decl_list TOK_SEMICOL;

defparam_decl_list:
	single_defparam_decl | defparam_decl_list TOK_COMMA single_defparam_decl;

single_defparam_decl:
	range rvalue TOK_EQ expr {
		auto node = std::make_unique<AstNode>(@$, AST_DEFPARAM);
		node->children.push_back(std::move($2));
		node->children.push_back(std::move($4));
		if ($1 != nullptr)
			node->children.push_back(std::move($1));
		extra->ast_stack.back()->children.push_back(std::move(node));
	};

/////////
// enum
/////////

enum_type: TOK_ENUM {
		static int enum_count;
		// create parent node for the enum
		extra->astbuf2 = std::make_unique<AstNode>(@$, AST_ENUM);
		extra->astbuf2->str = std::string("$enum");
		extra->astbuf2->str += std::to_string(enum_count++);
		log_assert(!extra->cell_hack);
		extra->cell_hack = extra->astbuf2.get();
		extra->ast_stack.back()->children.push_back(std::move(extra->astbuf2));
		// create the template for the names
		extra->astbuf1 = std::make_unique<AstNode>(@$, AST_ENUM_ITEM);
		extra->astbuf1->children.push_back(AstNode::mkconst_int(@$, 0, true));
	} enum_base_type TOK_LCURL enum_name_list optional_comma TOK_RCURL {
		// create template for the enum vars
		log_assert(extra->cell_hack);
		auto tnode_owned = extra->astbuf1->clone();
		auto* tnode = tnode_owned.get();
		extra->astbuf1 = std::move(tnode_owned);
		tnode->type = AST_WIRE;
		tnode->attributes[ID::enum_type] = AstNode::mkconst_str(@$, extra->cell_hack->str);
		extra->cell_hack = nullptr;
		// drop constant but keep any range
		tnode->children.erase(tnode->children.begin());
		$$ = extra->astbuf1->clone();
	};

enum_base_type: type_atom type_signing
	| type_vec type_signing range	{ if ($3) extra->astbuf1->children.push_back(std::move($3)); }
	| hierarchical_type_id		{ extra->addWiretypeNode($1.get(), extra->astbuf1.get()); }
	| TOK_SCOPED_ID			{ extra->addWiretypeNode($1.get(), extra->astbuf1.get()); }
	| %empty			{ extra->astbuf1->is_reg = true; addRange(extra->astbuf1.get()); }
	;

type_atom:
	integer_atom_type {
		// astbuf1 can be unset when error recovery parses a type in a
		// context where struct_member_type never ran.
		if (extra->astbuf1) {
			extra->astbuf1->is_reg = true;
			extra->astbuf1->is_signed = true;
			addRange(extra->astbuf1.get(), $1 - 1, 0);
		}
	} |
	TOK_TIME {
		if (extra->astbuf1) {
			extra->astbuf1->is_reg = true;
			extra->astbuf1->range_left = 63;
			extra->astbuf1->range_right = 0;
		}
	};

type_vec: TOK_REG		{ if (extra->astbuf1) extra->astbuf1->is_reg   = true; }		// unsigned
	| TOK_LOGIC		{ if (extra->astbuf1) extra->astbuf1->is_logic = true; }		// unsigned
	;

type_signing:
	  TOK_SIGNED		{ if (extra->astbuf1) extra->astbuf1->is_signed = true; }
	| TOK_UNSIGNED		{ if (extra->astbuf1) extra->astbuf1->is_signed = false; }
	| %empty
	;

enum_name_list: enum_name_decl
	| enum_name_list TOK_COMMA enum_name_decl
	;

enum_name_decl:
	TOK_ID opt_enum_init {
		// put in fn
		log_assert((bool)extra->astbuf1);
		log_assert((bool)extra->cell_hack);
		auto node = extra->astbuf1->clone();
		node->str = *$1;
		SET_AST_NODE_LOC(node.get(), @1, @1);
		node->children[0] = $2 ? std::move($2) : std::make_unique<AstNode>(@$, AST_NONE);
		extra->cell_hack->children.push_back(std::move(node));
	} |
	TOK_ID TOK_LBRA expr TOK_RBRA {
		// IEEE 1800-2009 6.19: `name[N]` enumerates name0..nameN-1 with
		// values continuing by auto-increment
		log_assert((bool)extra->astbuf1);
		log_assert((bool)extra->cell_hack);
		if ($3->type == AST_CONSTANT) {
			int n = $3->asInt(true);
			for (int i = 0; i < n; i++) {
				auto node = extra->astbuf1->clone();
				node->str = *$1 + stringf("%d", i);
				SET_AST_NODE_LOC(node.get(), @1, @4);
				node->children[0] = std::make_unique<AstNode>(@$, AST_NONE);
				extra->cell_hack->children.push_back(std::move(node));
			}
		} else {
			// non-constant count: keep the base name only
			auto node = extra->astbuf1->clone();
			node->str = *$1;
			SET_AST_NODE_LOC(node.get(), @1, @1);
			node->children[0] = std::make_unique<AstNode>(@$, AST_NONE);
			extra->cell_hack->children.push_back(std::move(node));
		}
	} |
	TOK_ID TOK_LBRA expr TOK_COL expr TOK_RBRA {
		// IEEE 1800-2009 6.19: `name[a:b]` enumerates namea..nameb with
		// values continuing by auto-increment
		log_assert((bool)extra->astbuf1);
		log_assert((bool)extra->cell_hack);
		if ($3->type == AST_CONSTANT && $5->type == AST_CONSTANT) {
			int a = $3->asInt(true), b = $5->asInt(true);
			int step = a <= b ? 1 : -1;
			for (int i = a; ; i += step) {
				auto node = extra->astbuf1->clone();
				node->str = *$1 + stringf("%d", i);
				SET_AST_NODE_LOC(node.get(), @1, @6);
				node->children[0] = std::make_unique<AstNode>(@$, AST_NONE);
				extra->cell_hack->children.push_back(std::move(node));
				if (i == b)
					break;
			}
		} else {
			auto node = extra->astbuf1->clone();
			node->str = *$1;
			SET_AST_NODE_LOC(node.get(), @1, @1);
			node->children[0] = std::make_unique<AstNode>(@$, AST_NONE);
			extra->cell_hack->children.push_back(std::move(node));
		}
	}
	;

opt_enum_init:
	TOK_EQ basic_expr		{ $$ = std::move($2); }	// TODO: restrict this
	| %empty		{ $$ = nullptr; }
	;

enum_var_list:
	enum_var
	| enum_var_list TOK_COMMA enum_var
	;

enum_var: TOK_ID {
		log_assert((bool)extra->astbuf1);
		auto node = extra->astbuf1->clone();
		node->str = *$1;
		SET_AST_NODE_LOC(node.get(), @1, @1);
		node->is_enum = true;
		extra->ast_stack.back()->children.push_back(std::move(node));
	}
	;

enum_decl: enum_type enum_var_list TOK_SEMICOL		{  }
	;

//////////////////
// struct or union
//////////////////

struct_decl:
	attr struct_type {
		append_attr(extra->astbuf2.get(), std::move($1));
	} range_or_multirange {
		extra->astbuf3 = std::move($4);
	} struct_var_list TOK_SEMICOL {
		(void)extra->astbuf2.reset();
		(void)extra->astbuf3.reset();
		extra->anon_struct_type.clear();
	}
	;

struct_type:
	struct_union {
		extra->astbuf2 = std::move($1);
		extra->astbuf2->is_custom_type = true;
		extra->pending_struct_defaults.clear();
	} opt_tagged {
		extra->struct_defaults_stack.emplace_back();
	} struct_body {
		extra->pending_struct_defaults = std::move(extra->struct_defaults_stack.back());
		extra->struct_defaults_stack.pop_back();
		if (!extra->pending_struct_defaults.empty()) {
			// IEEE 1800-2009 7.2.2: individual member defaults are only
			// allowed in unpacked structures without union members
			if (!extra->astbuf2->attributes.count(ID(sv_unpacked)))
				err_at_loc(@1, "Members of packed structures shall not be assigned individual default member values.");
			for (auto &member : extra->astbuf2->children)
				if (member->type == AST_UNION)
					err_at_loc(@1, "Members of unpacked structures containing a union shall not be assigned individual default member values.");
		}
		$$ = extra->astbuf2->clone();
	}
	;

opt_tagged:
	%empty |
	TOK_TAGGED {
		extra->astbuf2->set_attribute(ID(sv_tagged), AstNode::mkconst_int(@$, 1, false));
	}
	;

struct_union:
	  TOK_STRUCT		{ $$ = std::make_unique<AstNode>(@$, AST_STRUCT); }
	| TOK_UNION		{ $$ = std::make_unique<AstNode>(@$, AST_UNION); }
	;

struct_body: opt_packed TOK_LCURL struct_member_list TOK_RCURL
	;

opt_packed:
	TOK_PACKED opt_signed_struct |
	%empty {
		warn_at_loc(@$, "Unpacked struct or union is treated as packed.");
		extra->astbuf2->set_attribute(ID(sv_unpacked), AstNode::mkconst_int(@$, 1, false));
	};

opt_signed_struct:
	  TOK_SIGNED		{ extra->astbuf2->is_signed = true; }
	| TOK_UNSIGNED		{ extra->astbuf2->is_signed = false; }
	| %empty // default is unsigned
	;

struct_member_list: struct_member
	| struct_member_list struct_member
	;

struct_member:
	struct_member_type member_name_list TOK_SEMICOL		{ (void)extra->astbuf1.reset(); } |
	TOK_RAND struct_member_type member_name_list TOK_SEMICOL	{ (void)extra->astbuf1.reset(); } |
	TOK_RANDC struct_member_type member_name_list TOK_SEMICOL	{ (void)extra->astbuf1.reset(); }
	;

member_name_list:
	  member_name
	| member_name_list TOK_COMMA member_name
	;

member_name: TOK_ID {
		extra->astbuf1->str = $1->substr(1);
		extra->astbuf3 = extra->astbuf1->clone();
		log_assert(!extra->member_hack);
		extra->member_hack = extra->astbuf3.get();
		SET_AST_NODE_LOC(extra->member_hack, @1, @1);
		extra->astbuf2->children.push_back(std::move(extra->astbuf3));
	} wire_name_dims opt_member_default {
		log_assert((bool)extra->member_hack);
		if ($3) extra->member_hack->children.push_back(std::move($3));
		extra->member_hack = nullptr;
	}
	;

opt_member_default:
	%empty |
	TOK_EQ expr {
		if (!extra->struct_defaults_stack.empty())
			extra->struct_defaults_stack.back().emplace_back(extra->member_hack->str, std::move($2));
	}
	;

struct_member_type: { extra->astbuf1 = std::make_unique<AstNode>(@$, AST_STRUCT_ITEM); } member_type_token
	;

member_type_token:
	member_type range_or_multirange {
		auto range = checkRange(extra->astbuf1.get(), std::move($2));
		if (range)
			extra->astbuf1->children.push_back(std::move(range));
	}
	| {
		(void)extra->astbuf1.reset();
	} struct_union {
			// stash state on extra->ast_stack
			// sketchy!
			extra->ast_stack.push_back(extra->astbuf2.release());
			extra->astbuf2 = std::move($2);
		} struct_body  {
			extra->astbuf1 = std::move(extra->astbuf2);
			// recover state
			extra->astbuf2.reset(extra->ast_stack.back());
			extra->ast_stack.pop_back();
		}
	;

member_type:
	TOK_VOID {
		// tagged-union void member: carries only the tag, no payload
		extra->astbuf1->set_attribute(ID(sv_void_member), AstNode::mkconst_int(@$, 1, false));
	} |
	type_atom type_signing
	| type_vec type_signing
	| hierarchical_type_id { extra->addWiretypeNode($1.get(), extra->astbuf1.get()); }
	| TOK_SCOPED_ID { extra->addWiretypeNode($1.get(), extra->astbuf1.get()); }
	;

struct_var_list: struct_var_and_opt_assign
	| struct_var_list TOK_COMMA struct_var_and_opt_assign
	;

struct_var_and_opt_assign:
	struct_var |
	struct_var TOK_EQ expr {
		auto wire = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		wire->str = extra->ast_stack.back()->children.back()->str;
		AstNode *scope = extra->ast_stack.back();
		if (scope->type == AST_MODULE || scope->type == AST_INTERFACE) {
			// a variable declaration assignment is the variable's initial
			// value (IEEE 1800-2017 6.8), as for `logic x = v;`; it was a
			// continuous assign, so the variable read as that constant and
			// its procedural driver was lost
			auto assign = std::make_unique<AstNode>(@$, AST_ASSIGN_LE, std::move(wire), std::move($3));
			SET_AST_NODE_LOC(assign.get(), @1, @3);
			auto block = std::make_unique<AstNode>(@$, AST_BLOCK, std::move(assign));
			SET_AST_NODE_LOC(block.get(), @1, @3);
			auto init = std::make_unique<AstNode>(@$, AST_INITIAL, std::move(block));
			SET_AST_NODE_LOC(init.get(), @1, @3);
			scope->children.push_back(std::move(init));
		} else {
			auto assign = std::make_unique<AstNode>(@$, AST_ASSIGN, std::move(wire), std::move($3));
			SET_AST_NODE_LOC(assign.get(), @1, @3);
			scope->children.push_back(std::move(assign));
		}
	};

struct_var:
	TOK_ID range_or_multirange {
		if (!extra->astbuf3 && $2 && (extra->astbuf2->type == AST_STRUCT || extra->astbuf2->type == AST_UNION)) {
			// `struct packed {...} q [N];`: an unpacked array of an
			// anonymous struct is declared as `typedef struct {...} T;
			// T q [N];`, so element member selects (`q[i].m`, i dynamic)
			// see the element type; the cloned struct made a memory whose
			// element type was lost ("Hierarchical prefix did not resolve")
			if (extra->anon_struct_type.empty()) {
				extra->anon_struct_type = stringf("\\__sv_anon_struct_%d", ++extra->anon_struct_count);
				auto td = std::make_unique<AstNode>(@$, AST_TYPEDEF, extra->astbuf2->clone());
				td->str = extra->anon_struct_type;
				extra->ast_stack.back()->children.push_back(std::move(td));
			}
			auto var_node = std::make_unique<AstNode>(@1, AST_WIRE);
			var_node->str = *$1;
			var_node->is_logic = true;
			for (auto &a : extra->astbuf2->attributes)
				var_node->set_attribute(a.first, a.second->clone());
			extra->addWiretypeNode(&extra->anon_struct_type, var_node.get());
			rewriteAsMemoryNode(var_node.get(), std::move($2));
			extra->ast_stack.back()->children.push_back(std::move(var_node));
			extra->emitStructDefaults(@$, extra->ast_stack.back(), *$1, extra->pending_struct_defaults);
		} else {
		auto var_node = extra->astbuf2->clone();
		var_node->str = *$1;
		SET_AST_NODE_LOC(var_node.get(), @1, @1);
		std::unique_ptr<AstNode> dims = extra->astbuf3 ? extra->astbuf3->clone() : nullptr;
		if ($2) {
			if (!dims)
				dims = std::move($2);
			else if (dims->type == AST_MULTIRANGE && $2->type == AST_MULTIRANGE) {
				for (auto &child : $2->children)
					dims->children.push_back(std::move(child));
			} else if (dims->type == AST_MULTIRANGE) {
				dims->children.push_back(std::move($2));
			} else {
				auto multi = std::make_unique<AstNode>(@$, AST_MULTIRANGE, std::move(dims), std::move($2));
				dims = std::move(multi);
			}
		}
		if (dims)
			rewriteAsMemoryNode(var_node.get(), std::move(dims));
		extra->ast_stack.back()->children.push_back(std::move(var_node));
		extra->emitStructDefaults(@$, extra->ast_stack.back(), *$1, extra->pending_struct_defaults);
		}
	};

/////////
// wire
/////////

wire_decl:
	attr wire_type range_or_multirange {
		extra->albuf = std::move($1);
		extra->astbuf1 = std::move($2);
		extra->astbuf2 = checkRange(extra->astbuf1.get(), std::move($3));
	} delay wire_name_list {
		(void)extra->astbuf1.reset();
		if (extra->astbuf2 != nullptr)
			(void)extra->astbuf2.reset();
		extra->albuf.reset();
	} TOK_SEMICOL |
	attr TOK_SUPPLY0 TOK_ID {
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_WIRE));
		extra->ast_stack.back()->children.back()->str = *$3;
		append_attr(extra->ast_stack.back()->children.back().get(), std::move($1));
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_ASSIGN, std::make_unique<AstNode>(@$, AST_IDENTIFIER), AstNode::mkconst_int(@$, 0, false, 1)));
		extra->ast_stack.back()->children.back()->children[0]->str = *$3;
	} opt_supply_wires TOK_SEMICOL |
	attr TOK_SUPPLY1 TOK_ID {
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_WIRE));
		extra->ast_stack.back()->children.back()->str = *$3;
		append_attr(extra->ast_stack.back()->children.back().get(), std::move($1));
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_ASSIGN, std::make_unique<AstNode>(@$, AST_IDENTIFIER), AstNode::mkconst_int(@$, 1, false, 1)));
		extra->ast_stack.back()->children.back()->children[0]->str = *$3;
	} opt_supply_wires TOK_SEMICOL;

opt_supply_wires:
	%empty |
	opt_supply_wires TOK_COMMA TOK_ID {
		auto wire_node = extra->ast_stack.back()->children.at(GetSize(extra->ast_stack.back()->children)-2)->clone();
		auto assign_node = extra->ast_stack.back()->children.at(GetSize(extra->ast_stack.back()->children)-1)->clone();
		wire_node->str = *$3;
		assign_node->children[0]->str = *$3;
		extra->ast_stack.back()->children.push_back(std::move(wire_node));
		extra->ast_stack.back()->children.push_back(std::move(assign_node));
	};

wire_name_list:
	wire_name_and_opt_assign | wire_name_list TOK_COMMA wire_name_and_opt_assign;

wire_name_and_opt_assign:
	wire_name {
		bool attr_anyconst = false;
		bool attr_anyseq = false;
		bool attr_allconst = false;
		bool attr_allseq = false;
		if (extra->ast_stack.back()->children.back()->get_bool_attribute(ID::anyconst)) {
			extra->ast_stack.back()->children.back()->attributes.erase(ID::anyconst);
			attr_anyconst = true;
		}
		if (extra->ast_stack.back()->children.back()->get_bool_attribute(ID::anyseq)) {
			extra->ast_stack.back()->children.back()->attributes.erase(ID::anyseq);
			attr_anyseq = true;
		}
		if (extra->ast_stack.back()->children.back()->get_bool_attribute(ID::allconst)) {
			extra->ast_stack.back()->children.back()->attributes.erase(ID::allconst);
			attr_allconst = true;
		}
		if (extra->ast_stack.back()->children.back()->get_bool_attribute(ID::allseq)) {
			extra->ast_stack.back()->children.back()->attributes.erase(ID::allseq);
			attr_allseq = true;
		}
		if (extra->current_wire_rand || attr_anyconst || attr_anyseq || attr_allconst || attr_allseq) {
			auto wire = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
			auto fcall = std::make_unique<AstNode>(@$, AST_FCALL);
			wire->str = extra->ast_stack.back()->children.back()->str;
			fcall->str = extra->current_wire_const ? "\\$anyconst" : "\\$anyseq";
			if (attr_anyconst)
				fcall->str = "\\$anyconst";
			if (attr_anyseq)
				fcall->str = "\\$anyseq";
			if (attr_allconst)
				fcall->str = "\\$allconst";
			if (attr_allseq)
				fcall->str = "\\$allseq";
			fcall->attributes[ID::reg] = AstNode::mkconst_str(@$, RTLIL::unescape_id(wire->str));
			extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_ASSIGN, std::move(wire), std::move(fcall)));
		}
	} |
	wire_name TOK_EQ expr {
		auto wire = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		wire->str = extra->ast_stack.back()->children.back()->str;
		if (extra->astbuf1->is_input) {
			extra->astbuf1->attributes[ID::defaultvalue] = std::move($3);
		}
		else if (extra->astbuf1->is_reg || extra->astbuf1->is_logic){
			auto assign = std::make_unique<AstNode>(@$, AST_ASSIGN_LE, std::move(wire), std::move($3));
			SET_AST_NODE_LOC(assign.get(), @1, @3);
			auto block = std::make_unique<AstNode>(@$, AST_BLOCK, std::move(assign));
			SET_AST_NODE_LOC(block.get(), @1, @3);
			auto init = std::make_unique<AstNode>(@$, AST_INITIAL, std::move(block));
			SET_AST_NODE_LOC(init.get(), @1, @3);

			extra->ast_stack.back()->children.push_back(std::move(init));
		}
		else {
			auto assign = std::make_unique<AstNode>(@$, AST_ASSIGN, std::move(wire), std::move($3));
			SET_AST_NODE_LOC(assign.get(), @1, @3);
			extra->ast_stack.back()->children.push_back(std::move(assign));
		}

	};

wire_name_dims:
	range_or_multirange { $$ = std::move($1); } |
	TOK_SVA_REP_CONSEC TOK_RBRA {
		// wildcard associative-array dimension `int arr [*];` (IEEE 7.8.1):
		// unbounded; modeled as a single-element memory. Also used for
		// struct members (member_name) and for type-indexed dimensions,
		// which the lexer rewrites to this token pair.
		$$ = std::make_unique<AstNode>(@$, AST_RANGE);
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
		$$->children.push_back(AstNode::mkconst_int(@$, 0, true));
	};

wire_name:
	TOK_ID wire_name_dims {
		if (extra->astbuf1 == nullptr)
			err_at_loc(@1, "Internal error - should not happen - no AST_WIRE node.");
		auto node = extra->astbuf1->clone();
		node->str = *$1;
		append_attr_clone(node.get(), extra->albuf);
		if (extra->astbuf2 != nullptr)
			node->children.push_back(extra->astbuf2->clone());
		if ($2 != nullptr) {
			if ((node->is_input || node->is_output) && !extra->current_function_or_task)
				err_at_loc(@2, "input/output/inout ports cannot have unpacked dimensions.");
			if (!extra->astbuf2 && !node->is_custom_type) {
				addRange(node.get(), 0, 0, false);
			}
			rewriteAsMemoryNode(node.get(), std::move($2));
		}
		if (extra->current_function_or_task) {
			if (node->is_input || node->is_output)
				node->port_id = extra->current_function_or_task_port_id++;
		} else if (extra->ast_stack.back()->type == AST_GENBLOCK) {
			if (node->is_input || node->is_output)
				err_at_loc(@1, "Cannot declare module port `%s' within a generate block.", *$1);
		} else {
			if (extra->do_not_require_port_stubs && (node->is_input || node->is_output) && extra->port_stubs.count(*$1) == 0) {
				extra->port_stubs[*$1] = ++extra->port_counter;
			}
			if (extra->port_stubs.count(*$1) != 0) {
				if (!node->is_input && !node->is_output)
					err_at_loc(@1, "Module port `%s' is neither input nor output.", *$1);
				if (node->is_reg && node->is_input && !node->is_output && !mode->sv)
					err_at_loc(@1, "Input port `%s' is declared as register.", *$1);
				node->port_id = extra->port_stubs[*$1];
				extra->port_stubs.erase(*$1);
			} else {
				if (node->is_input || node->is_output)
					err_at_loc(@1, "Module port `%s' is not declared in module header.", *$1);
			}
		}
		//FIXME: for some reason, TOK_ID has a location which always points to one column *after* the real last column...
		SET_AST_NODE_LOC(node.get(), @1, @1);
		if (!node->is_input && !node->is_output && !extra->current_function_or_task) {
			for (auto &ch : node->children)
				if (ch->type == AST_WIRETYPE) {
					auto it = extra->struct_defaults_by_type.find(ch->str);
					if (it != extra->struct_defaults_by_type.end())
						extra->emitStructDefaults(@$, extra->ast_stack.back(), *$1, it->second);
					break;
				}
		}
		extra->ast_stack.back()->children.push_back(std::move(node));
	};

assign_stmt:
	TOK_ASSIGN delay assign_expr_list TOK_SEMICOL |
	TOK_ASSIGN TOK_LPAREN TOK_ID TOK_COMMA TOK_ID TOK_RPAREN delay assign_expr_list TOK_SEMICOL |
	TOK_ALIAS alias_lvalues TOK_SEMICOL {
		if (!mode->sv)
			err_at_loc(@1, "Alias statements are only supported in SystemVerilog mode.");
		extra->saveChild(std::move($2));
	};

alias_lvalues:
	lvalue {
		$$ = mk_sva(@$, "alias");
		$$->children.push_back(std::move($1));
	} |
	alias_lvalues TOK_EQ lvalue {
		$$ = std::move($1);
		$$->children.push_back(std::move($3));
	};

assign_expr_list:
	assign_expr | assign_expr_list TOK_COMMA assign_expr;

assign_expr:
	lvalue TOK_EQ expr {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_ASSIGN, std::move($1), std::move($3)));
		SET_AST_NODE_LOC(node, @$, @$);
	};

type_name: TOK_ID { $$ = std::move($1); } // first time seen
	 | TOK_USER_TYPE	{ if (extra->isInLocalScope($1.get())) err_at_loc(@1, "Duplicate declaration of TYPEDEF '%s'", $1->c_str()+1); $$ = std::move($1); }
	 ;

typedef_decl:
	TOK_TYPEDEF typedef_base_type range_or_multirange type_name range_or_multirange TOK_SEMICOL {
		extra->astbuf1 = std::move($2);
		extra->astbuf2 = checkRange(extra->astbuf1.get(), std::move($3));
		bool has_a_range = (bool)extra->astbuf2;
		if (extra->astbuf2) {
			extra->astbuf1->children.push_back(std::move(extra->astbuf2));
		}

		if ($5 != nullptr) {
			if (!has_a_range && !extra->astbuf1->is_custom_type) {
				addRange(extra->astbuf1.get(), 0, 0, false);
			}
			rewriteAsMemoryNode(extra->astbuf1.get(), std::move($5));
		}
		extra->addTypedefNode($4.get(), std::move(extra->astbuf1)); }
	| TOK_TYPEDEF enum_struct_type type_name TOK_SEMICOL   {
		extra->addTypedefNode($3.get(), std::move($2));
		if (!extra->pending_struct_defaults.empty()) {
			auto &defs = extra->struct_defaults_by_type[*$3];
			for (auto &def : extra->pending_struct_defaults)
				defs.emplace_back(def.first, def.second->clone());
			extra->pending_struct_defaults.clear();
		}
	}
	| TOK_TYPEDEF TOK_SCOPED_ID type_name TOK_SEMICOL {
		auto base = std::make_unique<AstNode>(@$, AST_WIRE);
		base->is_logic = true;
		extra->addWiretypeNode($2.get(), base.get());
		extra->addTypedefNode($3.get(), std::move(base));
	} |
	TOK_TYPEDEF TOK_CLASS TOK_ID TOK_SEMICOL {
		// forward declaration of a class (e.g. `typedef class uvm_object;`)
		auto base = std::make_unique<AstNode>(@$, AST_WIRE);
		base->is_logic = true;
		extra->addTypedefNode($3.get(), std::move(base));
	} |
	TOK_TYPEDEF hierarchical_type_id TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN type_name TOK_SEMICOL {
		// typedef of a parameterized class (e.g. `typedef uvm_registry #(T) type_id;`)
		auto base = std::make_unique<AstNode>(@$, AST_WIRE);
		base->is_logic = true;
		extra->addWiretypeNode($2.get(), base.get());
		extra->addTypedefNode($7.get(), std::move(base));
	} |
	TOK_TYPEDEF TOK_ID TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN type_name TOK_SEMICOL {
		// same, with the parameterized type not yet registered (a forward
		// declared class name lexes as a plain identifier, e.g.
		// `typedef uvm_config_db #(uvm_object) m_uvm_config_obj_misc;`)
		auto base = std::make_unique<AstNode>(@$, AST_WIRE);
		base->is_logic = true;
		extra->addWiretypeNode($2.get(), base.get());
		extra->addTypedefNode($7.get(), std::move(base));
	} |
	TOK_TYPEDEF TOK_STRUCT TOK_ID TOK_SEMICOL |
	TOK_TYPEDEF TOK_UNION TOK_ID TOK_SEMICOL |
	TOK_TYPEDEF TOK_ENUM TOK_ID TOK_SEMICOL {
		// forward enum typedef declaration (IEEE 6.18); the definition
		// follows later in the same scope
	} |
	TOK_TYPEDEF typedef_base_type range_or_multirange type_name TOK_SVA_REP_CONSEC TOK_RBRA TOK_SEMICOL {
		// wildcard dimension `typedef data_type name [*];` (IEEE 6.18,
		// open-ended array type): parsed and dropped; unsized array types
		// are not modeled
		extra->astbuf1 = std::move($2);
		extra->astbuf2 = checkRange(extra->astbuf1.get(), std::move($3));
		if (extra->astbuf2 != nullptr)
			extra->astbuf1->children.push_back(std::move(extra->astbuf2));
		extra->addTypedefNode($4.get(), std::move(extra->astbuf1));
	} |
	TOK_TYPEDEF TOK_ID TOK_SEMICOL {
		// forward typedef declaration; the definition follows later
	} |
	TOK_NETTYPE typedef_base_type TOK_ID opt_nettype_with TOK_SEMICOL {
		// user-defined nettype (IEEE 1800-2017 6.6.7): aliased to its data
		// type; net resolution semantics are not modeled
		auto base = std::move($2);
		extra->addTypedefNode($3.get(), std::move(base));
	}
	;

opt_nettype_with:
	%empty |
	TOK_WITH TOK_ID {
		// optional nettype resolution function; resolution is not modeled
	};

typedef_base_type:
	hierarchical_type_id {
		$$ = std::make_unique<AstNode>(@$, AST_WIRE);
		$$->is_logic = true;
		extra->addWiretypeNode($1.get(), $$.get());
	} |
	TOK_VIRTUAL hierarchical_type_id class_type_params {
		// virtual interface typedef (e.g. `typedef virtual pins_if #(N) intr_vif;`)
		$$ = std::make_unique<AstNode>(@$, AST_WIRE);
		$$->is_logic = true;
		extra->addWiretypeNode($2.get(), $$.get());
	} |
	TOK_REAL {
		$$ = std::make_unique<AstNode>(@$, AST_WIRE);
		$$->children.push_back(std::make_unique<AstNode>(@$, AST_REALVALUE));
	} |
	integer_vector_type opt_signedness_default_unsigned {
		$$ = std::make_unique<AstNode>(@$, AST_WIRE);
		if ($1 == token::TOK_REG) {
			$$->is_reg = true;
		} else {
			$$->is_logic = true;
		}
		$$->is_signed = $2;
	} |
	integer_atom_type opt_signedness_default_signed {
		$$ = std::make_unique<AstNode>(@$, AST_WIRE);
		$$->is_logic = true;
		$$->is_signed = $2;
		$$->range_left = $1 - 1;
		$$->range_right = 0;
	};

enum_struct_type:
	  enum_type { $$ = std::move($1); }
	| struct_type { $$ = std::move($1); }
	;

cell_stmt:
	attr TOK_ID {
		extra->astbuf1 = std::make_unique<AstNode>(@$, AST_CELL);
		append_attr(extra->astbuf1.get(), std::move($1));
		extra->astbuf1->children.push_back(std::make_unique<AstNode>(@$, AST_CELLTYPE));
		extra->astbuf1->children[0]->str = *$2;
	} cell_parameter_list_opt cell_list TOK_SEMICOL {
		(void)extra->astbuf1.reset();
	} |
	attr tok_prim_wrapper delay {
		extra->astbuf1 = std::make_unique<AstNode>(@$, AST_PRIMITIVE);
		extra->astbuf1->str = *$2;
		append_attr(extra->astbuf1.get(), std::move($1));
	} prim_list TOK_SEMICOL {
		(void)extra->astbuf1.reset();
	} |
	attr TOK_PULLUP TOK_LPAREN TOK_ID TOK_RPAREN TOK_ID {
		auto node = std::make_unique<AstNode>(@$, AST_PRIMITIVE);
		node->str = *$2;
		append_attr(node.get(), std::move($1));
		log_assert(!extra->cell_hack);
		extra->cell_hack = node.get();
		extra->ast_stack.back()->children.push_back(std::move(node));
	} TOK_LPAREN cell_port_list TOK_RPAREN TOK_SEMICOL {
		extra->cell_hack = nullptr;
	};

tok_prim_wrapper:
	TOK_PRIMITIVE {
		$$ = std::move($1);
	} |
	TOK_OR {
		$$ = std::make_unique<std::string>("or");
	};

cell_list:
	single_cell |
	cell_list TOK_COMMA single_cell;

single_cell:
	single_cell_no_array | single_cell_arraylist;

single_cell_no_array:
	TOK_ID {
		extra->astbuf2 = extra->astbuf1->clone();
		if (extra->astbuf2->type != AST_PRIMITIVE)
			extra->astbuf2->str = *$1;
		// TODO optimize again
		extra->cell_hack = extra->astbuf2.get();
		extra->ast_stack.back()->children.push_back(std::move(extra->astbuf2));
	} TOK_LPAREN cell_port_list TOK_RPAREN {
		log_assert(extra->cell_hack);
		SET_AST_NODE_LOC(extra->cell_hack, @1, @$);
		extra->cell_hack = nullptr;
	} |
	TOK_REF {
		// "ref" is a contextual keyword: usable as an instance name
		extra->astbuf2 = extra->astbuf1->clone();
		if (extra->astbuf2->type != AST_PRIMITIVE)
			extra->astbuf2->str = "\\ref";
		extra->cell_hack = extra->astbuf2.get();
		extra->ast_stack.back()->children.push_back(std::move(extra->astbuf2));
	} TOK_LPAREN cell_port_list TOK_RPAREN {
		log_assert(extra->cell_hack);
		SET_AST_NODE_LOC(extra->cell_hack, @1, @$);
		extra->cell_hack = nullptr;
	}

single_cell_arraylist:
	TOK_ID non_opt_range {
		extra->astbuf2 = extra->astbuf1->clone();
		if (extra->astbuf2->type != AST_PRIMITIVE)
			extra->astbuf2->str = *$1;
		// TODO optimize again
		extra->cell_hack = extra->astbuf2.get();
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_CELLARRAY, std::move($2), std::move(extra->astbuf2)));
	} TOK_LPAREN cell_port_list TOK_RPAREN{
		log_assert(extra->cell_hack);
		SET_AST_NODE_LOC(extra->cell_hack, @1, @$);
		extra->cell_hack = nullptr;
	} |
	TOK_REF non_opt_range {
		extra->astbuf2 = extra->astbuf1->clone();
		if (extra->astbuf2->type != AST_PRIMITIVE)
			extra->astbuf2->str = "\\ref";
		extra->cell_hack = extra->astbuf2.get();
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_CELLARRAY, std::move($2), std::move(extra->astbuf2)));
	} TOK_LPAREN cell_port_list TOK_RPAREN{
		log_assert(extra->cell_hack);
		SET_AST_NODE_LOC(extra->cell_hack, @1, @$);
		extra->cell_hack = nullptr;
	};

cell_list_no_array:
	single_cell_no_array |
	cell_list_no_array TOK_COMMA single_cell_no_array;

prim_list:
	single_prim |
	prim_list TOK_COMMA single_prim;

single_prim:
	single_cell |
	/* no name */ {
		extra->astbuf2 = extra->astbuf1->clone();
		log_assert(!extra->cell_hack);
		extra->cell_hack = extra->astbuf2.get();
		// TODO optimize again
		extra->ast_stack.back()->children.push_back(std::move(extra->astbuf2));
	} TOK_LPAREN cell_port_list TOK_RPAREN {
		log_assert(extra->cell_hack);
		SET_AST_NODE_LOC(extra->cell_hack, @1, @$);
		extra->cell_hack = nullptr;
	}

cell_parameter_list_opt:
	TOK_HASH TOK_LPAREN cell_parameter_list TOK_RPAREN | %empty;

cell_parameter_list:
	cell_parameter | cell_parameter_list TOK_COMMA cell_parameter;

cell_parameter:
	%empty |
	expr {
		auto node = std::make_unique<AstNode>(@$, AST_PARASET);
		node->children.push_back(std::move($1));
		extra->astbuf1->children.push_back(std::move(node));
	} |
	TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN {
		// just ignore empty parameters
	} |
	TOK_DOT TOK_ID TOK_LPAREN typedef_base_type range_or_multirange TOK_RPAREN {
		// `.T(logic [7:0])`: an anonymous data type as a type-parameter
		// override (register v3 D0b).  Declare it as a typedef of the
		// enclosing module under a private name and pass that name; genrtlil
		// qualifies it `module::name` and derive splices it into the child.
		auto base = std::move($4);
		if ($5) {
			auto range = checkRange(base.get(), std::move($5));
			if (range)
				base->children.push_back(std::move(range));
		}
		std::string tn = stringf("\\$tparam$%d", autoidx++);
		extra->addTypedefNode(&tn, std::move(base));
		auto node = std::make_unique<AstNode>(@$, AST_PARASET);
		node->str = *$2;
		auto ref = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		ref->str = tn;
		node->children.push_back(std::move(ref));
		extra->astbuf1->children.push_back(std::move(node));
	} |
	TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN {
		auto node = std::make_unique<AstNode>(@$, AST_PARASET);
		node->str = *$2;
		node->children.push_back(std::move($4));
		extra->astbuf1->children.push_back(std::move(node));
	} |
	TOK_DOT TOK_ID TOK_LPAREN TOK_LOGIC TOK_RPAREN {
		// type parameter override, e.g. `.TagType(logic)`; discarded
	} |
	TOK_DOT TOK_ID TOK_LPAREN TOK_REG TOK_RPAREN {
		// type parameter override, e.g. `.TagType(reg)`; discarded
	} |
	TOK_DOT TOK_ID TOK_LPAREN integer_atom_type TOK_RPAREN {
		// type parameter override, e.g. `.T(int)`; discarded
	} |
	TOK_DOT TOK_ID TOK_LPAREN integer_atom_type TOK_UNSIGNED TOK_RPAREN {
		// type parameter override, e.g. `.T(int unsigned)`; discarded
	};

cell_port_list:
	cell_port_list_rules {
		// remove empty args from end of list
		while (!extra->cell_hack->children.empty()) {
			auto& node = extra->cell_hack->children.back();
			if (node->type != AST_ARGUMENT) break;
			if (!node->children.empty()) break;
			if (!node->str.empty()) break;
			extra->cell_hack->children.pop_back();
		}

		// IEEE 1800-2009 23.3.2: positional connections may precede named
		// ones. A positional connection after a named one is illegal.
		bool seen_named = false;
		for (auto& node : extra->cell_hack->children) {
			if (node->type != AST_ARGUMENT) continue;
			if (node->str.empty()) {
				if (seen_named)
					err_at_loc(@1, "Positional cell port follows a named port.");
			} else {
				seen_named = true;
			}
		}
	};

cell_port_list_rules:
	cell_port | cell_port_list_rules TOK_COMMA cell_port;

cell_port:
	attr {
		auto node = std::make_unique<AstNode>(@$, AST_ARGUMENT);
		extra->cell_hack->children.push_back(std::move(node));
	} |
	attr expr {
		auto node = std::make_unique<AstNode>(@$, AST_ARGUMENT);
		node->children.push_back(std::move($2));
		extra->cell_hack->children.push_back(std::move(node));
	} |
	attr TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN {
		auto node = std::make_unique<AstNode>(@$, AST_ARGUMENT);
		node->str = *$3;
		node->children.push_back(std::move($5));
		extra->cell_hack->children.push_back(std::move(node));
	} |
	attr TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN {
		auto node = std::make_unique<AstNode>(@$, AST_ARGUMENT);
		node->str = *$3;
		extra->cell_hack->children.push_back(std::move(node));
	} |
	attr TOK_DOT TOK_ID {
		auto node = std::make_unique<AstNode>(@$, AST_ARGUMENT);
		node->str = *$3;
		node->children.push_back(std::make_unique<AstNode>(@$, AST_IDENTIFIER));
		node->children.back()->str = *$3;
		extra->cell_hack->children.push_back(std::move(node));
	} |
	attr TOK_WILDCARD_CONNECT {
		if (!mode->sv)
			err_at_loc(@2, "Wildcard port connections are only supported in SystemVerilog mode.");
		extra->cell_hack->attributes[ID::wildcard_port_conns] = AstNode::mkconst_int(@2, 1, false);
	};

always_comb_or_latch:
	TOK_ALWAYS_COMB {
		$$ = false;
	} |
	TOK_ALWAYS_LATCH {
		$$ = true;
	};

always_or_always_ff:
	TOK_ALWAYS {
		$$ = false;
	} |
	TOK_ALWAYS_FF {
		$$ = true;
	};

always_stmt:
	attr always_or_always_ff {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_ALWAYS));
		append_attr(node, std::move($1));
		if ($2)
			node->attributes[ID::always_ff] = AstNode::mkconst_int(@2, 1, false);
	} always_cond {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @6, @6);
		extra->ast_stack.pop_back();

		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @$);
		extra->ast_stack.pop_back();

		SET_RULE_LOC(@$, @2, @$);
	} |
	attr always_comb_or_latch {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_ALWAYS));
		append_attr(node, std::move($1));
		if ($2)
			node->attributes[ID::always_latch] = AstNode::mkconst_int(@2, 1, false);
		else
			node->attributes[ID::always_comb] = AstNode::mkconst_int(@2, 1, false);
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
		extra->in_comb_body = true;
	} behavioral_stmt {
		extra->in_comb_body = false;
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	} |
	attr TOK_INITIAL {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_INITIAL));
		append_attr(node, std::move($1));
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} behavioral_stmt {
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	} |
	attr TOK_FINAL {
		parse_degrade(extra, @2, "`final' block runs at step 0, like an initial block");
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_INITIAL));
		append_attr(node, std::move($1));
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} behavioral_stmt {
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	};

event_wait_list:
	event_wait |
	event_wait_list TOK_OR event_wait;

foreach_var:
	TOK_ID {
		auto id = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		id->str = *$1;
		extra->ast_stack.back()->children.push_back(std::move(id));
	} |
	%empty {
		auto id = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		extra->ast_stack.back()->children.push_back(std::move(id));
	};

foreach_vars:
	foreach_var |
	foreach_vars TOK_COMMA foreach_var;

foreach_dotted_opt:
	%empty { $$ = nullptr; } |
	TOK_DOT TOK_ID TOK_LBRA foreach_vars TOK_RBRA {
		$$ = std::move($2);
	};

event_wait:
	TOK_POSEDGE expr |
	TOK_NEGEDGE expr |
	TOK_EDGE expr |
	hierarchical_id range |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID;

always_cond:
	TOK_AT TOK_LPAREN always_events TOK_RPAREN |
	TOK_AT TOK_LPAREN TOK_ASTER TOK_RPAREN |
	TOK_AT ATTR_BEGIN TOK_RPAREN |
	TOK_AT TOK_LPAREN ATTR_END |
	TOK_AT TOK_ASTER |
	%empty;

always_events:
	always_event |
	always_events TOK_OR always_event |
	always_events TOK_COMMA always_event;

always_event:
	TOK_POSEDGE expr {
		auto node = std::make_unique<AstNode>(@$, AST_POSEDGE);
		SET_AST_NODE_LOC(node.get(), @1, @1);
		node->children.push_back(std::move($2));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	TOK_POSEDGE expr TOK_IFF expr {
		auto node = std::make_unique<AstNode>(@$, AST_POSEDGE);
		SET_AST_NODE_LOC(node.get(), @1, @1);
		node->children.push_back(std::move($2));
		node->children.push_back(std::move($4));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	TOK_NEGEDGE expr {
		auto node = std::make_unique<AstNode>(@$, AST_NEGEDGE);
		SET_AST_NODE_LOC(node.get(), @1, @1);
		node->children.push_back(std::move($2));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	TOK_NEGEDGE expr TOK_IFF expr {
		auto node = std::make_unique<AstNode>(@$, AST_NEGEDGE);
		SET_AST_NODE_LOC(node.get(), @1, @1);
		node->children.push_back(std::move($2));
		node->children.push_back(std::move($4));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	expr {
		auto node = std::make_unique<AstNode>(@$, AST_EDGE);
		node->children.push_back(std::move($1));
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	TOK_EDGE expr {
		// IEEE 1800-2009 9.4.2: `edge' selects any edge, like a bare
		// event expression
		auto node = std::make_unique<AstNode>(@$, AST_EDGE);
		node->children.push_back(std::move($2));
		extra->ast_stack.back()->children.push_back(std::move(node));
	};

opt_label:
	TOK_COL TOK_ID {
		$$ = std::move($2);
	} |
	%empty {
		$$ = nullptr;
	};

opt_sva_label:
	TOK_SVA_LABEL TOK_COL {
		$$ = std::move($1);
	} |
	%empty {
		$$ = nullptr;
	};

opt_property:
	TOK_FINAL {
		$$ = false;
	} |
	%empty {
		$$ = false;
	};

modport_stmt:
    TOK_MODPORT TOK_ID {
		AstNode* modport = extra->pushChild(std::make_unique<AstNode>(@$, AST_MODPORT));
        modport->str = *$2;

    }  modport_args_opt {
        extra->ast_stack.pop_back();
        log_assert(extra->ast_stack.size() == 2);
    } TOK_SEMICOL

modport_args_opt:
    TOK_LPAREN TOK_RPAREN | TOK_LPAREN modport_args optional_comma TOK_RPAREN;

modport_args:
    modport_arg | modport_args TOK_COMMA modport_arg;

modport_arg:
    modport_type_token modport_member |
    modport_member

modport_member:
    TOK_ID {
		AstNode* modport_member = extra->saveChild(std::make_unique<AstNode>(@$, AST_MODPORTMEMBER));
        modport_member->str = *$1;
        modport_member->is_input = extra->current_modport_input;
        modport_member->is_output = extra->current_modport_output;

    }

modport_type_token:
    TOK_INPUT {extra->current_modport_input = 1; extra->current_modport_output = 0;} | TOK_OUTPUT {extra->current_modport_input = 0; extra->current_modport_output = 1;}

assert:
	opt_sva_label TOK_ASSERT opt_property TOK_LPAREN expr TOK_RPAREN {
		extra->pending_assert_cond = $5->clone();
	} sva_action {
		extra->pending_assert_cond.reset();
		if (mode->noassert) {

		} else {
			AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, mode->assume_asserts ? AST_ASSUME : AST_ASSERT, std::move($5)));
			SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @6);
			if ($1 != nullptr)
				node->str = *$1;
		}
	} |
	opt_sva_label TOK_ASSUME opt_property TOK_LPAREN expr TOK_RPAREN sva_action {
		if (mode->noassume) {
		} else {
			AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, mode->assert_assumes ? AST_ASSERT : AST_ASSUME, std::move($5)));
			SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @6);
			if ($1 != nullptr)
				node->str = *$1;
		}
	} |
	opt_sva_label TOK_ASSUME opt_property TOK_HASH TOK_CONSTVAL TOK_LPAREN expr TOK_RPAREN sva_action {
		// IEEE 1800-2009 16.2: the `#0' postponed form is elaborated
		// like the plain statement
		if (mode->noassume) {
		} else {
			AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, mode->assert_assumes ? AST_ASSERT : AST_ASSUME, std::move($7)));
			SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @8);
			if ($1 != nullptr)
				node->str = *$1;
		}
	} |
	opt_sva_label TOK_ASSERT opt_property TOK_LPAREN sva_eventually_tok expr TOK_RPAREN sva_action {
		if (mode->noassert) {
		} else {
			AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, mode->assume_asserts ? AST_FAIR : AST_LIVE, std::move($6)));
			SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @7);
			if ($1 != nullptr)
				node->str = *$1;
		}
	} |
	opt_sva_label TOK_ASSUME opt_property TOK_LPAREN sva_eventually_tok expr TOK_RPAREN sva_action {
		if (mode->noassume) {
		} else {
			AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, mode->assert_assumes ? AST_LIVE : AST_FAIR, std::move($6)));
			SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @7);
			if ($1 != nullptr)
				node->str = *$1;
		}
	} |
	opt_sva_label TOK_COVER opt_property TOK_LPAREN expr TOK_RPAREN sva_action {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_COVER, std::move($5)));
		SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @6);
		if ($1 != nullptr) {
			node->str = *$1;
		}
	} |
	opt_sva_label TOK_COVER opt_property TOK_HASH TOK_CONSTVAL TOK_LPAREN expr TOK_RPAREN sva_action {
		// IEEE 1800-2009 16.2: the `#0' postponed form
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_COVER, std::move($7)));
		SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @8);
		if ($1 != nullptr) {
			node->str = *$1;
		}
	} |
	opt_sva_label TOK_COVER opt_property TOK_LPAREN TOK_RPAREN TOK_SEMICOL {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_COVER, AstNode::mkconst_int(@$, 1, false)));
		SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @5);
		if ($1 != nullptr) {
			node->str = *$1;
		}
	} |
	opt_sva_label TOK_COVER TOK_SEMICOL {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_COVER, AstNode::mkconst_int(@$, 1, false)));
		SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @2);
		if ($1 != nullptr) {
			node->str = *$1;
		}
	} |
	opt_sva_label TOK_RESTRICT opt_property TOK_LPAREN expr TOK_RPAREN sva_action {
		if (mode->norestrict) {
		} else {
			AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_ASSUME, std::move($5)));
			SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @6);
			if ($1 != nullptr)
				node->str = *$1;
		}
		if (!$3)
			warn_at_loc(@3, "SystemVerilog does not allow \"restrict\" without \"property\".");
	} |
	opt_sva_label TOK_RESTRICT opt_property TOK_LPAREN sva_eventually_tok expr TOK_RPAREN sva_action {
		if (mode->norestrict) {
		} else {
			AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_FAIR, std::move($6)));
			SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @7);
			if ($1 != nullptr)
				node->str = *$1;
		}
		if (!$3)
			warn_at_loc(@3, "SystemVerilog does not allow \"restrict\" without \"property\".");
	} |
	opt_sva_label TOK_ASSERT TOK_HASH integral_number TOK_LPAREN expr TOK_RPAREN sva_action {
		if (mode->noassert) {
		} else if (*$4 != "0") {
			err_at_loc(@4, "Deferred immediate assertions only allow #0 in this frontend.");
		} else {
			AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, mode->assume_asserts ? AST_ASSUME : AST_ASSERT, std::move($6)));
			SET_AST_NODE_LOC(node, ($1 != nullptr ? @1 : @2), @7);
			if ($1 != nullptr)
				node->str = *$1;
		}
	} |
	sva_concurrent;

assert_property:
	sva_concurrent;

sva_eventually_tok:
	TOK_EVENTUALLY | TOK_S_EVENTUALLY;

sva_dir:
	TOK_ASSERT { $$ = 'a'; } |
	TOK_ASSUME { $$ = 'm'; } |
	TOK_COVER { $$ = 'c'; } |
	TOK_RESTRICT { $$ = 'r'; };

sva_concurrent:
	opt_sva_label sva_dir TOK_PROPERTY TOK_LPAREN sva_property_spec TOK_RPAREN sva_action {
		char dir = $2;
		bool skip = (dir == 'a' && mode->noassert) || (dir == 'm' && mode->noassume) || (dir == 'r' && mode->norestrict);
		if (!skip) {
			auto body = std::move($5);
			AstNodeType ty = AST_ASSERT;
			if (dir == 'm' || dir == 'r')
				ty = AST_ASSUME;
			if (dir == 'c')
				ty = AST_COVER;
			if (dir == 'a' && mode->assume_asserts)
				ty = AST_ASSUME;
			if (dir == 'm' && mode->assert_assumes)
				ty = AST_ASSERT;
			if (body && body->type == AST_SVA &&
					(body->str == "eventually" || body->str == "s_eventually") &&
					body->children.size() == 1 && body->children[0]->type != AST_SVA) {
				if (dir == 'a')
					ty = mode->assume_asserts ? AST_FAIR : AST_LIVE;
				else if (dir == 'm')
					ty = mode->assert_assumes ? AST_LIVE : AST_FAIR;
				else if (dir == 'r')
					ty = AST_FAIR;
				body = std::move(body->children[0]);
			}
			auto node = std::make_unique<AstNode>(@$, ty, std::move(body));
			if ($7)
				node->children.push_back(std::move($7));
			SET_AST_NODE_LOC(node.get(), ($1 != nullptr ? @1 : @2), @6);
			if ($1 != nullptr)
				node->str = *$1;
			// Checker expansion retrofits the checker's own `default
			// clocking` onto clock-less concurrent assertions; the tag
			// distinguishes them from plain immediate assertions, which
			// take no clocking event.
			if (extra->in_checker)
				node->set_attribute(ID(sv_concurrent), AstNode::mkconst_int(@2, 1, false));
			extra->saveChild(std::move(node));
		}
	} |
	opt_sva_label TOK_COVER TOK_SEQUENCE TOK_LPAREN sva_property_spec TOK_RPAREN sva_action {
		auto node = std::make_unique<AstNode>(@$, AST_COVER, std::move($5));
		if ($7)
			node->children.push_back(std::move($7));
		SET_AST_NODE_LOC(node.get(), ($1 != nullptr ? @1 : @2), @6);
		if ($1 != nullptr)
			node->str = *$1;
		extra->saveChild(std::move(node));
	} |
	opt_sva_label TOK_EXPECT TOK_LPAREN sva_property_spec TOK_RPAREN sva_action {
		auto node = std::make_unique<AstNode>(@$, AST_ASSERT, std::move($4));
		node->attributes[IdString("\\expect")] = AstNode::mkconst_int(@2, 1, false);
		if ($6)
			node->children.push_back(std::move($6));
		SET_AST_NODE_LOC(node.get(), ($1 != nullptr ? @1 : @2), @5);
		if ($1 != nullptr)
			node->str = *$1;
		extra->saveChild(std::move(node));
	};

// a procedural event control; rejected at the '@' inside always_comb /
// always_latch bodies (IEEE 1800 9.2.2.2, upstream's wording)
proc_at:
	TOK_AT {
		if (extra->in_comb_body)
			err_at_loc(@1, "syntax error, unexpected '@'");
	};

asgn_delay:
	delay |
	TOK_AT TOK_LPAREN timing_events TOK_RPAREN |
	TOK_REPEAT TOK_LPAREN expr TOK_RPAREN asgn_delay;

timing_events:
	timing_event { $$ = std::move($1); } |
	timing_events TOK_OR timing_event { $$ = std::move($1); } |
	timing_events TOK_COMMA timing_event { $$ = std::move($1); };

timing_event:
	TOK_POSEDGE expr { $$ = nullptr; (void)$2; } |
	TOK_NEGEDGE expr { $$ = nullptr; (void)$2; } |
	TOK_EDGE expr { $$ = nullptr; (void)$2; } |
	expr { $$ = nullptr; (void)$1; };

simple_behavioral_stmt:
	attr lvalue TOK_EQ asgn_delay expr {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move($2), std::move($5)));
		SET_AST_NODE_LOC(node, @2, @5);
		append_attr(node, std::move($1));
	} |
	attr lvalue attr inc_or_dec_op {
		extra->addIncOrDecStmt(std::move($1), std::move($2), std::move($3), $4, location_range(@1, @4));
	} |
	attr inc_or_dec_op attr lvalue {
		extra->addIncOrDecStmt(std::move($1), std::move($4), std::move($3), $2, location_range(@1, @4));
	} |
	attr lvalue OP_LE asgn_delay expr {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_ASSIGN_LE, std::move($2), std::move($5)));
		SET_AST_NODE_LOC(node, @2, @5);
		append_attr(node, std::move($1));
	} |
	attr lvalue asgn_binop asgn_delay expr {
		(void)extra->addAsgnBinopStmt(std::move($1), std::move($2), $3, std::move($5));
	};

asgn_binop:
	TOK_BIT_OR_ASSIGN { $$ = AST_BIT_OR; } |
	TOK_BIT_AND_ASSIGN { $$ = AST_BIT_AND; } |
	TOK_BIT_XOR_ASSIGN { $$ = AST_BIT_XOR; } |
	TOK_ADD_ASSIGN { $$ = AST_ADD; } |
	TOK_SUB_ASSIGN { $$ = AST_SUB; } |
	TOK_DIV_ASSIGN { $$ = AST_DIV; } |
	TOK_MOD_ASSIGN { $$ = AST_MOD; } |
	TOK_MUL_ASSIGN { $$ = AST_MUL; } |
	TOK_SHL_ASSIGN { $$ = AST_SHIFT_LEFT; } |
	TOK_SHR_ASSIGN { $$ = AST_SHIFT_RIGHT; } |
	TOK_SSHL_ASSIGN { $$ = AST_SHIFT_SLEFT; } |
	TOK_SSHR_ASSIGN { $$ = AST_SHIFT_SRIGHT; } ;

inc_or_dec_op:
	// NOTE: These should only be permitted in SV mode, but Yosys has
	// allowed them in all modes since support for them was added in 2017.
	TOK_INCREMENT { $$ = AST_ADD; } |
	TOK_DECREMENT { $$ = AST_SUB; } ;

for_steps:
	simple_behavioral_stmt |
	for_steps TOK_COMMA simple_behavioral_stmt;

for_init_extra:
	%empty |
	for_init_extra TOK_COMMA TOK_ID TOK_EQ expr {
		auto ident = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		ident->str = *$3;
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move(ident), std::move($5)));
	} |
	for_init_extra TOK_COMMA TOK_LOGIC TOK_ID TOK_EQ expr {
		auto wire = std::make_unique<AstNode>(@$, AST_WIRE);
		wire->str = *$4;
		wire->is_logic = true;
		AstNode *parent = extra->ast_stack.at(extra->ast_stack.size() - 2);
		// declared ahead of the loop, like the loop variable: its
		// initializer runs before the first iteration
		if (!parent->children.empty() && parent->children.back()->type == AST_BLOCK)
			parent->children.back()->children.insert(parent->children.back()->children.begin(), std::move(wire));
		else if (!parent->children.empty())
			parent->children.insert(parent->children.end() - 1, std::move(wire));
		else
			parent->children.push_back(std::move(wire));
		auto ident = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		ident->str = *$4;
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move(ident), std::move($6)));
	} |
	for_init_extra TOK_COMMA integer_atom_type TOK_ID TOK_EQ expr {
		// a variable (not a net) of the atom type's width, like the
		// loop variable declared through non_io_wire_type
		auto wire = std::make_unique<AstNode>(@$, AST_WIRE);
		wire->str = *$4;
		wire->is_logic = true;
		wire->is_signed = true;
		wire->children.push_back(std::make_unique<AstNode>(@3, AST_RANGE,
				AstNode::mkconst_int(@3, $3 - 1, true), AstNode::mkconst_int(@3, 0, true)));
		AstNode *parent = extra->ast_stack.at(extra->ast_stack.size() - 2);
		// declared ahead of the loop, like the loop variable: its
		// initializer runs before the first iteration
		if (!parent->children.empty() && parent->children.back()->type == AST_BLOCK)
			parent->children.back()->children.insert(parent->children.back()->children.begin(), std::move(wire));
		else if (!parent->children.empty())
			parent->children.insert(parent->children.end() - 1, std::move(wire));
		else
			parent->children.push_back(std::move(wire));
		auto ident = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		ident->str = *$4;
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move(ident), std::move($6)));
	} |
	for_init_extra TOK_COMMA TOK_USER_TYPE TOK_ID TOK_EQ expr {
		auto wire = std::make_unique<AstNode>(@$, AST_WIRE);
		wire->str = *$4;
		extra->addWiretypeNode($3.get(), wire.get());
		AstNode *parent = extra->ast_stack.at(extra->ast_stack.size() - 2);
		// declared ahead of the loop, like the loop variable: its
		// initializer runs before the first iteration
		if (!parent->children.empty() && parent->children.back()->type == AST_BLOCK)
			parent->children.back()->children.insert(parent->children.back()->children.begin(), std::move(wire));
		else if (!parent->children.empty())
			parent->children.insert(parent->children.end() - 1, std::move(wire));
		else
			parent->children.push_back(std::move(wire));
		auto ident = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		ident->str = *$4;
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move(ident), std::move($6)));
	} |
	for_init_extra TOK_COMMA TOK_SCOPED_ID TOK_ID TOK_EQ expr {
		auto wire = std::make_unique<AstNode>(@$, AST_WIRE);
		wire->str = *$4;
		extra->addWiretypeNode($3.get(), wire.get());
		AstNode *parent = extra->ast_stack.at(extra->ast_stack.size() - 2);
		// declared ahead of the loop, like the loop variable: its
		// initializer runs before the first iteration
		if (!parent->children.empty() && parent->children.back()->type == AST_BLOCK)
			parent->children.back()->children.insert(parent->children.back()->children.begin(), std::move(wire));
		else if (!parent->children.empty())
			parent->children.insert(parent->children.end() - 1, std::move(wire));
		else
			parent->children.push_back(std::move(wire));
		auto ident = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		ident->str = *$4;
		extra->ast_stack.back()->children.push_back(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move(ident), std::move($6)));
	};

for_initialization:
	TOK_ID TOK_EQ expr {
		auto ident = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		ident->str = *$1;
		auto node = std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move(ident), std::move($3));
		SET_AST_NODE_LOC(node.get(), @1, @3);
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	non_io_wire_type range TOK_ID {
		err_at_loc(@3, "For loop variable declaration is missing initialization!");
	} |
	non_io_wire_type range TOK_ID TOK_EQ expr {
		if (!mode->sv)
			err_at_loc(@4, "For loop inline variable declaration is only supported in SystemVerilog mode!");

		// loop variable declaration
		auto wire = std::move($1);
		auto range = checkRange(wire.get(), std::move($2));
		SET_AST_NODE_LOC(wire.get(), @1, @3);
		if (range != nullptr) {
			SET_AST_NODE_LOC(range.get(), @2, @2);
			wire->children.push_back(std::move(range));
		}

		auto ident = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		ident->str = *$3;
		wire->str = *$3;

		AstNode *parent = extra->ast_stack.at(extra->ast_stack.size() - 2);
		auto& loop = parent->children.back();
		log_assert(extra->ast_stack.back() == loop.get());

		// loop variable initialization
		SET_AST_NODE_LOC(ident.get(), @3, @3);
		auto asgn = std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move(ident), std::move($5));
		SET_AST_NODE_LOC(asgn.get(), @3, @5);
		loop->children.push_back(std::move(asgn));

		// inject a wrapping block to declare the loop variable and
		// contain the current loop
		auto wrapper = std::make_unique<AstNode>(@$, AST_BLOCK);
		wrapper->str = "$fordecl_block$" + std::to_string(autoidx++);
		wrapper->children.push_back(std::move(wire));
		wrapper->children.push_back(std::move(loop));
		parent->children.back() = std::move(wrapper);
	};

// Method calls on indexed elements in statement position. These mirror the
// corresponding rvalue alternatives (hierarchical_id '[' expr ']' TOK_DOT ...)
// but reduce directly into a statement, where the rvalue forms cannot appear
// on their own (they would require an assignment).
call_args_tail:
	%empty {
		$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
	} |
	TOK_COMMA expr call_args_tail {
		$$ = std::move($3);
		$$->children.insert($$->children.begin(), std::move($2));
	} |
	TOK_COMMA TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN call_args_tail {
		// named argument in a method call, e.g. `q.add(item, .with_phase(p))`;
		// the name is dropped, matching single_arg's treatment
		$$ = std::move($7);
		$$->children.insert($$->children.begin(), std::move($5));
	} |
	TOK_COMMA TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN call_args_tail {
		// named argument with an empty value, e.g. `q.add(item, .abort())`;
		// dropped like the other named forms
		$$ = std::move($6);
	};

rvalue_call_stmt:
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN {
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN {
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr TOK_COMMA expr TOK_RPAREN {
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
		$$->children.push_back(std::move($10));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr TOK_COMMA expr TOK_COMMA expr TOK_RPAREN {
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
		$$->children.push_back(std::move($10));
		$$->children.push_back(std::move($12));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr TOK_COMMA expr TOK_COMMA expr TOK_COMMA expr TOK_RPAREN {
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
		$$->children.push_back(std::move($10));
		$$->children.push_back(std::move($12));
		$$->children.push_back(std::move($14));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr TOK_COMMA expr TOK_COMMA expr TOK_COMMA expr TOK_COMMA expr TOK_COMMA expr TOK_RPAREN {
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
		$$->children.push_back(std::move($10));
		$$->children.push_back(std::move($12));
		$$->children.push_back(std::move($14));
		$$->children.push_back(std::move($16));
		$$->children.push_back(std::move($18));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr TOK_COMMA expr TOK_COMMA expr TOK_COMMA expr TOK_COMMA expr TOK_COMMA expr TOK_COMMA expr TOK_RPAREN {
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
		$$->children.push_back(std::move($10));
		$$->children.push_back(std::move($12));
		$$->children.push_back(std::move($14));
		$$->children.push_back(std::move($16));
		$$->children.push_back(std::move($18));
		$$->children.push_back(std::move($20));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr call_args_tail TOK_RPAREN {
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
		for (auto &child : $9->children)
			$$->children.push_back(std::move(child));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT rvalue {
		// multi-argument form: rvalue is the method call expression
		if ($6->type == AST_FCALL) {
			auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
			recv->str = *$1;
			auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
			idx->children.push_back(std::move($3));
			recv->children.push_back(std::move(idx));
			std::string method = $6->str;
			if (!method.empty() && method[0] == '\\')
				method = method.substr(1);
			$$ = std::make_unique<AstNode>(@$, AST_FCALL);
			$$->str = "." + method;
			$$->children.push_back(std::move(recv));
			for (auto &child : $6->children)
				$$->children.push_back(std::move(child));
		} else {
			$$ = nullptr;
		}
	};

// this production creates the obligatory if-else shift/reduce conflict
behavioral_stmt:
	defattr | assert | wire_decl | param_decl | localparam_decl | typedef_decl |
	non_opt_delay behavioral_stmt {
		// stock yosys drops statement delays silently and upstream tests
		// expect no warning; record the site on the module instead, and
		// `sim` (the one consumer whose result the delay changes) warns
		note_dropped_delay(extra, @1);
	} |
	simple_behavioral_stmt TOK_SEMICOL |
	attr TOK_SEMICOL |
	attr TOK_RETURN expr TOK_SEMICOL {
		if (extra->current_function_or_task == nullptr)
			err_at_loc(@2, "return outside of a task or function");
		auto lhs = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		lhs->str = extra->current_function_or_task->str;
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move(lhs), std::move($3)));
		append_attr(node, std::move($1));
		auto stop = std::make_unique<AstNode>(@$, AST_TCALL);
		stop->str = "\\return";
		extra->saveChild(std::move(stop));
	} |
	attr TOK_RETURN TOK_SEMICOL {
		if (extra->current_function_or_task == nullptr)
			err_at_loc(@2, "return outside of a task or function");
		auto stop = std::make_unique<AstNode>(@$, AST_TCALL);
		stop->str = "\\return";
		append_attr(stop.get(), std::move($1));
		extra->saveChild(std::move(stop));
	} |
	attr TOK_FORK opt_label {
		auto block = std::make_unique<AstNode>(@$, AST_BLOCK);
		if ($3 != nullptr)
			block->str = *$3;
		AstNode *body = block.get();
		extra->ast_stack.back()->children.push_back(std::move(block));
		extra->ast_stack.push_back(body);
	} fork_stmts fork_join {
		AstNode *node = extra->ast_stack.back();
		// In SystemVerilog, unnamed blocks with block item declarations
		// create an implicit hierarchy scope (IEEE 1800 9.3.5); this also
		// applies to fork bodies, e.g. `fork automatic int k = ...; join_none`.
		if (mode->sv && node->str.empty())
		    for (auto& child : node->children)
			if (child->type == AST_WIRE || child->type == AST_MEMORY || child->type == AST_PARAMETER
				|| child->type == AST_LOCALPARAM || child->type == AST_TYPEDEF) {
			    node->str = "$unnamed_block$" + std::to_string(autoidx++);
			    break;
			}
		extra->ast_stack.pop_back();
		parse_degrade(extra, @2, "fork/join runs its branches as one sequential block");
		(void)$1;
	} |
	attr TOK_DISABLE TOK_FORK TOK_SEMICOL {
		parse_degrade(extra, @2, "`disable fork' is dropped");
		(void)$1;
	} |
	attr TOK_WAIT TOK_FORK TOK_SEMICOL {
		parse_degrade(extra, @2, "`wait fork' is dropped");
		(void)$1;
	} |
	attr TOK_WAIT TOK_LPAREN expr TOK_RPAREN opt_semi {
		parse_degrade(extra, @2, "`wait (...)' is dropped: the statements after it run without waiting");
		(void)$1;
		(void)$4;
	} |
	attr TOK_DISABLE hierarchical_id TOK_SEMICOL {
		auto id = std::make_unique<AstNode>(@3, AST_IDENTIFIER);
		id->str = *$3;
		auto node = mk_sva(@$, "disable");
		node->children.push_back(std::move(id));
		append_attr(node.get(), std::move($1));
		extra->saveChild(std::move(node));
	} |
	attr TOK_FOREACH TOK_LPAREN hierarchical_id TOK_LBRA {
		auto node = std::make_unique<AstNode>(@$, AST_SVA);
		node->str = "foreach";
		auto arr = std::make_unique<AstNode>(@4, AST_IDENTIFIER);
		arr->str = *$4;
		node->children.push_back(std::move(arr));
		append_attr(node.get(), std::move($1));
		extra->pushChild(std::move(node));
	} foreach_vars TOK_RBRA foreach_dotted_opt TOK_RPAREN {
		// foreach over a member of the indexed element uses
		// `foreach (successors[s].m_predecessors[p])`: the extra loop
		// variables were appended to the foreach node by foreach_var, and
		// the array becomes the dotted member name.
		if ($9 != nullptr) {
			std::string member = *$9;
			if (!member.empty() && member[0] == '\\')
				member.erase(0, 1);
			extra->ast_stack.back()->children[0]->str += "." + member;
		}
		auto block = std::make_unique<AstNode>(@$, AST_BLOCK);
		AstNode *body = block.get();
		extra->ast_stack.back()->children.push_back(std::move(block));
		extra->ast_stack.push_back(body);
	} behavioral_stmt {
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	} |
	attr TOK_FOREVER behavioral_stmt {
		parse_degrade(extra, @2, "`forever' runs its body once");
		(void)$1;
	} |
	attr TOK_RANDCASE {
		extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} randcase_items TOK_ENDCASE {
		extra->ast_stack.pop_back();
	} |
	attr TOK_RANDSEQUENCE {
		// random sequence generation (IEEE 1800-2009 18.17,
		// simulation-only): the production-selection structure is not
		// modeled; code-block statements are kept in source order, like
		// the randcase degrade above
		extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} opt_rs_production_id rs_productions TOK_ENDSEQUENCE {
		extra->ast_stack.pop_back();
	} |
	attr proc_at TOK_LPAREN event_wait_list TOK_RPAREN TOK_SEMICOL {
		parse_degrade(extra, @2, "procedural event control `@(...);' is dropped: the statements after it run without waiting");
		(void)$1;
	} |
	attr proc_at rvalue behavioral_stmt {
		// wait-on-event statement, e.g. `@waiter.trigger;` or
		// `@ev a = 1;` (IEEE 9.4.2, simulation-only): the wait itself is
		// not modeled, but the statement body stays in the flow so it
		// still elaborates (a bare `;` body keeps `@ev;` a plain no-op)
		parse_degrade(extra, @2, "procedural event wait `@ev' is dropped: its statement runs without waiting");
		(void)$1;
		(void)$3;
	} |
	attr TOK_ARROW rvalue TOK_SEMICOL {
		// event trigger statement, e.g. `->w.trigger;` (simulation-only)
		parse_degrade(extra, @2, "event trigger `->' is dropped");
		(void)$1;
		(void)$3;
	} |
	attr TOK_NB_ARROW rvalue TOK_SEMICOL {
		// nonblocking event trigger statement, e.g. `->>top.e;`
		// (IEEE 15.5.1, simulation-only)
		parse_degrade(extra, @2, "event trigger `->>' is dropped");
		(void)$1;
		(void)$3;
	} |
	attr hierarchical_id TOK_ID TOK_SEMICOL {
		auto wire = std::make_unique<AstNode>(@$, AST_WIRE);
		wire->str = *$3;
		extra->addWiretypeNode($2.get(), wire.get());
		append_attr(wire.get(), std::move($1));
		extra->saveChild(std::move(wire));
	} |
	attr hierarchical_id TOK_ID TOK_EQ expr TOK_SEMICOL {
		auto wire = std::make_unique<AstNode>(@$, AST_WIRE);
		wire->str = *$3;
		extra->addWiretypeNode($2.get(), wire.get());
		append_attr(wire.get(), std::move($1));
		extra->saveChild(std::move(wire));
		auto ident = std::make_unique<AstNode>(@3, AST_IDENTIFIER);
		ident->str = *$3;
		extra->saveChild(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move(ident), std::move($5)));
	} |
	attr TOK_FORCE lvalue TOK_EQ expr TOK_SEMICOL {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move($3), std::move($5)));
		node->set_attribute(ID(sv_force), AstNode::mkconst_int(@2, 1, false));
		append_attr(node, std::move($1));
	} |
	attr TOK_ASSIGN lvalue TOK_EQ expr TOK_SEMICOL {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move($3), std::move($5)));
		node->set_attribute(ID(sv_force), AstNode::mkconst_int(@2, 1, false));
		append_attr(node, std::move($1));
	} |
	attr TOK_RELEASE lvalue TOK_SEMICOL {
		auto node = std::make_unique<AstNode>(@$, AST_TCALL);
		node->str = "\\release";
		node->children.push_back(std::move($3));
		append_attr(node.get(), std::move($1));
		extra->saveChild(std::move(node));
	} |
	attr TOK_DEASSIGN lvalue TOK_SEMICOL {
		auto node = std::make_unique<AstNode>(@$, AST_TCALL);
		node->str = "\\deassign";
		node->children.push_back(std::move($3));
		append_attr(node.get(), std::move($1));
		extra->saveChild(std::move(node));
	} |
	attr hierarchical_id TOK_HASH TOK_LPAREN class_type_arg TOK_RPAREN TOK_PACKAGESEP TOK_ID {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_TCALL));
		std::string base = *$2;
		if (!base.empty() && base[0] == '\\')
			base.erase(0, 1);
		std::string method = *$8;
		if (!method.empty() && method[0] == '\\')
			method.erase(0, 1);
		node->str = "\\" + base + "::" + method;
		append_attr(node, std::move($1));
	} TOK_LPAREN arg_list optional_comma TOK_RPAREN opt_semi {
		extra->ast_stack.pop_back();
	} |
	attr hierarchical_id {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_TCALL));
		node->str = *$2;
		append_attr(node, std::move($1));
	} call_finish {
		extra->ast_stack.pop_back();
	} |
	attr rvalue_call_stmt TOK_SEMICOL {
		// Method call on an indexed element used as a statement, e.g.
		// `q[i].delete();`. Modeled as an AST_TCALL with a ".method" name
		// and the indexed receiver as the first child.
		if ($2 != nullptr) {
			auto node = std::make_unique<AstNode>(@$, AST_TCALL);
			node->str = $2->str;
			node->children = std::move($2->children);
			append_attr(node.get(), std::move($1));
			extra->saveChild(std::move(node));
		}
	} |
	TOK_VOID OP_CAST TOK_LPAREN expr TOK_RPAREN TOK_SEMICOL {
		(void)$4;
	} |
	attr TOK_MSG_TASKS {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_TCALL));
		node->str = *$2;
		append_attr(node, std::move($1));
	} opt_arg_list TOK_SEMICOL{
		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @5);
		extra->ast_stack.pop_back();
	} |
	attr TOK_ID TOK_COL attr TOK_BEGIN {
		extra->enterTypeScope();
	} opt_label {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
		append_attr(node, std::move($1));
		if ($7 != nullptr)
			node->str = *$7;
		else
			node->str = *$2;
	} behavioral_stmt_list TOK_END opt_label {
		extra->exitTypeScope();
		std::string eff_label = ($7 != nullptr) ? *$7 : *$2;
		checkLabelsMatch(@10, "Begin label", &eff_label, $11.get());
		AstNode *node = extra->ast_stack.back();
		if (mode->sv && node->str.empty())
		    for (auto& child : node->children)
			if (child->type == AST_WIRE || child->type == AST_MEMORY || child->type == AST_PARAMETER
				|| child->type == AST_LOCALPARAM || child->type == AST_TYPEDEF) {
			    node->str = "$unnamed_block$" + std::to_string(autoidx++);
			    break;
			}
		SET_AST_NODE_LOC(extra->ast_stack.back(), @5, @10);
		extra->ast_stack.pop_back();
	} |
	attr TOK_ID TOK_COL attr TOK_FORK opt_label fork_stmts fork_join {
		parse_degrade(extra, @4, "fork/join runs its branches as one sequential block");
		(void)$1;
	} |
	attr TOK_BEGIN {
		extra->enterTypeScope();
	} opt_label {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
		append_attr(node, std::move($1));
		if ($4 != nullptr)
			node->str = *$4;
	} behavioral_stmt_list TOK_END opt_label {
		extra->exitTypeScope();
		checkLabelsMatch(@8, "Begin label", $4.get(), $8.get());
		AstNode *node = extra->ast_stack.back();
		// In SystemVerilog, unnamed blocks with block item declarations
		// create an implicit hierarchy scope
		if (mode->sv && node->str.empty())
		    for (auto& child : node->children)
			if (child->type == AST_WIRE || child->type == AST_MEMORY || child->type == AST_PARAMETER
				|| child->type == AST_LOCALPARAM || child->type == AST_TYPEDEF) {
			    node->str = "$unnamed_block$" + std::to_string(autoidx++);
			    break;
			}
		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @8);
		extra->ast_stack.pop_back();
	} |
	attr TOK_FOR TOK_LPAREN {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_FOR));
		append_attr(node, std::move($1));
	} for_initialization for_init_extra TOK_SEMICOL expr {
		extra->ast_stack.back()->children.push_back(std::move($8));
	} TOK_SEMICOL for_steps TOK_RPAREN {
		AstNode* block = extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
		block->str = "$for_loop$" + std::to_string(autoidx++);
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @14, @14);
		extra->ast_stack.pop_back();
		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @14);
		// `for (int i = 0, T t = a; i < n; i++, t = f(t))`: keep the loop
		// node at [init, cond, step, body] (simplify resolves only the
		// first three as the loop header). The extra initializers run once
		// before the loop, the extra steps at the end of each body.
		AstNode *loop = extra->ast_stack.back();
		int nch = GetSize(loop->children);
		int n_inits = 0;
		while (1 + n_inits < nch - 1 && loop->children[1 + n_inits]->type == AST_ASSIGN_EQ)
			n_inits++;
		// [init, inits..., cond, step, steps..., body]
		int step_at = 1 + n_inits + 1;
		int n_steps = nch - 1 - step_at - 1;
		if (n_inits > 0 || n_steps > 0) {
			AstNode *body = loop->children.back().get();
			if (n_steps > 0) {
				std::function<bool(AstNode *)> has_continue = [&](AstNode *n) {
					if (n->type == AST_TCALL && n->str == "\\continue")
						return true;
					if (n->type == AST_FOR || n->type == AST_WHILE || n->type == AST_REPEAT)
						return false;
					for (auto &c : n->children)
						if (c && has_continue(c.get()))
							return true;
					return false;
				};
				if (has_continue(body))
					err_at_loc(@2, "`continue' in a for-loop with more than one step assignment is not supported.");
				for (int k = 0; k < n_steps; k++)
					body->children.push_back(std::move(loop->children[step_at + 1 + k]));
				loop->children.erase(loop->children.begin() + step_at + 1, loop->children.begin() + step_at + 1 + n_steps);
			}
			std::vector<std::unique_ptr<AstNode>> inits;
			for (int k = 0; k < n_inits; k++)
				inits.push_back(std::move(loop->children[1 + k]));
			loop->children.erase(loop->children.begin() + 1, loop->children.begin() + 1 + n_inits);
			// the loop's container: the $fordecl_block wrapper or the
			// enclosing statement list
			AstNode *parent = extra->ast_stack.at(extra->ast_stack.size() - 2);
			AstNode *container = parent;
			if (!parent->children.empty() && parent->children.back().get() != loop &&
					parent->children.back()->type == AST_BLOCK && !parent->children.back()->children.empty() &&
					parent->children.back()->children.back().get() == loop)
				container = parent->children.back().get();
			auto at = container->children.end() - 1;
			log_assert(at->get() == loop);
			for (auto &init : inits) {
				init->set_in_param_flag(false);
				at = container->children.insert(at, std::move(init)) + 1;
			}
		}
		extra->ast_stack.pop_back();
	} |
	attr TOK_DO {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_WHILE));
		node->set_attribute(ID(sv_do), AstNode::mkconst_int(@$, 1, false));
		append_attr(node, std::move($1));
		auto block_owned = std::make_unique<AstNode>(@$, AST_BLOCK);
		auto* block = block_owned.get();
		extra->ast_stack.back()->children.push_back(std::move(block_owned));
		extra->ast_stack.push_back(block);
	} behavioral_stmt TOK_WHILE TOK_LPAREN expr TOK_RPAREN TOK_SEMICOL {
		extra->ast_stack.pop_back();
		extra->ast_stack.back()->children.insert(extra->ast_stack.back()->children.begin(), std::move($7));
		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @9);
		extra->ast_stack.pop_back();
	} |
	attr TOK_BREAK TOK_SEMICOL {
		auto node = std::make_unique<AstNode>(@$, AST_TCALL);
		node->str = "\\break";
		append_attr(node.get(), std::move($1));
		extra->saveChild(std::move(node));
	} |
	attr TOK_CONTINUE TOK_SEMICOL {
		auto node = std::make_unique<AstNode>(@$, AST_TCALL);
		node->str = "\\continue";
		append_attr(node.get(), std::move($1));
		extra->saveChild(std::move(node));
	} |
	attr TOK_WHILE TOK_LPAREN expr TOK_RPAREN {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_WHILE));
		append_attr(node, std::move($1));
		auto block_owned = std::make_unique<AstNode>(@$, AST_BLOCK);
		auto* block = block_owned.get();
		extra->ast_stack.back()->children.push_back(std::move($4));
		extra->ast_stack.back()->children.push_back(std::move(block_owned));
		extra->ast_stack.push_back(block);
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @7, @7);
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	} |
	attr TOK_REPEAT TOK_LPAREN expr TOK_RPAREN {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_REPEAT));
		append_attr(node, std::move($1));
		auto block_owned = std::make_unique<AstNode>(@$, AST_BLOCK);
		auto* block = block_owned.get();
		extra->ast_stack.back()->children.push_back(std::move($4));
		extra->ast_stack.back()->children.push_back(std::move(block_owned));
		extra->ast_stack.push_back(block);
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @7, @7);
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	} |
	if_attr TOK_IF TOK_LPAREN expr TOK_RPAREN {
		std::unique_ptr<AstNode> node_owned;
		AstNode* node = nullptr;
		AstNode *context = extra->ast_stack.back();
		bool patch_block_on_stack = false;
		if (context && context->type == AST_BLOCK && context->get_bool_attribute(ID::promoted_if)) {
			AstNode *outer = extra->ast_stack[extra->ast_stack.size() - 2];
			log_assert (outer && outer->type == AST_CASE);
			if (outer->get_bool_attribute(ID::parallel_case)) {
				// parallel "else if": append condition to outer "if"
				node = outer;
				log_assert (node->children.size());
				node->children.pop_back();
				// `context` has been killed as a grandchild of `outer`
				// we have to undangle it from the stack
				patch_block_on_stack = true;
			} else if (outer->get_bool_attribute(ID::full_case))
				(*$1)[ID::full_case] = AstNode::mkconst_int(@$, 1, false);
		}
		auto expr = std::make_unique<AstNode>(@$, AST_REDUCE_BOOL, std::move($4));
		if (!node) {
			// not parallel "else if": begin new construction
			node_owned = std::make_unique<AstNode>(@$, AST_CASE);
			node = node_owned.get();
			append_attr(node, std::move($1));
			node->children.push_back(node->get_bool_attribute(ID::parallel_case) ? AstNode::mkconst_int(@$, 1, false, 1) : expr->clone());
			extra->ast_stack.back()->children.push_back(std::move(node_owned));
		}
		auto block_owned = std::make_unique<AstNode>(@$, AST_BLOCK);
		auto* block = block_owned.get();
		auto cond_owned = std::make_unique<AstNode>(@$, AST_COND, node->get_bool_attribute(ID::parallel_case) ? std::move(expr) : AstNode::mkconst_int(@$, 1, false, 1), std::move(block_owned));
		SET_AST_NODE_LOC(cond_owned.get(), @4, @4);
		node->children.push_back(std::move(cond_owned));
		// Double it and give it to the next person
		if (patch_block_on_stack)
			extra->ast_stack.back() = block;
		extra->ast_stack.push_back(node);
		extra->ast_stack.push_back(block);
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @7, @7);
	} optional_else {
		extra->ast_stack.pop_back();
		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @9);
		extra->ast_stack.pop_back();
	} |
	case_attr case_type TOK_LPAREN expr TOK_RPAREN {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_CASE, std::move($4)));
		append_attr(node, std::move($1));
		SET_AST_NODE_LOC(extra->ast_stack.back(), @4, @4);
	} opt_synopsys_attr case_body TOK_ENDCASE {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @9);
		extra->case_type_stack.pop_back();
		extra->ast_stack.pop_back();
	} |
	// IEEE 1800-2009 12.6.1: case (expr) matches. A constant pattern uses casez
	// wildcards; an &&& expression must also be true for the item to be selected.
	case_attr case_type TOK_LPAREN expr TOK_RPAREN TOK_MATCHES {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_CASE, std::move($4)));
		node->str = "\\matches";
		append_attr(node, std::move($1));
		extra->case_type_stack.push_back('z');
	} case_matches_body TOK_ENDCASE {
		AstNode *node = extra->ast_stack.back();
		node->children[0] = AstNode::mkconst_int(@6, 1, false);
		SET_AST_NODE_LOC(node, @2, @9);
		extra->case_type_stack.pop_back();
		extra->case_type_stack.pop_back();
		extra->ast_stack.pop_back();
	} |
	// IEEE 1800-2009 12.5.4: case (expr) inside
	case_attr case_type TOK_LPAREN expr TOK_RPAREN TOK_INSIDE {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_CASE, std::move($4)));
		node->str = "\\inside";
		append_attr(node, std::move($1));
	} case_inside_body TOK_ENDCASE {
		AstNode *node = extra->ast_stack.back();
		node->children[0] = AstNode::mkconst_int(@5, 1, false);
		SET_AST_NODE_LOC(node, @2, @8);
		extra->case_type_stack.pop_back();
		extra->ast_stack.pop_back();
	};

if_attr:
	attr {
		$$ = std::move($1);
	} |
	attr TOK_UNIQUE0 {
		if (mode->std_year != 0 && mode->std_year < 2012)
			err_at_loc(@2, "`unique0' requires IEEE 1800-2012 or later; this file is being read as IEEE 1800-%d.", mode->std_year);
		AstNode *context = extra->ast_stack.back();
		if (context && context->type == AST_BLOCK && context->get_bool_attribute(ID::promoted_if))
			err_at_loc(@2, "unique0 keyword cannot be used for 'else if' branch.");
		(*$1)[ID::parallel_case] = AstNode::mkconst_int(@$, 1, false);
		$$ = std::move($1);
	} |
	attr TOK_PRIORITY {
		AstNode *context = extra->ast_stack.back();
		if (context && context->type == AST_BLOCK && context->get_bool_attribute(ID::promoted_if))
			err_at_loc(@2, "priority keyword cannot be used for 'else if' branch.");
		(*$1)[ID::full_case] = AstNode::mkconst_int(@$, 1, false);
		$$ = std::move($1);
	} |
	attr TOK_UNIQUE {
		AstNode *context = extra->ast_stack.back();
		if (context && context->type == AST_BLOCK && context->get_bool_attribute(ID::promoted_if))
			err_at_loc(@2, "unique keyword cannot be used for 'else if' branch.");
		(*$1)[ID::full_case] = AstNode::mkconst_int(@$, 1, false);
		(*$1)[ID::parallel_case] = AstNode::mkconst_int(@$, 1, false);
		$$ = std::move($1);
	};

case_attr:
	attr {
		$$ = std::move($1);
	} |
	attr TOK_UNIQUE0 {
		if (mode->std_year != 0 && mode->std_year < 2012)
			err_at_loc(@2, "`unique0' requires IEEE 1800-2012 or later; this file is being read as IEEE 1800-%d.", mode->std_year);
		(*$1)[ID::parallel_case] = AstNode::mkconst_int(@$, 1, false);
		$$ = std::move($1);
	} |
	attr TOK_PRIORITY {
		(*$1)[ID::full_case] = AstNode::mkconst_int(@$, 1, false);
		$$ = std::move($1);
	} |
	attr TOK_UNIQUE {
		(*$1)[ID::full_case] = AstNode::mkconst_int(@$, 1, false);
		(*$1)[ID::parallel_case] = AstNode::mkconst_int(@$, 1, false);
		$$ = std::move($1);
	};

case_type:
	TOK_CASE {
		extra->case_type_stack.push_back(0);
	} |
	TOK_CASEX {
		extra->case_type_stack.push_back('x');
	} |
	TOK_CASEZ {
		extra->case_type_stack.push_back('z');
	};

opt_synopsys_attr:
	opt_synopsys_attr TOK_SYNOPSYS_FULL_CASE {
		if (extra->ast_stack.back()->attributes.count(ID::full_case) == 0)
			extra->ast_stack.back()->attributes[ID::full_case] = AstNode::mkconst_int(@$, 1, false);
	} |
	opt_synopsys_attr TOK_SYNOPSYS_PARALLEL_CASE {
		if (extra->ast_stack.back()->attributes.count(ID::parallel_case) == 0)
			extra->ast_stack.back()->attributes[ID::parallel_case] = AstNode::mkconst_int(@$, 1, false);
	} |
	%empty;

behavioral_stmt_list:
	behavioral_stmt_list behavioral_stmt |
	%empty;

optional_else:
	TOK_ELSE {
		extra->ast_stack.pop_back();
		auto block_owned = std::make_unique<AstNode>(@$, AST_BLOCK);
		auto* block = block_owned.get();
		block->attributes[ID::promoted_if] = AstNode::mkconst_int(@$, 1, false);
		AstNode* cond = extra->saveChild(
			std::make_unique<AstNode>(@$, AST_COND,
				std::make_unique<AstNode>(@$, AST_DEFAULT),
				std::move(block_owned)));
		extra->ast_stack.push_back(block);
		SET_AST_NODE_LOC(cond, @1, @1);
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @3, @3);
	} |
	%empty %prec FAKE_THEN;

case_body:
	case_body case_item |
	%empty;

case_inside_body:
	case_inside_body case_inside_item |
	%empty;

case_matches_body:
	case_matches_body case_matches_item |
	%empty;

case_matches_item:
	expr match_filter TOK_COL {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_COND));
		AstNode *cas = extra->ast_stack.at(extra->ast_stack.size() - 2);
		auto match = mk_sva(@1, "meq");
		match->children.push_back(cas->children[0]->clone());
		match->children.push_back(std::move($1));
		if ($2) {
			match = std::make_unique<AstNode>(@$, AST_LOGIC_AND, std::move(match), std::move($2));
		}
		extra->ast_stack.back()->children.push_back(std::move(match));
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @4, @4);
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	} |
	TOK_DEFAULT TOK_COL {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_COND));
		AstNode *def = extra->saveChild(std::make_unique<AstNode>(@1, AST_DEFAULT));
		SET_AST_NODE_LOC(def, @1, @1);
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @3, @3);
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	};

match_filter:
	TOK_SPECIFY_AND expr { $$ = std::move($2); } |
	TOK_IGNORED_SPECIFY_AND expr { $$ = std::move($2); } |
	%empty { $$ = nullptr; };

case_inside_item:
	sva_inside_items TOK_COL {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_COND));
		AstNode *cas = extra->ast_stack.at(extra->ast_stack.size() - 2);
		auto inside = mk_sva(@1, "inside");
		inside->children.push_back(cas->children[0]->clone());
		inside->children.push_back(std::move($1));
		extra->ast_stack.back()->children.push_back(std::move(inside));
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @4, @4);
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	} |
	TOK_DEFAULT TOK_COL {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_COND));
		AstNode *def = extra->saveChild(std::make_unique<AstNode>(@1, AST_DEFAULT));
		SET_AST_NODE_LOC(def, @1, @1);
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @4, @4);
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	} |
	TOK_DEFAULT {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_COND));
		AstNode *def = extra->saveChild(std::make_unique<AstNode>(@1, AST_DEFAULT));
		SET_AST_NODE_LOC(def, @1, @1);
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} behavioral_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @3, @3);
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	};

case_item:
	{
		(void)extra->pushChild(std::make_unique<AstNode>(
				@$,
				extra->case_type_stack.size() && extra->case_type_stack.back() == 'x' ? AST_CONDX :
				extra->case_type_stack.size() && extra->case_type_stack.back() == 'z' ? AST_CONDZ : AST_COND));
	} case_select {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
		extra->case_type_stack.push_back(0);
	} behavioral_stmt {
		extra->case_type_stack.pop_back();
		SET_AST_NODE_LOC(extra->ast_stack.back(), @4, @4);
		extra->ast_stack.pop_back();
		extra->ast_stack.pop_back();
	};

gen_case_body:
	gen_case_body gen_case_item |
	%empty;

gen_case_item:
	{
		(void)extra->pushChild(std::make_unique<AstNode>(
				@$,
				extra->case_type_stack.size() && extra->case_type_stack.back() == 'x' ? AST_CONDX :
				extra->case_type_stack.size() && extra->case_type_stack.back() == 'z' ? AST_CONDZ : AST_COND));
	} case_select {
		extra->case_type_stack.push_back(0);
		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @2);
	} gen_stmt_block {
		extra->case_type_stack.pop_back();
		extra->ast_stack.pop_back();
	};

case_select:
	case_expr_list TOK_COL |
	TOK_DEFAULT;

case_expr_list:
	TOK_DEFAULT {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_DEFAULT));
		SET_AST_NODE_LOC(node, @1, @1);
	} |
	TOK_SVA_LABEL {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_IDENTIFIER));
		node->str = *$1;
		SET_AST_NODE_LOC(node, @1, @1);
	} |
	expr {
		extra->ast_stack.back()->children.push_back(std::move($1));
	} |
	case_expr_list TOK_COMMA expr {
		extra->ast_stack.back()->children.push_back(std::move($3));
	};

rvalue:
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT rvalue {
		$$ = std::make_unique<AstNode>(@$, AST_PREFIX, std::move($3), std::move($6));
		$$->str = *$1;
		SET_AST_NODE_LOC($$.get(), @1, @6);
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@5, "Method calls are only supported in SystemVerilog mode.");
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@5, "Method calls are only supported in SystemVerilog mode.");
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr TOK_COMMA expr TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@5, "Method calls are only supported in SystemVerilog mode.");
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
		$$->children.push_back(std::move($10));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr TOK_COMMA expr TOK_COMMA expr TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@5, "Method calls are only supported in SystemVerilog mode.");
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
		$$->children.push_back(std::move($10));
		$$->children.push_back(std::move($12));
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_DOT TOK_ID TOK_LPAREN expr call_args_tail TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@5, "Method calls are only supported in SystemVerilog mode.");
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		auto idx = std::make_unique<AstNode>(@2, AST_RANGE);
		idx->children.push_back(std::move($3));
		recv->children.push_back(std::move(idx));
		std::string method = *$6;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($8));
		for (auto &child : $9->children)
			$$->children.push_back(std::move(child));
	} |
	hierarchical_id range {
		$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER, std::move($2));
		$$->str = *$1;
		SET_AST_NODE_LOC($$.get(), @1, @1);
		if ($2 == nullptr && ($$->str == "\\$initstate" ||
				$$->str == "\\$anyconst" || $$->str == "\\$anyseq" ||
				$$->str == "\\$allconst" || $$->str == "\\$allseq"))
			$$->type = AST_FCALL;
	} |
	hierarchical_id non_opt_multirange {
		$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER, std::move($2));
		$$->str = *$1;
		SET_AST_NODE_LOC($$.get(), @1, @1);
	} |
	hierarchical_id non_opt_multirange TOK_DOT TOK_ID range {
		std::string member = *$4;
		if (!member.empty() && member[0] == '\\')
			member.erase(0, 1);
		std::string base = *$1;
		if (!base.empty() && base[0] == '\\')
			base.erase(0, 1);
		$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		$$->str = "\\" + base + "." + member;
		SET_AST_NODE_LOC($$.get(), @1, @4);
		if ($5)
			$$->children.push_back(std::move($5));
	} |
	hierarchical_id non_opt_multirange TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@4, "Method calls are only supported in SystemVerilog mode.");
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		recv->children.push_back(std::move($2));
		std::string method = *$4;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
	} |
	hierarchical_id non_opt_multirange TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@4, "Method calls are only supported in SystemVerilog mode.");
		auto recv = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		recv->str = *$1;
		recv->children.push_back(std::move($2));
		std::string method = *$4;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(recv));
		$$->children.push_back(std::move($6));
	};

lvalue:
	rvalue {
		$$ = std::move($1);
	} |
	TOK_LCURL lvalue_concat_list TOK_RCURL {
		$$ = std::move($2);
	} |
	TOK_LCURL OP_SHL expr TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		// streaming assignment lvalue `{<<8{a,b}} = x` (IEEE 11.4.14.4):
		// bit reordering is not modeled; degrades to a plain concat assign
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHR expr TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHL TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($4);
	} |
	TOK_LCURL OP_SHR TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($4);
	} |
	TOK_LCURL OP_SHL TOK_LOGIC TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		// slice size given as a type, e.g. `{<<bit{a}}` (IEEE 11.4.14)
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHR TOK_LOGIC TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHL integer_atom_type TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHR integer_atom_type TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($5);
	};

lvalue_concat_list:
	expr {
		$$ = std::make_unique<AstNode>(@$, AST_CONCAT);
		$$->children.push_back(std::move($1));
	} |
	expr TOK_COMMA lvalue_concat_list {
		$$ = std::move($3);
		$$->children.push_back(std::move($1));
	};

opt_arg_list:
	TOK_LPAREN arg_list optional_comma TOK_RPAREN |
	%empty;

call_finish:
	TOK_SEMICOL |
	TOK_LPAREN arg_list optional_comma TOK_RPAREN rand_with opt_semi |
	rand_with_nonnull opt_semi;

rand_with_nonnull:
	TOK_WITH TOK_LCURL constraint_items TOK_RCURL |
	TOK_WITH TOK_LPAREN expr TOK_RPAREN |
	TOK_WITH TOK_LPAREN expr TOK_RPAREN TOK_LCURL constraint_items TOK_RCURL;

scoped_type_name:
	hierarchical_id { $$ = std::move($1); } |
	hierarchical_type_id { $$ = std::move($1); };

static_method_name:
	TOK_ID {
		$$ = std::move($1);
	} |
	TOK_SCOPED_ID {
		// the lexer glues the first two identifiers of a :: chain into one
		// token, e.g. `#(T)::type_id::create()` yields SCOPED_ID(type_id::create)
		auto s = *$1;
		if (!s.empty() && s[0] == '\\')
			s.erase(0, 1);
		$$ = std::make_unique<std::string>(std::move(s));
	} |
	static_method_name TOK_PACKAGESEP TOK_ID {
		$$ = std::make_unique<std::string>(*$1 + "::" + *$3);
	} |
	static_method_name TOK_PACKAGESEP TOK_SCOPED_ID {
		auto s = *$3;
		if (!s.empty() && s[0] == '\\')
			s.erase(0, 1);
		$$ = std::make_unique<std::string>(*$1 + "::" + std::move(s));
	} |
	static_method_name TOK_DOT TOK_ID {
		$$ = std::make_unique<std::string>(*$1 + "." + *$3);
	};

class_type_arg:
	class_type_core class_type_params |
	class_type_core non_opt_range |
	TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN {
		// named parameter assignment in a parameterized class scope,
		// e.g. `test_cls#(.t(23))::new(...)`; parameter values are
		// discarded like all class parameters
	} |
	TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN {
		// named parameter assignment with empty parens, e.g. `#(.t())`
	};

class_type_args:
	class_type_arg |
	class_type_args TOK_COMMA class_type_arg |
	%empty {
		// empty parameter list, e.g. `par_cls#()::b`
	};

class_type_core:
	TOK_ID |
	TOK_USER_TYPE |
	TOK_LOGIC |
	TOK_INT |
	TOK_INTEGER |
	TOK_INT TOK_UNSIGNED |
	TOK_INTEGER TOK_UNSIGNED |
	TOK_LOGIC TOK_UNSIGNED |
	TOK_SCOPED_ID |
	TOK_VIRTUAL TOK_ID |
	TOK_VIRTUAL TOK_USER_TYPE |
	TOK_VIRTUAL TOK_PKG_USER_TYPE |
	TOK_ID TOK_ID |
	TOK_ID TOK_USER_TYPE;

class_type_params:
	%empty |
	TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN;

class_spec_params:
	class_spec_param |
	class_spec_params TOK_COMMA class_spec_param;

class_spec_param:
	expr |
	TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN |
	TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN |
	hierarchical_type_id TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN |
	TOK_ID TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN |
	TOK_SCOPED_ID TOK_HASH TOK_LPAREN class_spec_params TOK_RPAREN |
	integer_atom_type {
		// a bare type used as a parameter, e.g. `uvm_resource #(int)`;
		// parameter values are discarded
	} |
	integer_atom_type TOK_UNSIGNED {
		// e.g. `uvm_resource #(int unsigned)`; discarded
	} |
	TOK_USER_TYPE {
		// a registered type used as a parameter, e.g. `uvm_resource #(uvm_integral_t)`;
		// discarded
	} |
	type_vec range_or_multirange {
		// e.g. `uvm_resource #(bit[N-1:0])`; discarded
	} |
	TOK_VIRTUAL hierarchical_type_id class_type_params {
		// e.g. `uvm_config_db #(virtual force_if #(...))`; discarded
	};

randcase_items:
	expr TOK_COL behavioral_stmt |
	randcase_items expr TOK_COL behavioral_stmt;

opt_rs_production_id:
	%empty |
	TOK_LPAREN rs_ident TOK_RPAREN;

rs_productions:
	rs_production |
	rs_productions rs_production;

rs_production:
	opt_rs_void rs_ident opt_rs_ports TOK_COL rs_rules TOK_SEMICOL;

rs_ident:
	TOK_ID { $$ = std::move($1); } |
	TOK_USER_TYPE {
		// production names are arbitrary identifiers; the lexer's
		// typedef lookahead may classify a name followed by another name
		// (`a b c`) as a user type, so accept it here
		$$ = std::move($1);
	};

opt_rs_void:
	%empty |
	TOK_VOID |
	TOK_FUNCTION TOK_VOID;

opt_rs_ports:
	%empty |
	TOK_LPAREN rs_ports optional_comma TOK_RPAREN;

rs_ports:
	rs_port |
	rs_ports TOK_COMMA rs_port;

rs_port:
	attr wire_type range_or_multirange TOK_ID {
		// production argument list (IEEE 18.17): parsed for syntax and
		// discarded, like the rest of the production structure
		(void)$1;
		(void)$2;
		(void)$3;
		(void)$4;
	};

rs_rules:
	rs_rule |
	rs_rules TOK_PIPE rs_rule;

rs_rule:
	rs_prod_list opt_rs_weight;

opt_rs_weight:
	%empty |
	TOK_COL TOK_EQ rs_weight opt_rs_weight_parens;

rs_weight:
	TOK_CONSTVAL {
		(void)$1;
	} |
	TOK_ID {
		(void)$1;
	} |
	TOK_SCOPED_ID {
		(void)$1;
	};

opt_rs_weight_parens:
	%empty |
	TOK_LPAREN rs_weight TOK_RPAREN;

rs_prod_list:
	rs_prods |
	TOK_RAND TOK_JOIN opt_rs_join_expr rs_prods;

opt_rs_join_expr:
	%empty |
	TOK_LPAREN expr TOK_RPAREN;

rs_prods:
	rs_prod |
	rs_prods rs_prod;

rs_prod:
	rs_prod_item |
	rs_code_block |
	rs_if_else |
	rs_repeat |
	rs_case;

rs_prod_item:
	rs_ident opt_rs_args;

opt_rs_args:
	%empty |
	TOK_LPAREN rs_arg_list TOK_RPAREN;

rs_arg_list:
	expr {
		(void)$1;
	} |
	rs_arg_list TOK_COMMA expr {
		(void)$3;
	};

rs_code_block:
	TOK_LCURL {
		extra->pushChild(std::make_unique<AstNode>(@$, AST_BLOCK));
	} rs_block_items TOK_RCURL {
		extra->ast_stack.pop_back();
	};

rs_block_items:
	%empty |
	rs_block_items behavioral_stmt;

rs_if_else:
	TOK_IF TOK_LPAREN expr TOK_RPAREN rs_prod_item opt_rs_else;

opt_rs_else:
	%empty |
	TOK_ELSE rs_prod_item;

rs_repeat:
	TOK_REPEAT TOK_LPAREN expr TOK_RPAREN rs_prod_item;

rs_case:
	TOK_CASE TOK_LPAREN expr TOK_RPAREN rs_case_items TOK_ENDCASE;

rs_case_items:
	rs_case_item |
	rs_case_items rs_case_item;

rs_case_item:
	expr TOK_COL rs_prods TOK_SEMICOL |
	TOK_DEFAULT TOK_COL rs_prods TOK_SEMICOL;

opt_semi:
	TOK_SEMICOL |
	%empty;

rand_with:
	%empty |
	TOK_WITH TOK_LCURL constraint_items TOK_RCURL |
	TOK_WITH TOK_LPAREN expr TOK_RPAREN |
	TOK_WITH TOK_LPAREN expr TOK_RPAREN TOK_LCURL constraint_items TOK_RCURL;

// optional method call chained onto a call result, e.g. `f().g(1)`. The
// returned node carries the method name as its str and the argument (if any)
// as its only child.
opt_chained_method:
	%empty { $$ = nullptr; } |
	chained_method { $$ = std::move($1); };

chained_method:
	TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN {
		$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		$$->str = *$2;
	} |
	TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN {
		$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		$$->str = *$2;
		$$->children.push_back(std::move($4));
	};
constraint_items:
	%empty |
	constraint_items expr TOK_SEMICOL |
	constraint_items TOK_SEMICOL |
	constraint_items TOK_SOFT expr TOK_SEMICOL {
		// soft constraint (IEEE 1800-2009 18.5.12): the softness is a
		// solver directive; degrade to a plain constraint expression
		extra->ast_stack.back()->children.push_back(std::move($3));
	} |
	constraint_items expr TOK_DIST TOK_LCURL dist_item_list TOK_RCURL TOK_SEMICOL {
		// distribution constraint `x dist {3 := 1, 10 := 0}` (IEEE
		// 18.5.13): the distribution is not modeled; the base expression
		// stays so the item still elaborates
		extra->ast_stack.back()->children.push_back(std::move($2));
	};

dist_item_list:
	dist_item |
	dist_item_list TOK_COMMA dist_item;

dist_item:
	expr opt_dist_weight {
		(void)$1;
	};

opt_dist_weight:
	%empty |
	TOK_COL TOK_EQ expr {
		(void)$3;
	} |
	TOK_COL TOK_SLASH expr {
		(void)$3;
	};

arg_list:
	arg_list2 |
	%empty;

arg_list2:
	single_arg |
	arg_list TOK_COMMA single_arg |
	arg_list TOK_COMMA TOK_COMMA single_arg {
		// empty positional argument between commas, e.g. `f(a,,b)`
	} |
	arg_list TOK_COMMA TOK_COMMA {
		// empty positional arguments before ')' or ',', e.g. `f(a,,)`
	};

single_arg:
	expr {
		extra->ast_stack.back()->children.push_back(std::move($1));
	} |
	sva_at_clock {
		extra->ast_stack.back()->children.push_back(std::move($1));
	} |
	TOK_USER_TYPE {
		auto id = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		id->str = *$1;
		extra->ast_stack.back()->children.push_back(std::move(id));
	} |
	integer_atom_type {
		// a bare type used as a system function argument, e.g. `$bits(int)`
		extra->ast_stack.back()->children.push_back(AstNode::mkconst_int(@1, $1, true));
	} |
	integer_vector_type {
		// a bare 1-bit vector type, e.g. `$typename(logic)`
		extra->ast_stack.back()->children.push_back(AstNode::mkconst_int(@1, 1, true));
	} |
	TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN {
		extra->ast_stack.back()->children.push_back(std::move($4));
	};

module_gen_body:
	module_gen_body gen_stmt_or_module_body_stmt |
	module_gen_body gen_block |
	%empty;

gen_stmt_or_module_body_stmt:
	gen_stmt | module_body_stmt |
	attr TOK_SEMICOL;

genvar_identifier:
	TOK_ID {
		$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		$$->str = *$1;
	};

genvar_initialization:
	TOK_GENVAR genvar_identifier {
		err_at_loc(@2, "Generate for loop variable declaration is missing initialization!");
	} |
	TOK_GENVAR genvar_identifier TOK_EQ expr {
		if (!mode->sv)
			err_at_loc(@3, "Generate for loop inline variable declaration is only supported in SystemVerilog mode!");
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_GENVAR));
		node->is_reg = true;
		node->is_signed = true;
		node->range_left = 31;
		node->range_right = 0;
		node->str = $2->str;
		node->children.push_back(checkRange(node, nullptr));
		SET_AST_NODE_LOC(node, @1, @4);
		node = extra->saveChild(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move($2), std::move($4)));
		SET_AST_NODE_LOC(node, @1, @4);
	} |
	genvar_identifier TOK_EQ expr {
		AstNode* node = extra->saveChild(std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move($1), std::move($3)));
		SET_AST_NODE_LOC(node, @1, @3);
	};

// this production creates the obligatory if-else shift/reduce conflict
gen_stmt:
	TOK_FOR TOK_LPAREN {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_GENFOR));
	} genvar_initialization TOK_SEMICOL expr {
		extra->ast_stack.back()->children.push_back(std::move($6));
	} TOK_SEMICOL simple_behavioral_stmt TOK_RPAREN gen_stmt_block {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @1, @11);
		extra->rewriteGenForDeclInit(extra->ast_stack.back());
		extra->ast_stack.pop_back();
	} |
	TOK_IF TOK_LPAREN expr TOK_RPAREN {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_GENIF));
		extra->ast_stack.back()->children.push_back(std::move($3));
	} gen_stmt_block opt_gen_else {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @1, @7);
		extra->ast_stack.pop_back();
	} |
	case_type TOK_LPAREN expr TOK_RPAREN {
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_GENCASE, std::move($3)));
	} gen_case_body TOK_ENDCASE {
		extra->case_type_stack.pop_back();
		SET_AST_NODE_LOC(extra->ast_stack.back(), @1, @7);
		extra->ast_stack.pop_back();
	} |
	TOK_MSG_TASKS {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_TECALL));
		node->str = *$1;
	} opt_arg_list TOK_SEMICOL{
		SET_AST_NODE_LOC(extra->ast_stack.back(), @1, @3);
		extra->ast_stack.pop_back();
	};

gen_block:
	TOK_BEGIN {
		extra->enterTypeScope();
	} opt_label {
		AstNode* node = extra->pushChild(std::make_unique<AstNode>(@$, AST_GENBLOCK));
		node->str = $3 ? *$3 : std::string();
	} module_gen_body TOK_END opt_label {
		extra->exitTypeScope();
		checkLabelsMatch(@7, "Begin label", $3.get(), $7.get());
		SET_AST_NODE_LOC(extra->ast_stack.back(), @1, @7);
		extra->ast_stack.pop_back();
	};

// result is wrapped in a genblock only if necessary
gen_stmt_block:
	{
		(void)extra->pushChild(std::make_unique<AstNode>(@$, AST_GENBLOCK));
	} gen_stmt_or_module_body_stmt {
		SET_AST_NODE_LOC(extra->ast_stack.back(), @2, @2);
		extra->ast_stack.pop_back();
	} | gen_block;

opt_gen_else:
	TOK_ELSE gen_stmt_block | %empty %prec FAKE_THEN;

expr:
	basic_expr {
		$$ = std::move($1);
	} |
	basic_expr TOK_QUE attr expr TOK_COL expr {
		$$ = std::make_unique<AstNode>(@$, AST_TERNARY);
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($4));
		$$->children.push_back(std::move($6));
		SET_AST_NODE_LOC($$.get(), @1, @$);
		append_attr($$.get(), std::move($3));
	} |
	inc_or_dec_op attr rvalue {
		$$ = extra->addIncOrDecExpr(std::move($3), std::move($2), $1, location_range(@1, @3), false, mode->sv);
	} |
	// TODO: Attributes are allowed in the middle here, but they create some
	// non-trivial conflicts that don't seem worth solving for now.
	rvalue inc_or_dec_op {
		$$ = extra->addIncOrDecExpr(std::move($1), nullptr, $2, location_range(@1, @2), true, mode->sv);
	};

basic_expr:
	rvalue {
		$$ = std::move($1);
	} |
	TOK_LPAREN expr TOK_RPAREN integral_number {
		if ($4->compare(0, 1, "'") != 0)
			err_at_loc(@4, "Cast operation must be applied on sized constants e.g. (<expr>)<constval> , while %s is not a sized constant.", *$4);
		ConstParser p{@4};
		auto val = p.const2ast(*$4, extra->case_type_stack.size() == 0 ? 0 : extra->case_type_stack.back(), !mode->lib);
		if (val == nullptr)
			log_error("Value conversion failed: `%s'\n", *$4);
		$$ = std::make_unique<AstNode>(@$, AST_TO_BITS, std::move($2), std::move(val));
	} |
	hierarchical_id integral_number {
		if ($2->compare(0, 1, "'") != 0)
			err_at_loc(@2, "Cast operation must be applied on sized constants, e.g. <ID>\'d0, while %s is not a sized constant.", *$2);
		auto bits = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		bits->str = *$1;
		SET_AST_NODE_LOC(bits.get(), @1, @1);
		ConstParser p{@2};
		auto val = p.const2ast(*$2, extra->case_type_stack.size() == 0 ? 0 : extra->case_type_stack.back(), !mode->lib);
		SET_AST_NODE_LOC(val.get(), @2, @2);
		if (val == nullptr)
			log_error("Value conversion failed: `%s'\n", *$2);
		$$ = std::make_unique<AstNode>(@$, AST_TO_BITS, std::move(bits), std::move(val));
	} |
	integral_number {
		ConstParser p{@1};
		$$ = p.const2ast(*$1, extra->case_type_stack.size() == 0 ? 0 : extra->case_type_stack.back(), !mode->lib);
		SET_AST_NODE_LOC($$.get(), @1, @1);
		if ($$ == nullptr)
			log_error("Value conversion failed: `%s'\n", *$1);
	} |
	TOK_REALVAL {
		$$ = std::make_unique<AstNode>(@$, AST_REALVALUE);
		char *p = (char*)malloc(GetSize(*$1) + 1), *q;
		for (int i = 0, j = 0; j < GetSize(*$1); j++)
			if ((*$1)[j] != '_')
				p[i++] = (*$1)[j], p[i] = 0;
		$$->realvalue = strtod(p, &q);
		SET_AST_NODE_LOC($$.get(), @1, @1);
		log_assert(*q == 0);
		free(p);
	} |
	TOK_STRING %prec UNARY_OPS {
		$$ = AstNode::mkconst_str(@1, *$1);
		SET_AST_NODE_LOC($$.get(), @1, @1);
	} |
	TOK_TIME_SCALE {
		$$ = AstNode::mkconst_int(@1, 0, false);
	} |
	scoped_type_name TOK_HASH TOK_LPAREN class_type_args TOK_RPAREN TOK_PACKAGESEP static_method_name {
		if (!mode->sv)
			err_at_loc(@6, "Scoped class member access is only supported in SystemVerilog mode.");
		// member access through a parameterized type scope, e.g.
		// `uvm_callbacks#(T,CB)::m_t_inst.m_tw_cb_q`
		std::string base = *$1;
		if (!base.empty() && base[0] == '\\')
			base.erase(0, 1);
		std::string member = *$7;
		if (!member.empty() && member[0] == '\\')
			member.erase(0, 1);
		$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		$$->str = "\\" + base + "::" + member;
		SET_AST_NODE_LOC($$.get(), @1, @7);
	} |
	scoped_type_name TOK_HASH TOK_LPAREN class_type_args TOK_RPAREN TOK_PACKAGESEP static_method_name attr <ast_t>{
		std::string base = *$1;
		if (!base.empty() && base[0] == '\\')
			base.erase(0, 1);
		std::string method = *$7;
		if (!method.empty() && method[0] == '\\')
			method.erase(0, 1);
		auto fcall = std::make_unique<AstNode>(@1, AST_FCALL);
		AstNode *fcall_node = fcall.get();
		fcall_node->str = "\\" + base + "::" + method;
		extra->ast_stack.push_back(fcall_node);
		SET_AST_NODE_LOC(fcall_node, @1, @7);
		append_attr(fcall_node, std::move($8));
		$$ = std::move(fcall);
	} TOK_LPAREN arg_list optional_comma TOK_RPAREN {
		log_assert($9 != nullptr);
		$$ = std::move($9);
		extra->ast_stack.pop_back();
	} |
	hierarchical_id TOK_LBRA expr TOK_RBRA TOK_LPAREN expr TOK_RPAREN {
		$$ = std::move($6);
	} |
	hierarchical_id rand_with_nonnull {
		// queue method with a with-clause but no argument list, e.g.
		// `avail_sequences.find with (item == i)`
		if (!mode->sv)
			err_at_loc(@1, "Method calls are only supported in SystemVerilog mode.");
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = *$1;
		SET_AST_NODE_LOC($$.get(), @1, @2);
	} |
	hierarchical_id attr <ast_t>{
		// Here we use "Typed Midrule Actions".
		// https://www.gnu.org/software/bison/manual/html_node/Typed-Midrule-Actions.html
		auto fcall = std::make_unique<AstNode>(@1, AST_FCALL);
		AstNode *fcall_node = fcall.get();
		fcall_node->str = *$1;
		extra->ast_stack.push_back(fcall_node);
		SET_AST_NODE_LOC(fcall_node, @1, @1);
		append_attr(fcall_node, std::move($2));
		$$ = std::move(fcall);
	} TOK_LPAREN arg_list optional_comma TOK_RPAREN rand_with opt_chained_method {
		log_assert($3 != nullptr);
		auto recv = std::move($3);
		extra->ast_stack.pop_back();
		if ($9 != nullptr) {
			if (!mode->sv)
				err_at_loc(@9, "Method calls are only supported in SystemVerilog mode.");
			std::string method = $9->str;
			if (!method.empty() && method[0] == '\\')
				method = method.substr(1);
			$$ = std::make_unique<AstNode>(@$, AST_FCALL);
			$$->str = "." + method;
			$$->children.push_back(std::move(recv));
			for (auto &child : $9->children)
				$$->children.push_back(std::move(child));
		} else {
			$$ = std::move(recv);
		}
	} |
	TOK_TO_SIGNED attr TOK_LPAREN expr TOK_RPAREN {
		$$ = std::make_unique<AstNode>(@$, AST_TO_SIGNED, std::move($4));
		append_attr($$.get(), std::move($2));
	} |
	TOK_TO_UNSIGNED attr TOK_LPAREN expr TOK_RPAREN {
		$$ = std::make_unique<AstNode>(@$, AST_TO_UNSIGNED, std::move($4));
		append_attr($$.get(), std::move($2));
	} |
	TOK_VOID OP_CAST TOK_LPAREN expr TOK_RPAREN {
		$$ = std::move($4);
	} |
	TOK_LPAREN expr TOK_RPAREN {
		$$ = std::move($2);
	} |
	TOK_LPAREN expr TOK_RPAREN TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@4, "Method calls are only supported in SystemVerilog mode.");
		std::string method = *$5;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move($2));
	} |
	TOK_LPAREN expr TOK_RPAREN TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@4, "Method calls are only supported in SystemVerilog mode.");
		std::string method = *$5;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move($2));
		$$->children.push_back(std::move($7));
	} |
	TOK_LPAREN expr TOK_COL expr TOK_COL expr TOK_RPAREN {
		$$ = std::move($4);
	} |
	TOK_LCURL TOK_RCURL {
		// empty concatenation, accepted by slang/VCS, e.g. `bit x[] = {};`
		$$ = std::make_unique<AstNode>(@$, AST_CONCAT);
	} |
	TOK_TAGGED TOK_ID TOK_LPAREN expr TOK_RPAREN {
		// tagged union pattern `tagged member(value)` (simulation-only
		// construct): the tag bits are not modeled, the payload is kept
		$$ = std::move($4);
	} |
	TOK_TAGGED TOK_ID OP_CAST TOK_LCURL member_pattern_list optional_comma TOK_RCURL {
		// IEEE 1800-2009 12.6.1: tagged union pattern with struct member
		// pattern `tagged member '{...}` (simulation-only construct):
		// approximated as match-anything
		$$ = AstNode::mkconst_int(@$, 1, false);
	} |
	TOK_TAGGED TOK_ID {
		// tag-only value `tagged member` for a member without a payload
		// (simulation-only construct): the tag bits are not modeled
		$$ = AstNode::mkconst_int(@$, 0, false);
	} |
	TOK_ID rvalue {
		// Class copy-constructor call `new <expr>` (IEEE 8.12): `new` lexes
		// as a plain identifier, and an identifier directly followed by an
		// rvalue is only valid here. Object allocation and field copying are
		// not modeled (simulation-only); degrade to a bare `new` exactly
		// like `obj = new;`.
		if (!mode->sv)
			err_at_loc(@1, "Class copy constructor `new <expr>` is only supported in SystemVerilog mode.");
		if (*$1 != "\\new")
			err_at_loc(@1, "Unexpected identifier `%s' followed by an expression; did you mean the class copy constructor `new <expr>`?", $1->c_str() + 1);
		$$ = std::make_unique<AstNode>(@1, AST_IDENTIFIER);
		$$->str = "\\new";
		SET_AST_NODE_LOC($$.get(), @1, @2);
	} |
	TOK_LCURL concat_list TOK_RCURL {
		$$ = std::move($2);
	} |
	TOK_LCURL concat_list TOK_RCURL TOK_LBRA expr TOK_COL expr TOK_RBRA {
		// IEEE 1800-2009 11.4.12: part-select of a concatenation,
		// e.g. `{b, c}[9:6]`; elaborated as a shift and mask
		auto concat = std::move($2);
		SET_AST_NODE_LOC(concat.get(), @1, @3);
		bool const_sel = $5->type == AST_CONSTANT && $5->isConst() && $7->type == AST_CONSTANT && $7->isConst();
		int sel_l = const_sel ? $5->integer : 0;
		int sel_r = const_sel ? $7->integer : 0;
		auto shr = std::make_unique<AstNode>(@$, AST_SHIFT_RIGHT, std::move(concat), std::move($7));
		if (const_sel) {
			uint32_t mask = sel_l >= sel_r ? (sel_l - sel_r + 1 >= 32 ? 0xffffffffu : ((1u << (sel_l - sel_r + 1)) - 1u)) : 0u;
			$$ = std::make_unique<AstNode>(@$, AST_BIT_AND, std::move(shr),
					AstNode::mkconst_int(@$, mask, false));
		} else {
			warn_at_loc(@$, "Non-constant part-select of a concatenation; the result is not truncated.");
			$$ = std::move(shr);
		}
	} |
	OP_CAST TOK_LCURL assignment_pattern_list optional_comma TOK_RCURL {
		if (!mode->sv)
			err_at_loc(@1, "Assignment patterns are only supported in SystemVerilog mode.");
		$$ = std::move($3);
	} |
	TOK_LCURL expr TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::make_unique<AstNode>(@$, AST_REPLICATE, std::move($2), std::move($4));
	} |
	TOK_LCURL OP_SHL expr TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = mk_sva(@$, "stream_l");
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($5));
	} |
	TOK_LCURL OP_SHL TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = mk_sva(@$, "stream_l");
		$$->children.push_back(AstNode::mkconst_int(@$, 1, false));
		$$->children.push_back(std::move($4));
	} |
	TOK_LCURL OP_SHL integer_atom_type TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = mk_sva(@$, "stream_l");
		$$->children.push_back(AstNode::mkconst_int(@$, $3, false));
		$$->children.push_back(std::move($5));
	} |
	TOK_LCURL OP_SHL TOK_LOGIC TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		// slice size given as a vector type, e.g. `{<<bit{a}}`
		$$ = mk_sva(@$, "stream_l");
		$$->children.push_back(AstNode::mkconst_int(@$, 1, false));
		$$->children.push_back(std::move($5));
	} |
	TOK_LCURL OP_SHR expr TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHR integer_atom_type TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHR TOK_LOGIC TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHR TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($4);
	} |
	basic_expr TOK_ARROW basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_LOGIC_OR, std::make_unique<AstNode>(@$, AST_LOGIC_NOT, std::move($1)), std::move($3));
	} |
	basic_expr TOK_IFF basic_expr {
		// IEEE 1800-2009 11.4.7: a <-> b is (a && b) || (!a && !b).
		auto a = std::move($1);
		auto b = std::move($3);
		auto both = std::make_unique<AstNode>(@$, AST_LOGIC_AND, a->clone(), b->clone());
		auto neither = std::make_unique<AstNode>(@$, AST_LOGIC_AND,
				std::make_unique<AstNode>(@$, AST_LOGIC_NOT, std::move(a)),
				std::make_unique<AstNode>(@$, AST_LOGIC_NOT, std::move(b)));
		$$ = std::make_unique<AstNode>(@$, AST_LOGIC_OR, std::move(both), std::move(neither));
	} |
	TOK_TILDE attr basic_expr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_BIT_NOT, std::move($3));
		SET_AST_NODE_LOC($$.get(), @1, @3);
		append_attr($$.get(), std::move($2));
	} |
	basic_expr TOK_AMP attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_BIT_AND, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_NAND attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_BIT_NOT, std::make_unique<AstNode>(@$, AST_BIT_AND, std::move($1), std::move($4)));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr TOK_PIPE attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_BIT_OR, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_NOR attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_BIT_NOT, std::make_unique<AstNode>(@$, AST_BIT_OR, std::move($1), std::move($4)));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr TOK_CARET attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_BIT_XOR, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_XNOR attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_BIT_XNOR, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	TOK_AMP attr basic_expr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_REDUCE_AND, std::move($3));
		SET_AST_NODE_LOC($$.get(), @1, @3);
		append_attr($$.get(), std::move($2));
	} |
	OP_NAND attr basic_expr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_REDUCE_AND, std::move($3));
		SET_AST_NODE_LOC($$.get(), @1, @3);
		append_attr($$.get(), std::move($2));
		$$ = std::make_unique<AstNode>(@$, AST_LOGIC_NOT, std::move($$));
	} |
	TOK_PIPE attr basic_expr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_REDUCE_OR, std::move($3));
		SET_AST_NODE_LOC($$.get(), @1, @3);
		append_attr($$.get(), std::move($2));
	} |
	OP_NOR attr basic_expr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_REDUCE_OR, std::move($3));
		SET_AST_NODE_LOC($$.get(), @1, @3);
		append_attr($$.get(), std::move($2));
		$$ = std::make_unique<AstNode>(@$, AST_LOGIC_NOT, std::move($$));
		SET_AST_NODE_LOC($$.get(), @1, @3);
	} |
	TOK_CARET attr basic_expr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_REDUCE_XOR, std::move($3));
		SET_AST_NODE_LOC($$.get(), @1, @3);
		append_attr($$.get(), std::move($2));
	} |
	OP_XNOR attr basic_expr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_REDUCE_XNOR, std::move($3));
		SET_AST_NODE_LOC($$.get(), @1, @3);
		append_attr($$.get(), std::move($2));
	} |
	basic_expr OP_SHL attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_SHIFT_LEFT, std::move($1), std::make_unique<AstNode>(@$, AST_TO_UNSIGNED, std::move($4)));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_SHR attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_SHIFT_RIGHT, std::move($1), std::make_unique<AstNode>(@$, AST_TO_UNSIGNED, std::move($4)));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_SSHL attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_SHIFT_SLEFT, std::move($1), std::make_unique<AstNode>(@$, AST_TO_UNSIGNED, std::move($4)));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_SSHR attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_SHIFT_SRIGHT, std::move($1), std::make_unique<AstNode>(@$, AST_TO_UNSIGNED, std::move($4)));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr TOK_INSIDE TOK_LCURL sva_inside_items TOK_RCURL {
		$$ = mk_sva(@$, "inside");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @5);
	} |
	basic_expr TOK_LT attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_LT, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_LE attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_LE, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_EQ attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_EQ, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_NE attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_NE, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_WEQ attr basic_expr {
		$$ = mk_sva(@$, "weq");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_WNE attr basic_expr {
		$$ = mk_sva(@$, "wne");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr TOK_MATCHES attr basic_expr {
		// IEEE 1800-2009 12.6: `subject matches pattern`
		$$ = mk_sva(@$, "meq");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_EQX attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_EQX, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_NEX attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_NEX, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_GE attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_GE, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr TOK_GT attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_GT, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr TOK_PLUS attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_ADD, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr TOK_MINUS attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_SUB, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr TOK_ASTER attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_MUL, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr TOK_SLASH attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_DIV, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr TOK_PERC attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_MOD, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_POW attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_POW, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	TOK_PLUS attr basic_expr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_POS, std::move($3));
		SET_AST_NODE_LOC($$.get(), @1, @3);
		append_attr($$.get(), std::move($2));
	} |
	TOK_MINUS attr basic_expr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_NEG, std::move($3));
		SET_AST_NODE_LOC($$.get(), @1, @3);
		append_attr($$.get(), std::move($2));
	} |
	basic_expr OP_LAND attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_LOGIC_AND, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	basic_expr OP_LOR attr basic_expr {
		$$ = std::make_unique<AstNode>(@$, AST_LOGIC_OR, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
		append_attr($$.get(), std::move($3));
	} |
	TOK_EXCL attr basic_expr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_LOGIC_NOT, std::move($3));
		SET_AST_NODE_LOC($$.get(), @1, @3);
		append_attr($$.get(), std::move($2));
	} |
	TOK_SIGNED OP_CAST TOK_LPAREN expr TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@2, "Static cast is only supported in SystemVerilog mode.");
		$$ = std::make_unique<AstNode>(@$, AST_TO_SIGNED, std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
	} |
	TOK_UNSIGNED OP_CAST TOK_LPAREN expr TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@2, "Static cast is only supported in SystemVerilog mode.");
		$$ = std::make_unique<AstNode>(@$, AST_TO_UNSIGNED, std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
	} |
	TOK_TYPE TOK_LPAREN type_operand TOK_RPAREN opt_type_cast {
		if (!mode->sv)
			err_at_loc(@1, "The type operator is only supported in SystemVerilog mode.");
		if ($5) {
			auto bits = std::make_unique<AstNode>(@$, AST_FCALL);
			bits->str = "\\$bits";
			bits->children.push_back($3->clone());
			$$ = std::make_unique<AstNode>(@$, AST_CAST_SIZE, std::move(bits), std::move($5));
		} else {
			// the type() operator yields a type, not a value (IEEE
			// 1800-2009 6.23); approximated as a constant so that
			// comparisons elaborate
			$$ = AstNode::mkconst_int(@$, 0, false);
		}
	} |
	basic_expr OP_CAST TOK_LPAREN expr TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@2, "Static cast is only supported in SystemVerilog mode.");
		$$ = std::make_unique<AstNode>(@$, AST_CAST_SIZE, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
	} |
	basic_expr OP_CAST TOK_LCURL assignment_pattern_list optional_comma TOK_RCURL {
		if (!mode->sv)
			err_at_loc(@2, "Static cast is only supported in SystemVerilog mode.");
		$$ = std::move($4);
	} |
	typedef_base_type OP_CAST TOK_LPAREN expr TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@2, "Static cast is only supported in SystemVerilog mode.");
		$$ = std::make_unique<AstNode>(@$, AST_CAST_SIZE, std::move($1), std::move($4));
		SET_AST_NODE_LOC($$.get(), @1, @4);
	} |
	typedef_base_type OP_CAST TOK_LCURL assignment_pattern_list optional_comma TOK_RCURL {
		if (!mode->sv)
			err_at_loc(@2, "Static cast is only supported in SystemVerilog mode.");
		$$ = std::move($4);
	} |
	typedef_base_type OP_CAST TOK_LPAREN expr TOK_RPAREN TOK_DOT TOK_ID TOK_LPAREN TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@2, "Static cast is only supported in SystemVerilog mode.");
		auto cast = std::make_unique<AstNode>(@$, AST_CAST_SIZE, std::move($1), std::move($4));
		std::string method = *$7;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(cast));
	} |
	typedef_base_type OP_CAST TOK_LPAREN expr TOK_RPAREN TOK_DOT TOK_ID TOK_LPAREN expr TOK_RPAREN {
		if (!mode->sv)
			err_at_loc(@2, "Static cast is only supported in SystemVerilog mode.");
		auto cast = std::make_unique<AstNode>(@$, AST_CAST_SIZE, std::move($1), std::move($4));
		std::string method = *$7;
		if (!method.empty() && method[0] == '\\')
			method = method.substr(1);
		$$ = std::make_unique<AstNode>(@$, AST_FCALL);
		$$->str = "." + method;
		$$->children.push_back(std::move(cast));
		$$->children.push_back(std::move($9));
	} |
	TOK_LPAREN expr TOK_EQ expr TOK_RPAREN {
		extra->ensureAsgnExprAllowed(@3, mode->sv);
		$$ = $2->clone();
		auto node = std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move($2), std::move($4));
		SET_AST_NODE_LOC(node.get(), @2, @4);
		extra->ast_stack.back()->children.push_back(std::move(node));
	} |
	TOK_LPAREN expr asgn_binop expr TOK_RPAREN {
		extra->ensureAsgnExprAllowed(@3, mode->sv);
		$$ = extra->addAsgnBinopStmt(nullptr, std::move($2), $3, std::move($4))-> clone();
	};

concat_list:
	expr {
		$$ = std::make_unique<AstNode>(@$, AST_CONCAT, std::move($1));
	} |
	expr with_slice {
		// dynamic-array slice in a streaming concatenation
		// `arr with [0 +: n]` (IEEE 11.4.14.4): the slice bounds are not
		// modeled; the whole array participates
		$$ = std::make_unique<AstNode>(@$, AST_CONCAT, std::move($1));
	} |
	expr TOK_COMMA concat_list {
		$$ = std::move($3);
		$$->children.push_back(std::move($1));
	} |
	expr with_slice TOK_COMMA concat_list {
		$$ = std::move($4);
		$$->children.push_back(std::move($1));
	};

with_slice:
	TOK_WITH_SLICE TOK_LBRA expr TOK_POS_INDEXED expr TOK_RBRA {
		(void)$3;
		(void)$5;
	} |
	TOK_WITH_SLICE TOK_LBRA expr TOK_COL expr TOK_RBRA {
		(void)$3;
		(void)$5;
	} |
	TOK_WITH_SLICE TOK_LBRA expr TOK_RBRA {
		(void)$3;
	};

assignment_pattern_list:
	%empty {
		$$ = std::make_unique<AstNode>(@$, AST_ASSIGN_PATTERN);
	} |
	expr {
		$$ = std::make_unique<AstNode>(@$, AST_ASSIGN_PATTERN);
		$$->children.push_back(std::move($1));
	} |
	expr TOK_LCURL concat_list TOK_RCURL {
		$$ = std::make_unique<AstNode>(@$, AST_ASSIGN_PATTERN);
		auto rep = std::make_unique<AstNode>(@$, AST_REPLICATE, std::move($1), std::move($3));
		// keep the replication visible to the unpacked-array pattern shape
		// check instead of const-folding it into a single value
		rep->set_attribute(ID(sv_pattern_rep), AstNode::mkconst_int(@$, 1, false));
		$$->children.push_back(std::move(rep));
	} |
	TOK_ID TOK_COL expr {
		$$ = std::make_unique<AstNode>(@$, AST_ASSIGN_PATTERN);
		$3->set_attribute(ID(sv_member), AstNode::mkconst_str(@1, *$1));
		$$->children.push_back(std::move($3));
	} |
	integral_number TOK_COL expr {
		$$ = std::make_unique<AstNode>(@$, AST_ASSIGN_PATTERN);
		$3->set_attribute(ID(sv_index), sva_num(@1, *$1, extra->case_type_stack));
		$$->children.push_back(std::move($3));
	} |
	integer_atom_type TOK_COL expr {
		// IEEE 1800-2009 10.9: a simple-type pattern key assigns every
		// member of that type (elaborated as a fill-all, last wins)
		$$ = std::make_unique<AstNode>(@$, AST_ASSIGN_PATTERN);
		$3->set_attribute(ID(sv_typekey), AstNode::mkconst_int(@1, $1, true));
		$$->children.push_back(std::move($3));
	} |
	TOK_DEFAULT TOK_COL expr {
		$$ = std::make_unique<AstNode>(@$, AST_ASSIGN_PATTERN);
		$3->set_attribute(ID(sv_member), AstNode::mkconst_str(@1, "default"));
		$$->children.push_back(std::move($3));
	} |
	assignment_pattern_list TOK_COMMA expr {
		$$ = std::move($1);
		$$->children.push_back(std::move($3));
	} |
	assignment_pattern_list TOK_COMMA TOK_ID TOK_COL expr {
		$$ = std::move($1);
		$5->set_attribute(ID(sv_member), AstNode::mkconst_str(@3, *$3));
		$$->children.push_back(std::move($5));
	} |
	assignment_pattern_list TOK_COMMA integral_number TOK_COL expr {
		$$ = std::move($1);
		$5->set_attribute(ID(sv_index), sva_num(@3, *$3, extra->case_type_stack));
		$$->children.push_back(std::move($5));
	} |
	assignment_pattern_list TOK_COMMA integer_atom_type TOK_COL expr {
		$$ = std::move($1);
		$5->set_attribute(ID(sv_typekey), AstNode::mkconst_int(@3, $3, true));
		$$->children.push_back(std::move($5));
	} |
	assignment_pattern_list TOK_COMMA TOK_DEFAULT TOK_COL expr {
		$$ = std::move($1);
		$5->set_attribute(ID(sv_member), AstNode::mkconst_str(@3, "default"));
		$$->children.push_back(std::move($5));
	};

member_pattern_list:
	%empty {
		$$ = std::make_unique<AstNode>(@$, AST_ASSIGN_PATTERN);
	} |
	expr {
		$$ = std::make_unique<AstNode>(@$, AST_ASSIGN_PATTERN);
		$$->children.push_back(std::move($1));
	} |
	TOK_DOT TOK_ID {
		// `.id` binds the member to a pattern variable; bindings are not modeled
		$$ = std::make_unique<AstNode>(@$, AST_ASSIGN_PATTERN);
		$$->children.push_back(AstNode::mkconst_int(@2, 1, false));
	} |
	member_pattern_list TOK_COMMA expr {
		$$ = std::move($1);
		$$->children.push_back(std::move($3));
	} |
	member_pattern_list TOK_COMMA TOK_DOT TOK_ID {
		$$ = std::move($1);
		$$->children.push_back(AstNode::mkconst_int(@3, 1, false));
	};

integral_number:
	TOK_CONSTVAL { $$ = std::move($1); } |
	TOK_UNBASED_UNSIZED_CONSTVAL { $$ = std::move($1); } |
	TOK_BASE TOK_BASED_CONSTVAL {
		$1->append(*$2);
		$$ = std::move($1);
	} |
	TOK_CONSTVAL TOK_BASE TOK_BASED_CONSTVAL {
		$1->append(*$2).append(*$3);
		$$ = std::move($1);
	};

// IEEE 1800-2009 clause 16 / Annex A.2.10. Layers follow Tables 16-1 and 16-3:
// tighter operators are inner nonterminals. Boolean operators stay tighter than ##.

sva_property_spec:
	sva_clock_opt sva_dis_opt sva_prop {
		auto clock = std::move($1);
		auto dis = std::move($2);
		auto body = std::move($3);
		if (!clock && extra->sva_default_clock)
			clock = extra->sva_default_clock->clone();
		if (!dis && extra->sva_default_disable && !extra->in_property_decl)
			dis = extra->sva_default_disable->clone();
		if (!clock && !dis) {
			$$ = std::move(body);
		} else {
			$$ = mk_sva(@$, "spec");
			if (clock)
				$$->children.push_back(std::move(clock));
			else
				$$->children.push_back(mk_sva(@$, "no_clock"));
			if (dis)
				$$->children.push_back(std::move(dis));
			else
				$$->children.push_back(mk_sva(@$, "no_disable"));
			$$->children.push_back(std::move(body));
		}
	};

sva_at_clock:
	TOK_AT TOK_LPAREN sva_event TOK_RPAREN {
		$$ = std::move($3);
		// `@($global_clock)` arrives as edge($global_clock)
		AstNode *gid = $$.get();
		if (gid && gid->type == AST_SVA && gid->str == "edge" && gid->children.size() == 1)
			gid = gid->children[0].get();
		if (extra->sva_global_clock && gid && gid->type == AST_IDENTIFIER && gid->str == "\\$global_clock" &&
				gid->children.empty())
			$$ = extra->sva_global_clock->clone();
	} |
	TOK_AT hierarchical_id {
		$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		$$->str = *$2;
	};

sva_clock_opt:
	sva_at_clock { $$ = std::move($1); } |
	%empty { $$ = nullptr; };

sva_dis_opt:
	TOK_DISABLE TOK_IFF TOK_LPAREN expr TOK_RPAREN {
		$$ = sva_add(mk_sva(@$, "disable_iff"), std::move($4));
	} |
	%empty { $$ = nullptr; };

sva_event_item:
	TOK_POSEDGE expr {
		$$ = sva_add(mk_sva(@$, "posedge"), std::move($2));
	} |
	TOK_POSEDGE expr TOK_IFF expr {
		$$ = sva_add(mk_sva(@$, "posedge"), std::move($2));
	} |
	TOK_NEGEDGE expr {
		$$ = sva_add(mk_sva(@$, "negedge"), std::move($2));
	} |
	TOK_NEGEDGE expr TOK_IFF expr {
		$$ = sva_add(mk_sva(@$, "negedge"), std::move($2));
	} |
	expr {
		$$ = sva_add(mk_sva(@$, "edge"), std::move($1));
	} |
	expr TOK_IFF expr {
		$$ = sva_add(mk_sva(@$, "edge"), std::move($1));
	};

sva_event:
	sva_event_item { $$ = std::move($1); } |
	sva_event TOK_OR sva_event_item {
		$$ = sva_bin(@$, "or_event", std::move($1), std::move($3));
	} |
	sva_event TOK_COMMA sva_event_item {
		$$ = sva_bin(@$, "or_event", std::move($1), std::move($3));
	};

sva_prop:
	sva_impl { $$ = std::move($1); } |
	TOK_IF TOK_LPAREN expr TOK_RPAREN sva_prop %prec FAKE_THEN {
		$$ = sva_add(mk_sva(@$, "if"), std::move($3));
		$$->children.push_back(std::move($5));
	} |
	TOK_IF TOK_LPAREN expr TOK_RPAREN sva_prop TOK_ELSE sva_prop {
		$$ = sva_add(mk_sva(@$, "ifelse"), std::move($3));
		$$->children.push_back(std::move($5));
		$$->children.push_back(std::move($7));
	} |
	TOK_STRONG TOK_LPAREN sva_prop TOK_RPAREN {
		$$ = sva_add(mk_sva(@$, "strong"), std::move($3));
	} |
	TOK_WEAK TOK_LPAREN sva_prop TOK_RPAREN {
		$$ = sva_add(mk_sva(@$, "weak"), std::move($3));
	} |
	TOK_CASE TOK_LPAREN expr TOK_RPAREN sva_case_items TOK_ENDCASE {
		$$ = mk_sva(@$, "case");
		$$->children.push_back(std::move($3));
		for (auto &child : $5->children)
			$$->children.push_back(std::move(child));
	} |
	TOK_ACCEPT_ON TOK_LPAREN expr TOK_RPAREN sva_prop {
		$$ = sva_bin(@$, "accept_on", std::move($3), std::move($5));
	} |
	TOK_REJECT_ON TOK_LPAREN expr TOK_RPAREN sva_prop {
		$$ = sva_bin(@$, "reject_on", std::move($3), std::move($5));
	} |
	TOK_SYNC_ACCEPT_ON TOK_LPAREN expr TOK_RPAREN sva_prop {
		$$ = sva_bin(@$, "sync_accept_on", std::move($3), std::move($5));
	} |
	TOK_SYNC_REJECT_ON TOK_LPAREN expr TOK_RPAREN sva_prop {
		$$ = sva_bin(@$, "sync_reject_on", std::move($3), std::move($5));
	};

sva_impl:
	sva_live { $$ = std::move($1); } |
	sva_live TOK_SVA_OV_IMPL sva_prop {
		$$ = sva_bin(@$, "|->", std::move($1), std::move($3));
	} |
	sva_live TOK_SVA_NONOV_IMPL sva_prop {
		$$ = sva_bin(@$, "|=>", std::move($1), std::move($3));
	} |
	sva_live TOK_SVA_OV_FOL sva_prop {
		$$ = sva_bin(@$, "#-#", std::move($1), std::move($3));
	} |
	sva_live TOK_SVA_NONOV_FOL sva_prop {
		$$ = sva_bin(@$, "#=#", std::move($1), std::move($3));
	};

sva_live:
	sva_until { $$ = std::move($1); } |
	sva_live_op { $$ = std::move($1); };

// the always / eventually prefix forms, also the operand of nexttime and
// not (IEEE 1800 16.13.10: `nexttime always a`, `s_nexttime s_eventually a`
// -- the operand is a property_expr; they used to be syntax errors)
sva_live_op:
	TOK_ALWAYS sva_live {
		$$ = sva_add(mk_sva(@$, "always"), std::move($2));
	} |
	TOK_ALWAYS TOK_LBRA sva_bounds TOK_RBRA sva_live {
		$$ = mk_sva(@$, "always");
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($5));
	} |
	TOK_S_ALWAYS TOK_LBRA sva_bounds TOK_RBRA sva_live {
		$$ = mk_sva(@$, "s_always");
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($5));
	} |
	TOK_EVENTUALLY sva_live {
		$$ = sva_add(mk_sva(@$, "eventually"), std::move($2));
	} |
	TOK_EVENTUALLY TOK_LBRA sva_bounds TOK_RBRA sva_live {
		$$ = mk_sva(@$, "eventually");
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($5));
	} |
	TOK_S_EVENTUALLY sva_live {
		$$ = sva_add(mk_sva(@$, "s_eventually"), std::move($2));
	} |
	TOK_S_EVENTUALLY TOK_LBRA sva_bounds TOK_RBRA sva_live {
		$$ = mk_sva(@$, "s_eventually");
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($5));
	};

sva_until:
	sva_iff { $$ = std::move($1); } |
	sva_iff TOK_UNTIL sva_until { $$ = sva_bin(@$, "until", std::move($1), std::move($3)); } |
	sva_iff TOK_S_UNTIL sva_until { $$ = sva_bin(@$, "s_until", std::move($1), std::move($3)); } |
	sva_iff TOK_UNTIL_WITH sva_until { $$ = sva_bin(@$, "until_with", std::move($1), std::move($3)); } |
	sva_iff TOK_S_UNTIL_WITH sva_until { $$ = sva_bin(@$, "s_until_with", std::move($1), std::move($3)); } |
	sva_iff TOK_IMPLIES sva_until { $$ = sva_bin(@$, "implies", std::move($1), std::move($3)); };

sva_iff:
	sva_or { $$ = std::move($1); } |
	sva_or TOK_IFF sva_iff { $$ = sva_bin(@$, "iff", std::move($1), std::move($3)); };

sva_or:
	sva_and { $$ = std::move($1); } |
	sva_or TOK_OR sva_and { $$ = sva_bin(@$, "or", std::move($1), std::move($3)); };

sva_and:
	sva_not { $$ = std::move($1); } |
	sva_and TOK_PRIMITIVE sva_not {
		if (*$2 != "and")
			err_at_loc(@2, "SVA requires operator 'and', got '%s'.", $2->c_str());
		$$ = sva_bin(@$, "and", std::move($1), std::move($3));
	};

sva_not:
	TOK_PRIMITIVE sva_not {
		if (*$1 != "not")
			err_at_loc(@1, "SVA requires operator 'not', got '%s'.", $1->c_str());
		$$ = sva_add(mk_sva(@$, "not"), std::move($2));
	} |
	TOK_PRIMITIVE sva_live_op {
		if (*$1 != "not")
			err_at_loc(@1, "SVA requires operator 'not', got '%s'.", $1->c_str());
		$$ = sva_add(mk_sva(@$, "not"), std::move($2));
	} |
	TOK_NEXTTIME sva_live_op { $$ = sva_add(mk_sva(@$, "nexttime"), std::move($2)); } |
	TOK_NEXTTIME TOK_LBRA expr TOK_RBRA sva_live_op {
		$$ = mk_sva(@$, "nexttime");
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($5));
	} |
	TOK_S_NEXTTIME sva_live_op { $$ = sva_add(mk_sva(@$, "s_nexttime"), std::move($2)); } |
	TOK_S_NEXTTIME TOK_LBRA expr TOK_RBRA sva_live_op {
		$$ = mk_sva(@$, "s_nexttime");
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($5));
	} |
	TOK_NEXTTIME sva_not { $$ = sva_add(mk_sva(@$, "nexttime"), std::move($2)); } |
	TOK_NEXTTIME TOK_LBRA expr TOK_RBRA sva_not {
		$$ = mk_sva(@$, "nexttime");
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($5));
	} |
	TOK_S_NEXTTIME sva_not { $$ = sva_add(mk_sva(@$, "s_nexttime"), std::move($2)); } |
	TOK_S_NEXTTIME TOK_LBRA expr TOK_RBRA sva_not {
		$$ = mk_sva(@$, "s_nexttime");
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($5));
	} |
	sva_intersect { $$ = std::move($1); };

sva_intersect:
	sva_within { $$ = std::move($1); } |
	sva_intersect TOK_INTERSECT sva_within { $$ = sva_bin(@$, "intersect", std::move($1), std::move($3)); };

sva_within:
	sva_throughout { $$ = std::move($1); } |
	sva_within TOK_WITHIN sva_throughout { $$ = sva_bin(@$, "within", std::move($1), std::move($3)); };

sva_throughout:
	sva_concat { $$ = std::move($1); } |
	sva_concat TOK_THROUGHOUT sva_throughout { $$ = sva_bin(@$, "throughout", std::move($1), std::move($3)); };

sva_concat:
	sva_repeat { $$ = std::move($1); } |
	sva_at_clock sva_repeat {
		$$ = sva_bin(@$, "clocked", std::move($1), std::move($2));
	} |
	sva_at_clock TOK_SVA_HASH2 sva_cycle sva_repeat {
		// multiclock sequence `@(posedge a) ##1 x` (IEEE 16.5): the
		// resampling clock is kept as the clock operand
		auto delay = mk_sva(@$, "##");
		delay->children.push_back(std::move($3));
		delay->children.push_back(std::move($4));
		$$ = sva_bin(@$, "clocked", std::move($1), std::move(delay));
	} |
	sva_concat TOK_SVA_HASH2 sva_cycle sva_repeat {
		$$ = mk_sva(@$, "##");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($4));
	} |
	sva_concat TOK_SVA_HASH2 sva_cycle sva_at_clock sva_repeat {
		// multiclock tail `##1 @(posedge b) x`: the resampled element
		// keeps its clock
		$$ = mk_sva(@$, "##");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($3));
		$$->children.push_back(sva_bin(@$, "clocked", std::move($4), std::move($5)));
	} |
	sva_concat TOK_SVA_HASH2 sva_cycle sva_delay_tail {
		$$ = mk_sva(@$, "##");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($4));
	} |
	TOK_SVA_HASH2 sva_cycle sva_repeat {
		$$ = mk_sva(@$, "##");
		$$->children.push_back(mk_sva(@1, "true"));
		$$->children.push_back(std::move($2));
		$$->children.push_back(std::move($3));
	} |
	TOK_SVA_HASH2 sva_cycle sva_delay_tail {
		$$ = mk_sva(@$, "##");
		$$->children.push_back(mk_sva(@1, "true"));
		$$->children.push_back(std::move($2));
		$$->children.push_back(std::move($3));
	};

sva_delay_tail:
	TOK_SVA_HASH2 sva_cycle sva_repeat {
		$$ = mk_sva(@$, "##");
		$$->children.push_back(mk_sva(@1, "true"));
		$$->children.push_back(std::move($2));
		$$->children.push_back(std::move($3));
	} |
	TOK_SVA_HASH2 sva_cycle sva_delay_tail {
		$$ = mk_sva(@$, "##");
		$$->children.push_back(mk_sva(@1, "true"));
		$$->children.push_back(std::move($2));
		$$->children.push_back(std::move($3));
	};

sva_repeat:
	sva_bool { $$ = std::move($1); } |
	sva_bool TOK_SVA_REP_CONSEC TOK_RBRA {
		$$ = sva_add(mk_sva(@$, "[*]"), std::move($1));
	} |
	sva_bool TOK_SVA_REP_PLUS TOK_RBRA {
		$$ = sva_add(mk_sva(@$, "[+]"), std::move($1));
	} |
	sva_bool TOK_SVA_REP_CONSEC sva_bounds TOK_RBRA {
		$$ = mk_sva(@$, "[*]");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($3));
	} |
	sva_bool TOK_SVA_REP_GOTO sva_bounds TOK_RBRA {
		$$ = mk_sva(@$, "[->]");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($3));
	} |
	sva_bool TOK_SVA_REP_NONCONS sva_bounds TOK_RBRA {
		$$ = mk_sva(@$, "[=]");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($3));
	};

sva_bool:
	sva_bexpr { $$ = std::move($1); } |
	sva_bexpr TOK_QUE sva_bool TOK_COL sva_bool {
		$$ = std::make_unique<AstNode>(@$, AST_TERNARY, std::move($1), std::move($3), std::move($5));
	};

sva_bexpr:
	sva_primary { $$ = std::move($1); } |
	sva_bexpr OP_LOR sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_LOGIC_OR, std::move($1), std::move($3)); } |
	sva_bexpr OP_LAND sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_LOGIC_AND, std::move($1), std::move($3)); } |
	sva_bexpr TOK_PIPE sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_BIT_OR, std::move($1), std::move($3)); } |
	sva_bexpr TOK_CARET sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_BIT_XOR, std::move($1), std::move($3)); } |
	sva_bexpr OP_XNOR sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_BIT_XNOR, std::move($1), std::move($3)); } |
	sva_bexpr TOK_AMP sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_BIT_AND, std::move($1), std::move($3)); } |
	sva_bexpr OP_EQ sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_EQ, std::move($1), std::move($3)); } |
	sva_bexpr OP_NE sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_NE, std::move($1), std::move($3)); } |
	sva_bexpr OP_WEQ sva_bexpr {
		$$ = mk_sva(@$, "weq");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($3));
	} |
	sva_bexpr OP_WNE sva_bexpr {
		$$ = mk_sva(@$, "wne");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($3));
	} |
	sva_bexpr OP_EQX sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_EQX, std::move($1), std::move($3)); } |
	sva_bexpr OP_NEX sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_NEX, std::move($1), std::move($3)); } |
	sva_bexpr TOK_LT sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_LT, std::move($1), std::move($3)); } |
	sva_bexpr OP_LE sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_LE, std::move($1), std::move($3)); } |
	sva_bexpr OP_GE sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_GE, std::move($1), std::move($3)); } |
	sva_bexpr TOK_GT sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_GT, std::move($1), std::move($3)); } |
	sva_bexpr OP_SHL sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_SHIFT_LEFT, std::move($1), std::move($3)); } |
	sva_bexpr OP_SHR sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_SHIFT_RIGHT, std::move($1), std::move($3)); } |
	sva_bexpr OP_SSHL sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_SHIFT_SLEFT, std::move($1), std::move($3)); } |
	sva_bexpr OP_SSHR sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_SHIFT_SRIGHT, std::move($1), std::move($3)); } |
	sva_bexpr TOK_PLUS sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_ADD, std::move($1), std::move($3)); } |
	sva_bexpr TOK_MINUS sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_SUB, std::move($1), std::move($3)); } |
	sva_bexpr TOK_ASTER sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_MUL, std::move($1), std::move($3)); } |
	sva_bexpr TOK_SLASH sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_DIV, std::move($1), std::move($3)); } |
	sva_bexpr TOK_PERC sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_MOD, std::move($1), std::move($3)); } |
	sva_bexpr OP_POW sva_bexpr { $$ = std::make_unique<AstNode>(@$, AST_POW, std::move($1), std::move($3)); } |
	sva_primary OP_CAST TOK_LPAREN sva_bexpr TOK_RPAREN {
		$$ = std::make_unique<AstNode>(@$, AST_CAST_SIZE, std::move($1), std::move($4));
	} |
	integer_atom_type OP_CAST TOK_LPAREN sva_bexpr TOK_RPAREN {
		$$ = std::make_unique<AstNode>(@$, AST_CAST_SIZE, AstNode::mkconst_int(@1, $1, false), std::move($4));
	} |
	TOK_SIGNED OP_CAST TOK_LPAREN sva_bexpr TOK_RPAREN {
		$$ = std::make_unique<AstNode>(@$, AST_TO_SIGNED, std::move($4));
	} |
	TOK_UNSIGNED OP_CAST TOK_LPAREN sva_bexpr TOK_RPAREN {
		$$ = std::make_unique<AstNode>(@$, AST_TO_UNSIGNED, std::move($4));
	} |
	TOK_USER_TYPE OP_CAST TOK_LPAREN sva_bexpr TOK_RPAREN {
		auto cast_ty = std::make_unique<AstNode>(@1, AST_WIRE);
		cast_ty->is_logic = true;
		extra->addWiretypeNode($1.get(), cast_ty.get());
		$$ = std::make_unique<AstNode>(@$, AST_CAST_SIZE, std::move(cast_ty), std::move($4));
	} |
	sva_bexpr TOK_ARROW sva_bexpr {
		$$ = std::make_unique<AstNode>(@$, AST_LOGIC_OR, std::make_unique<AstNode>(@$, AST_LOGIC_NOT, std::move($1)), std::move($3));
	} |
	sva_bexpr TOK_INSIDE TOK_LCURL sva_inside_items TOK_RCURL {
		$$ = mk_sva(@$, "inside");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($4));
	} |
	TOK_EXCL sva_bexpr %prec UNARY_OPS { $$ = std::make_unique<AstNode>(@$, AST_LOGIC_NOT, std::move($2)); } |
	TOK_TILDE sva_bexpr %prec UNARY_OPS { $$ = std::make_unique<AstNode>(@$, AST_BIT_NOT, std::move($2)); } |
	TOK_PLUS sva_bexpr %prec UNARY_OPS { $$ = std::make_unique<AstNode>(@$, AST_POS, std::move($2)); } |
	TOK_MINUS sva_bexpr %prec UNARY_OPS { $$ = std::make_unique<AstNode>(@$, AST_NEG, std::move($2)); } |
	TOK_AMP sva_bexpr %prec UNARY_OPS { $$ = std::make_unique<AstNode>(@$, AST_REDUCE_AND, std::move($2)); } |
	TOK_PIPE sva_bexpr %prec UNARY_OPS { $$ = std::make_unique<AstNode>(@$, AST_REDUCE_OR, std::move($2)); } |
	TOK_CARET sva_bexpr %prec UNARY_OPS { $$ = std::make_unique<AstNode>(@$, AST_REDUCE_XOR, std::move($2)); } |
	OP_NAND sva_bexpr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_LOGIC_NOT, std::make_unique<AstNode>(@$, AST_REDUCE_AND, std::move($2)));
	} |
	OP_NOR sva_bexpr %prec UNARY_OPS {
		$$ = std::make_unique<AstNode>(@$, AST_LOGIC_NOT, std::make_unique<AstNode>(@$, AST_REDUCE_OR, std::move($2)));
	} |
	OP_XNOR sva_bexpr %prec UNARY_OPS { $$ = std::make_unique<AstNode>(@$, AST_REDUCE_XNOR, std::move($2)); };

sva_primary:
	rvalue { $$ = std::move($1); } |
	integral_number { $$ = sva_num(@1, *$1, extra->case_type_stack); } |
	TOK_REALVAL {
		$$ = std::make_unique<AstNode>(@$, AST_REALVALUE);
		$$->realvalue = strtod($1->c_str(), nullptr);
	} |
	TOK_STRING { $$ = AstNode::mkconst_str(@1, *$1); } |
	TOK_LCURL concat_list TOK_RCURL { $$ = std::move($2); } |
	TOK_LCURL expr TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::make_unique<AstNode>(@$, AST_REPLICATE, std::move($2), std::move($4));
	} |
	TOK_LCURL OP_SHL expr TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = mk_sva(@$, "stream_l");
		$$->children.push_back(std::move($3));
		$$->children.push_back(std::move($5));
	} |
	TOK_LCURL OP_SHL TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = mk_sva(@$, "stream_l");
		$$->children.push_back(AstNode::mkconst_int(@$, 1, false));
		$$->children.push_back(std::move($4));
	} |
	TOK_LCURL OP_SHL integer_atom_type TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = mk_sva(@$, "stream_l");
		$$->children.push_back(AstNode::mkconst_int(@$, $3, false));
		$$->children.push_back(std::move($5));
	} |
	TOK_LCURL OP_SHL TOK_LOGIC TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = mk_sva(@$, "stream_l");
		$$->children.push_back(AstNode::mkconst_int(@$, 1, false));
		$$->children.push_back(std::move($5));
	} |
	TOK_LCURL OP_SHR expr TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHR integer_atom_type TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHR TOK_LOGIC TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($5);
	} |
	TOK_LCURL OP_SHR TOK_LCURL concat_list TOK_RCURL TOK_RCURL {
		$$ = std::move($4);
	} |
	TOK_FIRST_MATCH TOK_LPAREN sva_prop sva_matches TOK_RPAREN {
		$$ = mk_sva(@$, "first_match");
		$$->children.push_back(std::move($3));
		if (!$4->children.empty())
			$$->children.push_back(std::move($4));
	} |
	TOK_LPAREN sva_at_clock sva_bexpr TOK_RPAREN {
		$$ = sva_bin(@$, "clocked", std::move($2), std::move($3));
	} |
	TOK_LPAREN sva_prop sva_matches TOK_RPAREN {
		if ($3->children.empty())
			$$ = std::move($2);
		else {
			$$ = mk_sva(@$, "seq_match");
			$$->children.push_back(std::move($2));
			$$->children.push_back(std::move($3));
		}
	} |
	hierarchical_id <ast_t>{
		auto fcall = std::make_unique<AstNode>(@1, AST_FCALL);
		fcall->str = *$1;
		extra->ast_stack.push_back(fcall.get());
		$$ = std::move(fcall);
	} TOK_LPAREN arg_list optional_comma TOK_RPAREN sva_seq_method {
		extra->ast_stack.pop_back();
		if (!$7) {
			$$ = std::move($2);
		} else {
			// `seq(args).triggered` / `.matched` (IEEE 1800-2009 16.9.11):
			// "rule2 is equivalent to rule2a" -- the instance becomes the
			// body of a hidden argument-free declaration and the method
			// applies to that, the form the rest of the flow handles
			if (*$7 != "\\triggered" && *$7 != "\\matched")
				err_at_loc(@7, "Unsupported method `%s' on sequence instance `%s'.", $7->c_str() + 1, $1->c_str() + 1);
			AstNode *scope = nullptr;
			for (auto it = extra->ast_stack.rbegin(); it != extra->ast_stack.rend() && !scope; ++it)
				if ((*it)->type == AST_MODULE || (*it)->type == AST_GENBLOCK)
					scope = *it;
			if (!scope)
				err_at_loc(@$, "Sequence method on an instance with arguments outside a module.");
			static int seq_method_ctr = 0;
			std::string hidden = stringf("%s$method$%d", $1->c_str(), ++seq_method_ctr);
			auto id = std::make_unique<AstNode>(@1, AST_CELLTYPE);
			id->str = hidden;
			auto decl = mk_sva(@$, "sequence_decl");
			decl->children.push_back(std::move(id));
			decl->children.push_back(std::move($2));
			scope->children.push_back(std::move(decl));
			$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
			$$->str = hidden + "." + $7->substr(1);
		}
	};

sva_seq_method:
	%empty { $$ = nullptr; } |
	TOK_DOT TOK_ID { $$ = std::move($2); };

sva_matches:
	%empty { $$ = mk_sva(@$, "matches"); } |
	sva_matches TOK_COMMA hierarchical_id TOK_EQ expr {
		$$ = std::move($1);
		auto asgn = mk_sva(@$, "assign");
		auto id = std::make_unique<AstNode>(@3, AST_IDENTIFIER);
		id->str = *$3;
		asgn->children.push_back(std::move(id));
		asgn->children.push_back(std::move($5));
		$$->children.push_back(std::move(asgn));
	} |
	sva_matches TOK_COMMA hierarchical_id TOK_LPAREN TOK_RPAREN {
		$$ = std::move($1);
	} |
	sva_matches TOK_COMMA hierarchical_id TOK_LPAREN expr TOK_RPAREN {
		$$ = std::move($1);
	} |
	sva_matches TOK_COMMA hierarchical_id TOK_LBRA expr TOK_RBRA TOK_EQ expr {
		$$ = std::move($1);
		auto asgn = mk_sva(@$, "assign");
		auto id = std::make_unique<AstNode>(@3, AST_IDENTIFIER);
		id->str = *$3;
		asgn->children.push_back(std::move(id));
		asgn->children.push_back(std::move($8));
		$$->children.push_back(std::move(asgn));
	} |
	sva_matches TOK_COMMA hierarchical_id TOK_LBRA expr TOK_COL expr TOK_RBRA TOK_EQ expr {
		$$ = std::move($1);
		auto asgn = mk_sva(@$, "assign");
		auto id = std::make_unique<AstNode>(@3, AST_IDENTIFIER);
		id->str = *$3;
		asgn->children.push_back(std::move(id));
		asgn->children.push_back(std::move($10));
		$$->children.push_back(std::move(asgn));
	};

sva_cycle:
	integral_number { $$ = sva_num(@1, *$1, extra->case_type_stack); } |
	TOK_ID {
		$$ = std::make_unique<AstNode>(@$, AST_IDENTIFIER);
		$$->str = *$1;
	} |
	TOK_LPAREN expr TOK_RPAREN { $$ = std::move($2); } |
	TOK_LBRA sva_bounds TOK_RBRA { $$ = std::move($2); } |
	TOK_SVA_REP_CONSEC TOK_RBRA { $$ = mk_sva(@$, "##[*]"); } |
	TOK_SVA_REP_PLUS TOK_RBRA { $$ = mk_sva(@$, "##[+]"); };

sva_bounds:
	expr sva_bounds_tail {
		$$ = std::move($2);
		$$->children.insert($$->children.begin(), std::move($1));
	};

sva_bounds_tail:
	%empty {
		$$ = mk_sva(@$, "bounds");
	} |
	TOK_COL expr {
		// `$` (LRM 16.7/16.9.2: unbounded upper bound of a cycle delay or
		// repetition range) lexes as an identifier; it used to become an
		// implicit wire named `$` and fail sva2check as a non-constant
		// bound.  sva2check reads an upper bound of -1 as unbounded.
		if ($2->type == AST_IDENTIFIER && $2->str == "\\$" && $2->children.empty())
			$2 = AstNode::mkconst_int($2->location, 0xffffffffu, true);
		$$ = sva_add(mk_sva(@$, "bounds"), std::move($2));
	};

sva_case_items:
	expr TOK_COL sva_prop TOK_SEMICOL {
		$$ = mk_sva(@$, "case_items");
		$$->children.push_back(std::move($1));
		$$->children.push_back(std::move($3));
	} |
	TOK_DEFAULT TOK_COL sva_prop TOK_SEMICOL {
		$$ = sva_add(mk_sva(@$, "case_items"), std::move($3));
	} |
	TOK_DEFAULT sva_prop TOK_SEMICOL {
		$$ = sva_add(mk_sva(@$, "case_items"), std::move($2));
	} |
	sva_case_items expr TOK_COL sva_prop TOK_SEMICOL {
		$$ = std::move($1);
		$$->children.push_back(std::move($2));
		$$->children.push_back(std::move($4));
	} |
	sva_case_items TOK_DEFAULT TOK_COL sva_prop TOK_SEMICOL {
		$$ = std::move($1);
		$$->children.push_back(std::move($4));
	};

sva_msg_args:
	expr {
		$$ = sva_add(mk_sva(@$, "action"), std::move($1));
	} |
	sva_msg_args TOK_COMMA expr {
		$$ = std::move($1);
		$$->children.push_back(std::move($3));
	};

sva_msg:
	TOK_MSG_TASKS TOK_LPAREN sva_msg_args TOK_RPAREN TOK_SEMICOL {
		$$ = std::move($3);
		$$->str = *$1;
	} |
	TOK_MSG_TASKS TOK_SEMICOL {
		$$ = mk_sva(@$, $1->c_str());
	} |
	hierarchical_id TOK_LPAREN sva_msg_args TOK_RPAREN TOK_SEMICOL {
		$$ = std::move($3);
		$$->str = *$1;
	} |
	hierarchical_id TOK_LPAREN sva_msg_args TOK_RPAREN {
		$$ = std::move($3);
		$$->str = *$1;
	};

sva_action:
	TOK_SEMICOL { $$ = nullptr; } |
	TOK_ELSE TOK_SEMICOL { $$ = nullptr; } |
	TOK_ELSE hierarchical_id TOK_EQ expr TOK_SEMICOL {
		// IEEE 1800-2009 16.2: the else statement runs when the immediate assertion fails.
		auto lhs = std::make_unique<AstNode>(@2, AST_IDENTIFIER);
		lhs->str = *$2;
		auto asg = std::make_unique<AstNode>(@$, AST_ASSIGN_EQ, std::move(lhs), std::move($4));
		SET_AST_NODE_LOC(asg.get(), @2, @4);
		if (extra->pending_assert_cond) {
			auto cas = std::make_unique<AstNode>(@$, AST_CASE);
			cas->children.push_back(std::make_unique<AstNode>(@$, AST_REDUCE_BOOL,
					std::make_unique<AstNode>(@$, AST_LOGIC_NOT, extra->pending_assert_cond->clone())));
			auto blk = std::make_unique<AstNode>(@$, AST_BLOCK);
			blk->children.push_back(std::move(asg));
			cas->children.push_back(std::make_unique<AstNode>(@$, AST_COND,
					AstNode::mkconst_int(@$, 1, false, 1), std::move(blk)));
			extra->saveChild(std::move(cas));
		} else {
			extra->saveChild(std::move(asg));
		}
		$$ = nullptr;
	} |
	TOK_ELSE sva_msg { $$ = std::move($2); } |
	TOK_ELSE behavioral_stmt {
		// behavioral_stmt (incl. begin/end) registers itself as the last
		// child of the enclosing context: move it under the assertion.
		$$ = sva_action_detach_last(extra);
	} |
	behavioral_stmt TOK_ELSE behavioral_stmt {
		// if-action / else-action pair: keep the else action
		$$ = sva_action_detach_last(extra);
		(void)sva_action_detach_last(extra);
	} |
	behavioral_stmt {
		$$ = sva_action_detach_last(extra);
	} |
	sva_msg { $$ = std::move($1); };

sva_port_list:
	TOK_ID { sva_formal_wire(extra, @1, *$1); } |
	TOK_UNTYPED TOK_ID { sva_formal_wire(extra, @2, *$2); } |
	sva_port_list TOK_COMMA TOK_ID { sva_formal_wire(extra, @3, *$3); } |
	sva_port_list TOK_COMMA TOK_UNTYPED TOK_ID { sva_formal_wire(extra, @4, *$4); };

sva_ports:
	%empty |
	TOK_LPAREN sva_port_list optional_comma TOK_RPAREN;

sva_sequence_decl:
	TOK_SEQUENCE TOK_ID {
		extra->let_port_names.clear();
		extra->collecting_let_ports = true;
		extra->sva_local_renames.clear();
	} sva_ports {
		extra->collecting_let_ports = false;
	} TOK_SEMICOL sva_prop_locals sva_or TOK_SEMICOL TOK_ENDSEQUENCE opt_label {
		checkLabelsMatch(@10, "Sequence label", $2.get(), $11.get());
		auto id = std::make_unique<AstNode>(@2, AST_CELLTYPE);
		id->str = *$2;
		auto node = mk_sva(@$, "sequence_decl");
		node->children.push_back(std::move(id));
		sva_rename_locals(extra, $8.get());
		extra->sva_local_renames.clear();
		node->children.push_back(std::move($8));
		for (auto &port : extra->let_port_names) {
			auto formal = std::make_unique<AstNode>(@2, AST_IDENTIFIER);
			formal->str = port;
			node->children.push_back(std::move(formal));
		}
		extra->let_port_names.clear();
		extra->saveChild(std::move(node));
	};

sva_local_names:
	TOK_ID { sva_local_wire(extra, @1, *$1); } |
	sva_local_names TOK_COMMA TOK_ID { sva_local_wire(extra, @3, *$3); };

fork_join:
	TOK_JOIN opt_label |
	TOK_JOIN_ANY opt_label |
	TOK_JOIN_NONE opt_label;

fork_stmts:
	behavioral_stmt |
	fork_stmts behavioral_stmt;

sva_prop_local:
	TOK_USER_TYPE {
		extra->sva_local_kind = 2;
		extra->sva_local_tname = *$1;
	} sva_local_names TOK_SEMICOL |
	TOK_PKG_USER_TYPE {
		extra->sva_local_kind = 2;
		extra->sva_local_tname = *$1;
	} sva_local_names TOK_SEMICOL |
	TOK_INT { extra->sva_local_kind = 1; } sva_local_names TOK_SEMICOL |
	TOK_INTEGER { extra->sva_local_kind = 1; } sva_local_names TOK_SEMICOL |
	TOK_LOGIC range_or_multirange {
		extra->sva_local_kind = 0;
		extra->sva_local_range = std::move($2);
	} sva_local_names TOK_SEMICOL { extra->sva_local_range.reset(); };

sva_prop_locals:
	%empty |
	sva_prop_locals sva_prop_local;

sva_property_decl:
	TOK_PROPERTY TOK_ID {
		extra->let_port_names.clear();
		extra->collecting_let_ports = true;
		extra->sva_local_renames.clear();
	} sva_ports {
		extra->collecting_let_ports = false;
		extra->in_property_decl = true;
	} TOK_SEMICOL sva_prop_locals sva_property_spec TOK_SEMICOL TOK_ENDPROPERTY opt_label {
		extra->in_property_decl = false;
		checkLabelsMatch(@10, "Property label", $2.get(), $11.get());
		auto id = std::make_unique<AstNode>(@2, AST_CELLTYPE);
		id->str = *$2;
		auto node = mk_sva(@$, "property_decl");
		node->children.push_back(std::move(id));
		sva_rename_locals(extra, $8.get());
		extra->sva_local_renames.clear();
		node->children.push_back(std::move($8));
		for (auto &port : extra->let_port_names) {
			auto formal = std::make_unique<AstNode>(@2, AST_IDENTIFIER);
			formal->str = port;
			node->children.push_back(std::move(formal));
		}
		extra->let_port_names.clear();
		extra->saveChild(std::move(node));
	};

sva_let_decl:
	TOK_LET TOK_ID {
		extra->let_port_names.clear();
		extra->collecting_let_ports = true;
	} sva_ports {
		extra->collecting_let_ports = false;
	} TOK_EQ sva_prop TOK_SEMICOL {
		auto id = std::make_unique<AstNode>(@2, AST_CELLTYPE);
		id->str = *$2;
		auto node = mk_sva(@$, "let_decl");
		node->children.push_back(std::move(id));
		node->children.push_back(std::move($7));
		for (auto &port : extra->let_port_names) {
			auto formal = std::make_unique<AstNode>(@2, AST_IDENTIFIER);
			formal->str = port;
			node->children.push_back(std::move(formal));
		}
		extra->let_port_names.clear();
		extra->saveChild(std::move(node));
	};

clocking_decl:
	TOK_GLOBAL TOK_CLOCKING TOK_ID sva_at_clock TOK_SEMICOL clocking_items TOK_ENDCLOCKING opt_label {
		// IEEE 14.14: `global clocking` names the global clock event;
		// `@($global_clock)` in a concurrent assertion stands for it (it
		// used to become an undriven implicit `$global_clock` net)
		extra->sva_global_clock = $4->clone();
		checkLabelsMatch(@7, "Clocking block label", $3.get(), $8.get());
	} |
	TOK_CLOCKING TOK_ID sva_at_clock TOK_SEMICOL clocking_items TOK_ENDCLOCKING opt_label {
		checkLabelsMatch(@6, "Clocking block label", $2.get(), $7.get());
	} |
	TOK_DEFAULT TOK_CLOCKING TOK_ID sva_at_clock TOK_SEMICOL clocking_items TOK_ENDCLOCKING opt_label {
		extra->sva_default_clock = $4->clone();
		checkLabelsMatch(@7, "Clocking block label", $3.get(), $8.get());
	} |
	TOK_DEFAULT TOK_CLOCKING sva_at_clock TOK_SEMICOL clocking_items TOK_ENDCLOCKING opt_label {
		extra->sva_default_clock = $3->clone();
	};

clocking_items:
	%empty |
	clocking_items clocking_item;

clocking_item:
	TOK_DEFAULT clocking_dir clocking_skew clocking_dir clocking_skew TOK_SEMICOL |
	TOK_DEFAULT clocking_dir clocking_skew TOK_SEMICOL |
	clocking_dir clocking_skew clocking_assign_list TOK_SEMICOL |
	TOK_SEMICOL;

clocking_assign_list:
	clocking_assign |
	clocking_assign_list TOK_COMMA clocking_assign;

clocking_assign:
	TOK_ID |
	TOK_ID TOK_EQ expr;

clocking_dir:
	TOK_INPUT | TOK_OUTPUT | TOK_INOUT;

clocking_skew:
	TOK_POSEDGE clocking_delay_opt |
	TOK_NEGEDGE clocking_delay_opt |
	clocking_delay_opt;

clocking_delay_opt:
	TOK_HASH TOK_CONSTVAL |
	TOK_HASH TOK_REALVAL |
	TOK_HASH TOK_TIME_SCALE |
	%empty;

sva_inside_items:
	expr {
		$$ = sva_add(mk_sva(@$, "set"), std::move($1));
	} |
	TOK_LBRA expr TOK_COL expr TOK_RBRA {
		auto range = std::make_unique<AstNode>(@$, AST_RANGE, std::move($2), std::move($4));
		$$ = sva_add(mk_sva(@$, "set"), std::move(range));
	} |
	sva_inside_items TOK_COMMA expr {
		$$ = std::move($1);
		$$->children.push_back(std::move($3));
	} |
	sva_inside_items TOK_COMMA TOK_LBRA expr TOK_COL expr TOK_RBRA {
		$$ = std::move($1);
		$$->children.push_back(std::make_unique<AstNode>(@$, AST_RANGE, std::move($4), std::move($6)));
	};


sva_default_disable:
	TOK_DEFAULT TOK_DISABLE TOK_IFF expr TOK_SEMICOL {
		extra->sva_default_disable = sva_add(mk_sva(@$, "disable_iff"), std::move($4));
	};
