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

#include "kernel/yosys.h"
#include "kernel/celltypes.h"
#include "kernel/utils.h"
#include "kernel/sigtools.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string_view>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

template <typename... Args>
[[nodiscard]] std::string concat_views(const Args&... views) {
    static_assert((std::is_convertible_v<Args, std::string_view> && ...),
                  "All arguments must be convertible to std::string_view.");
    const std::size_t total_size = (std::string_view(views).size() + ... + 0);

    std::string result;
    result.reserve(total_size);

    (result.append(views), ...);
    return result;
}

IdString concat_name(RTLIL::Cell *cell, IdString const &object_name, const std::string &separator = ".")
{
	std::string_view object_name_view(object_name.c_str());
	if (object_name_view[0] == '\\'){
		return concat_views(cell->name.c_str(), separator, object_name_view.substr(1));
	}

	constexpr std::string_view prefix = "$flatten";
	if (object_name_view.substr(0, prefix.size()) == prefix){
		object_name_view.remove_prefix(prefix.size());
	}
	return concat_views(prefix, cell->name.c_str(), separator, object_name_view);
}

// Register v3 B5 marks (sv_degraded reasons, sv_permissive) live on the
// module where the leniency fired; ic3 refuses a design in which any module
// carries one. Inlined, the template is deleted with its marks, so a free
// wire in a submodule left a flattened design looking clean: the parent
// takes the template's reasons (each keeps its @file:line; repeats from
// further instances are not added again)
static void inherit_degradation(RTLIL::Module *module, RTLIL::Module *tpl)
{
	IdString attr = IdString("\\sv_degraded");
	if (tpl->attributes.count(attr)) {
		std::string cur = module->attributes.count(attr) ? module->attributes.at(attr).decode_string() : std::string();
		pool<std::string> have;
		for (size_t pos = 0; pos <= cur.size(); ) {
			size_t next = cur.find("; ", pos);
			have.insert(cur.substr(pos, next == std::string::npos ? std::string::npos : next - pos));
			if (next == std::string::npos)
				break;
			pos = next + 2;
		}
		std::string add = tpl->attributes.at(attr).decode_string();
		for (size_t pos = 0; pos <= add.size(); ) {
			size_t next = add.find("; ", pos);
			std::string entry = add.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
			// an upward-reference port's pending mark is per template:
			// connect_upward_refs has connected this instance's port (or
			// recorded its own reason) before it is inlined
			bool pending_up = entry.rfind("Upward reference `", 0) == 0 &&
					entry.find("in the instantiating module) is connected only by flatten.") != std::string::npos;
			if (!entry.empty() && !pending_up && !have.count(entry)) {
				cur += (cur.empty() ? "" : "; ") + entry;
				have.insert(entry);
			}
			if (next == std::string::npos)
				break;
			pos = next + 2;
		}
		if (!cur.empty())
			module->attributes[attr] = RTLIL::Const(cur);
	}
	if (tpl->get_bool_attribute(IdString("\\sv_permissive")))
		module->set_bool_attribute(IdString("\\sv_permissive"));
}

// remove the frontend's "Identifier `NAME' is implicitly declared." entries
// for a joined hierarchical reference from the module's sv_degraded list
// (entries are "<reason> @file:line", joined by "; ")
static void drop_degradation(RTLIL::Module *module, const std::string &prefix);

static void drop_hierref_degradation(RTLIL::Module *module, IdString name)
{
	drop_degradation(module, stringf("Identifier `%s' is implicitly declared. @", name.c_str()));
	// fe190: a reference hierarchy declared at its target's width, whose
	// join flatten makes
	drop_degradation(module, stringf("Hierarchical reference `%s' is not joined yet. @", name.c_str()));
}

static void drop_degradation(RTLIL::Module *module, const std::string &prefix)
{
	IdString attr = IdString("\\sv_degraded");
	if (!module->attributes.count(attr))
		return;
	std::string cur = module->attributes.at(attr).decode_string();
	std::string out;
	size_t pos = 0;
	while (pos <= cur.size()) {
		size_t next = cur.find("; ", pos);
		std::string entry = cur.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
		if (!entry.empty() && entry.compare(0, prefix.size(), prefix) != 0)
			out += (out.empty() ? "" : "; ") + entry;
		if (next == std::string::npos)
			break;
		pos = next + 2;
	}
	if (out.empty())
		module->attributes.erase(attr);
	else
		module->attributes[attr] = RTLIL::Const(out);
}

template<class T>
IdString map_name(RTLIL::Cell *cell, T *object, const std::string &separator = ".")
{
	return cell->module->uniquify(concat_name(cell, object->name, separator));
}

void map_sigspec(const dict<RTLIL::Wire*, RTLIL::Wire*> &map, RTLIL::SigSpec &sig, RTLIL::Module *into = nullptr)
{
	vector<SigChunk> chunks = sig;
	for (auto &chunk : chunks)
		if (chunk.wire != nullptr && chunk.wire->module != into)
			chunk.wire = map.at(chunk.wire);
	sig = chunks;
}

struct FlattenWorker
{
	bool ignore_wb = false;
	bool create_scopeinfo = true;
	bool create_scopename = false;
	std::string separator = ".";
	// fe190: per module, the first name component of every instance and
	// every non-reference dotted name (generate scopes) before flattening:
	// a dotted reference whose head is one of these is downward (23.8) and
	// never looked up further up
	dict<RTLIL::Module*, pool<std::string>> local_heads;

	void collect_local_heads(RTLIL::Design *design)
	{
		auto head_of = [](IdString n) { std::string s = n.unescape(); return s.substr(0, s.find('.')); };
		for (auto mod : design->modules()) {
			if (local_heads.count(mod))
				continue;
			pool<std::string> &h = local_heads[mod];
			for (auto c : mod->cells())
				if (c->name[0] == '\\')
					h.insert(head_of(c->name));
			for (auto w : mod->wires())
				if (w->name[0] == '\\' && !w->get_bool_attribute(ID(sv_hierref)) && !w->has_attribute(ID(sv_upward_ref)) &&
						!w->get_bool_attribute(ID::hierconn) && !w->port_input && !w->port_output &&
						w->name.str().find('.') != std::string::npos)
					h.insert(head_of(w->name));
		}
	}

	// a module's own name as written (`$paramod$<hash>\\name', `$paramod\\name\\P=..',
	// `...\\name$interfaces$..' -> name): an upward name may also match a
	// scope's module name (23.8)
	static std::string module_base_name(RTLIL::Module *m)
	{
		std::string h = m->get_string_attribute(ID::hdlname);
		if (!h.empty())
			return h;
		std::string n = m->name.str();
		if (!n.empty() && n[0] == '\\')
			return n.substr(1);
		size_t p = n.find('\\');
		if (p == std::string::npos)
			return n;
		std::string r = n.substr(p + 1);
		return r.substr(0, r.find_first_of("\\$"));
	}

	// wires a module reads (connections, cell inputs, process rules):
	// a reference nothing reads -- a bind clone's connection to another
	// clone's checker -- cannot reach a check, so it is neither forwarded
	// (it would claim the name at the scope above, at its own width, ahead
	// of a reference that is read) nor marked
	dict<RTLIL::Module*, pool<RTLIL::Wire*>> read_cache;

	const pool<RTLIL::Wire*> &read_wires(RTLIL::Module *module)
	{
		auto it = read_cache.find(module);
		if (it != read_cache.end())
			return it->second;
		pool<RTLIL::Wire*> &read = read_cache[module];
		auto add_read = [&](const RTLIL::SigSpec &sig) {
			for (auto &c : sig.chunks())
				if (c.wire)
					read.insert(c.wire);
		};
		for (auto &conn : module->connections())
			add_read(conn.second);
		for (auto c : module->cells())
			for (auto &conn : c->connections())
				if (!yosys_celltypes.cell_known(c->type) || yosys_celltypes.cell_input(c->type, conn.first))
					add_read(conn.second);
		std::function<void(RTLIL::CaseRule*)> walk = [&](RTLIL::CaseRule *cr) {
			for (auto &cmp : cr->compare)
				add_read(cmp);
			for (auto &a : cr->actions)
				add_read(a.second);
			for (auto sw : cr->switches) {
				add_read(sw->signal);
				for (auto c : sw->cases)
					walk(c);
			}
		};
		for (auto &pit : module->processes) {
			walk(&pit.second->root_case);
			for (auto sync : pit.second->syncs) {
				add_read(sync->signal);
				for (auto &a : sync->actions)
					add_read(a.second);
				for (auto &mw : sync->mem_write_actions) {
					add_read(mw.address);
					add_read(mw.data);
					add_read(mw.enable);
				}
			}
		}
		return read;
	}

	// fe190: wires a module drives (connections, cell outputs, process
	// assignments): a written reference is never forwarded (its join would
	// add a second driver to the target)
	dict<RTLIL::Module*, pool<RTLIL::Wire*>> driven_cache;

	const pool<RTLIL::Wire*> &driven_wires(RTLIL::Module *m)
	{
		auto it = driven_cache.find(m);
		if (it != driven_cache.end())
			return it->second;
		pool<RTLIL::Wire*> &d = driven_cache[m];
		auto add = [&](const RTLIL::SigSpec &sig) {
			for (auto &c : sig.chunks())
				if (c.wire)
					d.insert(c.wire);
		};
		for (auto &conn : m->connections())
			add(conn.first);
		for (auto c : m->cells())
			for (auto &conn : c->connections())
				// fe192: a `\$sva' cell (before sva2check) drives only its
				// Y: a reference an assertion reads directly
				// (`wb_stage_i.vec[3]') is not written
				if (c->type == "\\$sva" ? conn.first == ID::Y :
						(!yosys_celltypes.cell_known(c->type) || yosys_celltypes.cell_output(c->type, conn.first)))
					add(conn.second);
		std::function<void(RTLIL::CaseRule*)> walk = [&](RTLIL::CaseRule *cr) {
			for (auto &a : cr->actions)
				add(a.first);
			for (auto sw : cr->switches)
				for (auto c : sw->cases)
					walk(c);
		};
		for (auto &pit : m->processes) {
			walk(&pit.second->root_case);
			for (auto sync : pit.second->syncs)
				for (auto &a : sync->actions)
					add(a.first);
		}
		return d;
	}

	template<class T>
	void map_attributes(RTLIL::Cell *cell, T *object, IdString orig_object_name)
	{
		if (!create_scopeinfo && object->has_attribute(ID::src))
			object->add_strpool_attribute(ID::src, cell->get_strpool_attribute(ID::src));

		// Preserve original names via the hdlname attribute, but only for objects with a fully public name.
		// If the '-scopename' option is used, also preserve the containing scope of private objects if their scope is fully public.
		if (cell->name[0] == '\\') {
			if (object->has_attribute(ID::hdlname) || orig_object_name[0] == '\\') {
				std::string new_hdlname;

				if (cell->has_attribute(ID::hdlname)) {
					new_hdlname = cell->get_string_attribute(ID(hdlname));
				} else {
					log_assert(!cell->name.empty());
					new_hdlname = cell->name.c_str() + 1;
				}
				new_hdlname += ' ';

				if (object->has_attribute(ID::hdlname)) {
					new_hdlname += object->get_string_attribute(ID(hdlname));
				} else {
					log_assert(!orig_object_name.empty());
					new_hdlname += orig_object_name.c_str() + 1;
				}
				object->set_string_attribute(ID(hdlname), new_hdlname);
			} else if (object->has_attribute(ID(scopename))) {
				std::string new_scopename;

				if (cell->has_attribute(ID::hdlname)) {
					new_scopename = cell->get_string_attribute(ID(hdlname));
				} else {
					log_assert(!cell->name.empty());
					new_scopename = cell->name.c_str() + 1;
				}
				new_scopename += ' ';
				new_scopename += object->get_string_attribute(ID(scopename));
				object->set_string_attribute(ID(scopename), new_scopename);
			} else if (create_scopename) {
				log_assert(!cell->name.empty());
				object->set_string_attribute(ID(scopename), cell->name.c_str() + 1);
			}
		}
	}

	void flatten_cell(RTLIL::Design *design, RTLIL::Module *module, RTLIL::Cell *cell, RTLIL::Module *tpl, SigMap &sigmap, std::vector<RTLIL::Cell*> &new_cells, const std::string &separator)
	{
		inherit_degradation(module, tpl);

		// fe151: an interface instance.  Its members are already wires of
		// the parent, `<cell>.<member>' (expand_interfaces), which modules
		// using the interface connect to; inlining joins each one to the
		// interface's own wire of that name, so the interface's logic
		// drives it (before, the inlined wire was uniquified to
		// `<cell>.<member>_1' and the parent's wire stayed undriven).  A
		// joined member is no longer an implicit wire.  The copy of the
		// interface that derive puts into a module using it through a port
		// (sv_intf_port_copy) stands for that instance: its members are the
		// module's ports, so it is dropped rather than inlined a second time.
		bool intf_instance = tpl->get_bool_attribute(ID::is_interface) && cell->get_bool_attribute(ID::is_interface);
		if (intf_instance && cell->get_bool_attribute(ID(sv_intf_port_copy))) {
			// only a member that is a port here: one outside the
			// port's modport, read anyway, is a free implicit wire
			for (auto tpl_wire : tpl->wires()) {
				if (tpl_wire->name[0] != '\\')
					continue;
				IdString mname = concat_name(cell, tpl_wire->name, separator);
				RTLIL::Wire *mw = module->wire(mname);
				if (mw && (mw->port_input || mw->port_output) && !mw->get_bool_attribute(ID(sv_hierref)))
					drop_hierref_degradation(module, mname);
			}
			log("Dropping %s.%s (the interface of port `%s', instantiated in a parent).\n", module, cell, cell->name.unescape());
			module->remove(cell);
			return;
		}
		bool intf_all_joined = true;

		// Copy the contents of the flattened cell

		dict<IdString, IdString> memory_map;
		for (auto &tpl_memory_it : tpl->memories) {
			RTLIL::Memory *new_memory = module->addMemory(map_name(cell, tpl_memory_it.second, separator), tpl_memory_it.second);
			map_attributes(cell, new_memory, tpl_memory_it.second->name);
			memory_map[tpl_memory_it.first] = new_memory->name;
			design->select(module, new_memory);
		}

		dict<RTLIL::Wire*, RTLIL::Wire*> wire_map;
		dict<IdString, IdString> positional_ports;
		for (auto tpl_wire : tpl->wires()) {
			if (tpl_wire->port_id > 0)
				positional_ports.emplace(stringf("$%d", tpl_wire->port_id), tpl_wire->name);

			RTLIL::Wire *new_wire = nullptr;
			if (tpl_wire->name[0] == '\\') {
				RTLIL::Wire *hier_wire = module->wire(concat_name(cell, tpl_wire->name, separator));
				// an implicit wire the Verilog frontend made for a downward
				// hierarchical reference (sv_hierref): it is one bit, unless
				// hierarchy declared it at its target's width
				// (AstModule::resolve_hierrefs), so a target of the same
				// width is joined (widening it here would leave the reads,
				// sized and folded at width 1, on its bit 0); joined, the
				// reference is no longer a free wire and its "implicitly
				// declared" degradation is dropped from the module
				if (hier_wire != nullptr && hier_wire->get_bool_attribute(ID(sv_hierref)) &&
						hier_wire->get_bool_attribute(ID::hierconn)) {
					if (GetSize(hier_wire) != GetSize(tpl_wire)) {
						log_warning("Hierarchical reference %s.%s is not joined to %s.%s (width %d, not %d); it stays a free wire.\n",
							module, hier_wire, tpl, tpl_wire, GetSize(tpl_wire), GetSize(hier_wire));
						hier_wire->attributes.erase(ID::hierconn);
						hier_wire = nullptr;
					} else {
						hier_wire->attributes.erase(ID(sv_hierref));
						drop_hierref_degradation(module, hier_wire->name);
					}
				}
				if (intf_instance && hier_wire != nullptr && !hier_wire->get_bool_attribute(ID::hierconn) &&
						!hier_wire->port_input && !hier_wire->port_output) {
					if (GetSize(hier_wire) == GetSize(tpl_wire)) {
						drop_hierref_degradation(module, hier_wire->name);
						// the interface's initial value (`logic c = 0;')
						if (tpl_wire->attributes.count(ID::init) && !hier_wire->attributes.count(ID::init))
							hier_wire->attributes[ID::init] = tpl_wire->attributes.at(ID::init);
						new_wire = hier_wire;
					} else {
						log_warning("Interface member %s.%s is not joined to %s.%s (width %d, not %d).\n",
							module, hier_wire, tpl, tpl_wire, GetSize(hier_wire), GetSize(tpl_wire));
						intf_all_joined = false;
					}
				}
				if (hier_wire != nullptr && hier_wire->get_bool_attribute(ID::hierconn)) {
					hier_wire->attributes.erase(ID::hierconn);
					if (GetSize(hier_wire) < GetSize(tpl_wire)) {
						log_warning("Widening signal %s.%s to match size of %s.%s (via %s.%s).\n",
							module, hier_wire, tpl, tpl_wire, module, cell);
						hier_wire->width = GetSize(tpl_wire);
					}
					new_wire = hier_wire;
				}
			}
			// fe190: a dotted reference of the template that is still
			// unjoined (its head is no instance there: an upward name,
			// IEEE 1800-2017 23.8, `core_i.data_req_o' read in a checker
			// bound three levels below the wrapper that holds core_i) is
			// looked up from here under its own name: joined to this
			// module's wire of that name (the head instance already
			// inlined, or another reference forwarded here), else made a
			// reference here, joined when the head is inlined or forwarded
			// further.  Unjoined at the top it stays a degraded free wire.
			if (new_wire == nullptr && tpl_wire->name[0] == '\\' && tpl_wire->get_bool_attribute(ID(sv_hierref)) &&
					tpl_wire->get_bool_attribute(ID::hierconn) && !tpl_wire->has_attribute(ID(sv_hierref_base)) &&
					!tpl_wire->port_input && !tpl_wire->port_output &&
					tpl_wire->name.str().find('.') != std::string::npos &&
					local_heads.count(tpl) && !local_heads.at(tpl).count(tpl_wire->name.unescape().substr(0, tpl_wire->name.unescape().find('.'))) &&
					module_base_name(tpl) != tpl_wire->name.unescape().substr(0, tpl_wire->name.unescape().find('.')) &&
					module_base_name(module) != tpl_wire->name.unescape().substr(0, tpl_wire->name.unescape().find('.')) &&
					!driven_wires(tpl).count(tpl_wire) && read_wires(tpl).count(tpl_wire)) {
				// (a head naming the template's or this module's own module
				// is an upward reference by module name, resolved at that
				// scope or not at all: never looked up further)
				RTLIL::Wire *up = module->wire(tpl_wire->name);
				if (up == nullptr) {
					up = module->addWire(tpl_wire->name, GetSize(tpl_wire));
					up->is_signed = tpl_wire->is_signed;
					up->set_bool_attribute(ID::hierconn);
					up->set_bool_attribute(ID(sv_hierref));
					log("Forwarding hierarchical reference %s.%s to %s (no instance `%s' below).\n", log_id(cell), log_id(tpl_wire),
							log_id(module), tpl_wire->name.unescape().substr(0, tpl_wire->name.unescape().find('.')).c_str());
					new_wire = up;
				} else if (GetSize(up) == GetSize(tpl_wire) && !up->port_output &&
						(!up->port_input || up->attributes.count(ID(sv_upward_ref)))) {
					// (an input port: this module's own reference to the same
					// name, resolved one level up -- the same target, 23.8)
					if (!up->get_bool_attribute(ID(sv_hierref)))
						drop_hierref_degradation(module, up->name);
					new_wire = up;
				}
			}
			if (new_wire == nullptr) {
				new_wire = module->addWire(map_name(cell, tpl_wire, separator), tpl_wire);
				new_wire->port_input = new_wire->port_output = false;
				new_wire->port_id = false;
			}

			map_attributes(cell, new_wire, tpl_wire->name);
			wire_map[tpl_wire] = new_wire;
			design->select(module, new_wire);
		}

		// fe166: a reference to a member of a packed struct / union wire of
		// this instance (resolve_hierrefs tagged it with the wire's name
		// here and the member's offset): it reads that slice
		{
			dict<IdString, RTLIL::Wire*> inlined;
			for (auto &it : wire_map)
				if (it.first->name[0] == '\\')
					inlined[concat_name(cell, it.first->name, separator)] = it.second;
			std::vector<RTLIL::Wire*> refs;
			for (auto w : module->wires())
				if (w->has_attribute(ID(sv_hierref_base)) && w->get_bool_attribute(ID(sv_hierref)))
					refs.push_back(w);
			for (auto w : refs) {
				auto it = inlined.find(RTLIL::escape_id(w->get_string_attribute(ID(sv_hierref_base))));
				if (it == inlined.end())
					continue;
				int offset = w->attributes.at(ID(sv_hierref_offset)).as_int();
				w->attributes.erase(ID(sv_hierref_base));
				w->attributes.erase(ID(sv_hierref_offset));
				w->attributes.erase(ID::hierconn);
				if (offset < 0 || offset + GetSize(w) > GetSize(it->second)) {
					log_warning("Hierarchical reference %s.%s is not joined to bits %d+:%d of %s (width %d); it stays a free wire.\n",
						module, w, offset, GetSize(w), log_id(it->second->name), GetSize(it->second));
					continue;
				}
				module->connect(w, RTLIL::SigSpec(it->second, offset, GetSize(w)));
				w->attributes.erase(ID(sv_hierref));
				drop_hierref_degradation(module, w->name);
			}
		}

		// the pending mark of expand_interfaces holds until every member
		// is joined
		if (intf_instance && intf_all_joined)
			drop_degradation(module, stringf("Interface `%s' (%s) has logic that is connected only by flatten. @cell %s",
					cell->name.unescape(), tpl->name.unescape(), cell->name.unescape()));

		for (auto &tpl_proc_it : tpl->processes) {
			RTLIL::Process *new_proc = module->addProcess(map_name(cell, tpl_proc_it.second, separator), tpl_proc_it.second);
			map_attributes(cell, new_proc, tpl_proc_it.second->name);
			for (auto new_proc_sync : new_proc->syncs)
				for (auto &memwr_action : new_proc_sync->mem_write_actions)
					memwr_action.memid = memory_map.at(memwr_action.memid).str();
			auto rewriter = [&](RTLIL::SigSpec &sig) { map_sigspec(wire_map, sig); };
			new_proc->rewrite_sigspecs(rewriter);
			design->select(module, new_proc);
		}

		for (auto tpl_cell : tpl->cells()) {
			RTLIL::Cell *new_cell = module->addCell(map_name(cell, tpl_cell, separator), tpl_cell);
			map_attributes(cell, new_cell, tpl_cell->name);
			if (new_cell->has_memid()) {
				IdString memid = new_cell->getParam(ID::MEMID).decode_string();
				new_cell->setParam(ID::MEMID, Const(memory_map.at(memid).str()));
			} else if (new_cell->is_mem_cell()) {
				IdString memid = new_cell->getParam(ID::MEMID).decode_string();
				new_cell->setParam(ID::MEMID, Const(concat_name(cell, memid, separator).str()));
			}
			auto rewriter = [&](RTLIL::SigSpec &sig) { map_sigspec(wire_map, sig); };
			new_cell->rewrite_sigspecs(rewriter);
			design->select(module, new_cell);
			new_cells.push_back(new_cell);
		}

		for (auto &tpl_conn_it : tpl->connections()) {
			RTLIL::SigSig new_conn = tpl_conn_it;
			map_sigspec(wire_map, new_conn.first);
			map_sigspec(wire_map, new_conn.second);
			module->connect(new_conn);
		}

		// Attach port connections of the flattened cell

		pool<SigBit> tpl_driven;
		for (auto tpl_cell : tpl->cells())
			for (auto &tpl_conn : tpl_cell->connections())
				if (tpl_cell->output(tpl_conn.first))
					for (auto bit : tpl_conn.second)
						tpl_driven.insert(bit);
		for (auto &tpl_conn : tpl->connections())
			for (auto bit : tpl_conn.first)
				tpl_driven.insert(bit);

		for (auto &port_it : cell->connections())
		{
			IdString port_name = port_it.first;
			if (positional_ports.count(port_name) > 0)
				port_name = positional_ports.at(port_name);
			if (tpl->wire(port_name) == nullptr || tpl->wire(port_name)->port_id == 0) {
				if (port_name.begins_with("$"))
					log_error("Can't map port `%s' of cell `%s' to template `%s'!\n",
						port_name.c_str(), cell->name.c_str(), tpl->name.c_str());
				continue;
			}

			if (GetSize(port_it.second) == 0)
				continue;

			RTLIL::Wire *tpl_wire = tpl->wire(port_name);
			RTLIL::SigSig new_conn;
			bool is_signed = false;
			if (tpl_wire->port_output && !tpl_wire->port_input) {
				new_conn.first = port_it.second;
				new_conn.second = tpl_wire;
				is_signed = tpl_wire->is_signed;
			} else if (!tpl_wire->port_output && tpl_wire->port_input) {
				new_conn.first = tpl_wire;
				new_conn.second = port_it.second;
				is_signed = new_conn.second.is_wire() && new_conn.second.as_wire()->is_signed;
			} else {
				SigSpec sig_tpl = tpl_wire, sig_mod = port_it.second;
				for (int i = 0; i < GetSize(sig_tpl) && i < GetSize(sig_mod); i++) {
					if (tpl_driven.count(sig_tpl[i])) {
						new_conn.first.append(sig_mod[i]);
						new_conn.second.append(sig_tpl[i]);
					} else {
						new_conn.first.append(sig_tpl[i]);
						new_conn.second.append(sig_mod[i]);
					}
				}
			}
			map_sigspec(wire_map, new_conn.first, module);
			map_sigspec(wire_map, new_conn.second, module);

			if (new_conn.second.size() > new_conn.first.size())
				new_conn.second.remove(new_conn.first.size(), new_conn.second.size() - new_conn.first.size());
			if (new_conn.second.size() < new_conn.first.size())
				new_conn.second.extend_u0(new_conn.first.size(), is_signed);
			log_assert(new_conn.first.size() == new_conn.second.size());

			if (sigmap(new_conn.first).has_const())
				log_error("Cell port %s.%s.%s is driving constant bits: %s <= %s\n",
					module, cell, port_it.first.unescape(), log_signal(new_conn.first), log_signal(new_conn.second));

			module->connect(new_conn);
			sigmap.add(new_conn.first, new_conn.second);
		}

		RTLIL::Cell *scopeinfo = nullptr;
		RTLIL::IdString cell_name = cell->name;

		if (create_scopeinfo && cell_name.isPublic())
		{
			// The $scopeinfo's name will be changed below after removing the flattened cell
			scopeinfo = module->addCell(NEW_ID, ID($scopeinfo));
			scopeinfo->setParam(ID::TYPE, RTLIL::Const("module"));

			for (auto const &attr : cell->attributes)
			{
				if (attr.first == ID::hdlname)
					scopeinfo->attributes.insert(attr);
				else
					scopeinfo->attributes.emplace(stringf("\\cell_%s", attr.first.unescape()), attr.second);
			}

			for (auto const &attr : tpl->attributes)
				scopeinfo->attributes.emplace(stringf("\\module_%s", attr.first.unescape()), attr.second);

			scopeinfo->attributes.emplace(ID(module), tpl->name.unescape());
		}

		module->remove(cell);

		if (scopeinfo != nullptr)
			module->rename(scopeinfo, cell_name);
	}

	// An upward reference in a bound checker (`bind dut chk_m u_chk(..)`,
	// chk_m reading `dut.sig`) was turned into an input port by hierarchy
	// (AstModule::resolve_hierrefs, sv_upward_ref = the path below the
	// parent). Connect it in this parent before any cell is inlined, so a
	// path that goes on down (`dut.u_sub.cnt`) is a downward reference here
	// that the inlining of u_sub joins. An instance whose parent lacks the
	// target keeps a free input, recorded as a degradation.
	static void connect_upward_refs(RTLIL::Design *design, RTLIL::Module *module)
	{
		for (auto cell : module->cells()) {
			RTLIL::Module *tpl = design->module(cell->type);
			if (!tpl)
				continue;
			for (auto port : tpl->ports) {
				RTLIL::Wire *pw = tpl->wire(port);
				if (!pw || !pw->port_input || !pw->attributes.count(ID(sv_upward_ref)) || cell->hasPort(port))
					continue;
				std::string rest = pw->attributes.at(ID(sv_upward_ref)).decode_string();
				// fe168: a member of the wire `rest' (its offset, and the
				// wire's width)
				int moff = pw->attributes.count(ID(sv_upward_ref_offset)) ? pw->attributes.at(ID(sv_upward_ref_offset)).as_int() : -1;
				int want = moff >= 0 ? pw->attributes.at(ID(sv_upward_ref_width)).as_int() : GetSize(pw);
				RTLIL::IdString rest_id = RTLIL::escape_id(rest);
				RTLIL::Wire *src = module->wire(rest_id);
				if (!src && rest.find('.') != std::string::npos) {
					src = module->addWire(rest_id, want);
					src->is_signed = pw->is_signed;
					src->set_bool_attribute(ID::hierconn);
					src->set_bool_attribute(ID(sv_hierref));
				}
				bool dotted = rest.find('.') != std::string::npos;
				if (!src || GetSize(src) != want || (!dotted && src->get_bool_attribute(ID(sv_hierref))) ||
						(moff >= 0 && moff + GetSize(pw) > want)) {
					log_warning("Upward reference %s.%s is not connected in %s (no %d-bit `%s' there); it stays a free input.\n",
							log_id(cell), log_id(port), log_id(module), GetSize(pw), rest.c_str());
					IdString attr = IdString("\\sv_degraded");
					std::string cur = module->attributes.count(attr) ? module->attributes.at(attr).decode_string() : std::string();
					cur += (cur.empty() ? "" : "; ") + stringf("Upward reference `%s' of %s is not connected (no %d-bit `%s' here). @cell %s",
							log_id(port), log_id(cell->type), GetSize(pw), rest.c_str(), log_id(cell));
					module->attributes[attr] = RTLIL::Const(cur);
					continue;
				}
				if (moff >= 0) {
					log("Connecting upward reference %s.%s to bits %d+:%d of %s.%s.\n", log_id(cell), log_id(port), moff, GetSize(pw),
							log_id(module), log_id(src));
					cell->setPort(port, RTLIL::SigSpec(src, moff, GetSize(pw)));
					continue;
				}
				log("Connecting upward reference %s.%s to %s.%s.\n", log_id(cell), log_id(port), log_id(module), log_id(src));
				cell->setPort(port, src);
			}
		}
	}

	void flatten_module(RTLIL::Design *design, RTLIL::Module *module, pool<RTLIL::Module*> &used_modules, const std::string &separator)
	{
		if (!design->selected(module) || module->get_blackbox_attribute(ignore_wb))
			return;

		connect_upward_refs(design, module);

		SigMap sigmap(module);
		std::vector<RTLIL::Cell*> worklist = module->selected_cells();
		while (!worklist.empty())
		{
			RTLIL::Cell *cell = worklist.back();
			worklist.pop_back();

			if (!design->has(cell->type))
				continue;

			RTLIL::Module *tpl = design->module(cell->type);
			if (tpl->get_blackbox_attribute(ignore_wb))
				continue;

			if (cell->get_bool_attribute(ID::keep_hierarchy) || tpl->get_bool_attribute(ID::keep_hierarchy)) {
				log("Keeping %s.%s (found keep_hierarchy attribute).\n", module, cell);
				used_modules.insert(tpl);
				continue;
			}

			log_debug("Flattening %s.%s (%s).\n", module, cell, cell->type.unescape());
			// If a design is fully selected and has a top module defined, topological sorting ensures that all cells
			// added during flattening are black boxes, and flattening is finished in one pass. However, when flattening
			// individual modules, this isn't the case, and the newly added cells might have to be flattened further.
			flatten_cell(design, module, cell, tpl, sigmap, worklist, separator);
		}
	}

	// fe190: a reference still unjoined once flattening is done is a free
	// wire.  The reasons dropped on joins are matched by name, and
	// references of different templates share a name once forwarded or
	// inherited, so each one left keeps a reason of its own.  (Run after
	// every module is flattened: a template's references are joined by its
	// parents.)
	void mark_unjoined(RTLIL::Module *module)
	{
		read_cache.erase(module);
		const pool<RTLIL::Wire*> &read = read_wires(module);
		IdString attr = IdString("\\sv_degraded");
		std::string cur = module->has_attribute(attr) ? module->attributes.at(attr).decode_string() : std::string();
		std::string add;
		for (auto w : module->wires())
			if (w->name[0] == '\\' && w->get_bool_attribute(ID(sv_hierref)) && !w->port_input && !w->port_output &&
					w->name.str().find('.') != std::string::npos && read.count(w)) {
				std::string reason = stringf("Hierarchical reference `%s' is not joined (a free wire). @module %s",
						log_id(w->name), log_id(module->name));
				if (cur.find(reason) == std::string::npos && add.find(reason) == std::string::npos)
					add += (add.empty() && cur.empty() ? "" : "; ") + reason;
			}
		if (!add.empty())
			module->attributes[attr] = RTLIL::Const(cur + add);
	}
};

struct FlattenPass : public Pass {
	FlattenPass() : Pass("flatten", "flatten design") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    flatten [options] [selection]\n");
		log("\n");
		log("This pass flattens the design by replacing cells by their implementation. This\n");
		log("pass is very similar to the 'techmap' pass. The only difference is that this\n");
		log("pass is using the current design as mapping library.\n");
		log("\n");
		log("Cells and/or modules with the 'keep_hierarchy' attribute set will not be\n");
		log("flattened by this command.\n");
		log("\n");
		log("    -wb\n");
		log("        Ignore the 'whitebox' attribute on cell implementations.\n");
		log("\n");
		log("    -noscopeinfo\n");
		log("        Do not create '$scopeinfo' cells that preserve attributes of cells and\n");
		log("        modules that were removed during flattening. With this option, the\n");
		log("        'src' attribute of a given cell is merged into all objects replacing\n");
		log("        that cell, with multiple distinct 'src' locations separated by '|'.\n");
		log("        Without this option these 'src' locations can be found via the\n");
		log("        cell_src' and 'module_src' attribute of '$scopeinfo' cells.\n");
		log("\n");
		log("    -scopename\n");
		log("        Create 'scopename' attributes for objects with a private name. This\n");
		log("        attribute records the 'hdlname' of the enclosing scope. For objects\n");
		log("        with a public name the enclosing scope can be found via their\n");
		log("        'hdlname' attribute.\n");
		log("\n");
		log("    -separator <char>\n");
		log("        Use this separator char instead of '.' when concatenating design levels.\n");
		log("\n");
		log("    -nocleanup\n");
		log("        Don't remove unused submodules, leave a flattened version of each\n");
		log("        submodule in the design.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing FLATTEN pass (flatten design).\n");
		log_push();

		FlattenWorker worker;

		if (design->scratchpad.count("flatten.separator"))
			worker.separator = design->scratchpad_get_string("flatten.separator");

		bool cleanup = true;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-wb") {
				worker.ignore_wb = true;
				continue;
			}
			if (args[argidx] == "-noscopeinfo") {
				worker.create_scopeinfo = false;
				continue;
			}
			if (args[argidx] == "-scopename") {
				worker.create_scopename = true;
				continue;
			}
			if (args[argidx] == "-separator" && argidx + 1 < args.size()) {
				worker.separator = args[++argidx];
				continue;
			}
			if (args[argidx] == "-nocleanup") {
				cleanup = false;
				continue;
			}
			break;
		}
		extra_args(args, argidx, design);

		RTLIL::Module *top = nullptr;
		if (design->full_selection())
			for (auto module : design->modules())
				if (module->get_bool_attribute(ID::top))
					top = module;

		pool<RTLIL::Module*> used_modules;
		if (top == nullptr)
			used_modules = design->modules();
		else
			used_modules.insert(top);

		TopoSort<RTLIL::Module*, IdString::compare_ptr_by_name<RTLIL::Module>> topo_modules;
		pool<RTLIL::Module*> worklist = used_modules;
		while (!worklist.empty()) {
			RTLIL::Module *module = worklist.pop();
			for (auto cell : module->selected_cells()) {
				RTLIL::Module *tpl = design->module(cell->type);
				if (tpl != nullptr) {
                                        if (!topo_modules.has_node(tpl))
						worklist.insert(tpl);
					topo_modules.edge(tpl, module);
				}
			}
		}

		if (!topo_modules.sort())
			log_error("Cannot flatten a design containing recursive instantiations.\n");

		worker.collect_local_heads(design);
		for (auto module : topo_modules.sorted)
			worker.flatten_module(design, module, used_modules, worker.separator);
		for (auto module : topo_modules.sorted)
			if (design->selected(module) && !module->get_blackbox_attribute(worker.ignore_wb))
				worker.mark_unjoined(module);

		if (cleanup && top != nullptr)
			for (auto module : design->modules().to_vector())
				if (!used_modules[module] && !module->get_blackbox_attribute(worker.ignore_wb)) {
					log("Deleting now unused module %s.\n", module);
					design->remove(module);
				}

		log_pop();
	}
} FlattenPass;

PRIVATE_NAMESPACE_END
