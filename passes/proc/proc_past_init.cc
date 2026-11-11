/*
 *  yosys -- Yosys Open SYnthesis Suite
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

// fe156: IEEE 1800-2017 16.9.3: before k clock ticks have elapsed,
// $past(e, k) returns e evaluated on the initial values of its variables.
// The $past registers (wires tagged (* sv_past *) by the frontend and by
// sva2check) get that value as their init: the first register of a chain
// takes its D input evaluated with every flip-flop at its init and
// everything else (inputs, uninitialised registers, memories, instances,
// loops) free; a later register takes its predecessor's init.  Bits that do
// not evaluate to a defined value stay free, as before.

#include "kernel/yosys.h"
#include "kernel/consteval.h"
#include "kernel/sigtools.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

static bool is_past_q(const SigSpec &q)
{
	if (q.empty())
		return false;
	for (auto &c : q.chunks())
		if (!c.wire || !c.wire->get_bool_attribute(ID(sv_past)))
			return false;
	return true;
}

static State init_bit(SigBit b)
{
	if (!b.wire || !b.wire->attributes.count(ID::init))
		return b.wire ? State::Sx : b.data;
	const Const &iv = b.wire->attributes.at(ID::init);
	return b.offset < GetSize(iv) ? iv[b.offset] : State::Sx;
}

static void set_init_bit(SigBit b, State s)
{
	Const iv = b.wire->attributes.count(ID::init) ? b.wire->attributes.at(ID::init) : Const(State::Sx, b.wire->width);
	if (GetSize(iv) < b.wire->width)
		iv.resize(b.wire->width, State::Sx);
	iv.set(b.offset, s);
	b.wire->attributes[ID::init] = iv;
}

static int past_init_module(Module *module)
{
	std::vector<Cell *> past_ffs, ffs;
	for (auto cell : module->cells()) {
		if (!RTLIL::builtin_ff_cell_types().count(cell->type) || !cell->hasPort(ID::Q))
			continue;
		ffs.push_back(cell);
		if (cell->hasPort(ID::D) && is_past_q(cell->getPort(ID::Q)))
			past_ffs.push_back(cell);
	}
	if (past_ffs.empty())
		return 0;

	int count = 0;
	pool<Cell *> done;
	// a chain resolves one register per round (its predecessor's init is
	// a flip-flop init the next round sees)
	for (int round = 0; round <= GetSize(past_ffs); round++) {
		ConstEval ce(module, State::Sx);
		for (auto cell : ffs) {
			SigSpec q = cell->getPort(ID::Q);
			for (int i = 0; i < GetSize(q); i++) {
				State s = init_bit(q[i]);
				if (s == State::S0 || s == State::S1)
					ce.set(q[i], Const(s));
			}
		}
		// case equality reads x as a value; here x means free, so a
		// cone through one is not evaluated (the register stays free)
		for (auto cell : module->cells())
			if (cell->type.in(ID($eqx), ID($nex), ID($bweqx)))
				ce.stop(cell->getPort(ID::Y));
		bool changed = false;
		for (auto cell : past_ffs) {
			if (done.count(cell))
				continue;
			SigSpec q = cell->getPort(ID::Q);
			SigSpec d = cell->getPort(ID::D);
			SigSpec undef;
			ce.push();
			bool ok = ce.eval(d, undef);
			ce.pop();
			if (!ok) {
				done.insert(cell);
				continue;
			}
			bool any = false;
			for (int i = 0; i < GetSize(q); i++) {
				State s = d[i].wire ? State::Sx : d[i].data;
				if ((s == State::S0 || s == State::S1) && init_bit(q[i]) != s) {
					State cur = init_bit(q[i]);
					if (cur == State::S0 || cur == State::S1)
						continue;	// an existing defined init wins
					set_init_bit(q[i], s);
					any = true;
				}
			}
			if (any) {
				count++;
				changed = true;
				done.insert(cell);
			}
		}
		if (!changed)
			break;
	}
	return count;
}

struct ProcPastInitPass : public Pass {
	ProcPastInitPass() : Pass("proc_past_init", "initial values of $past registers") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    proc_past_init [selection]\n");
		log("\n");
		log("Give the $past registers (wires marked (* sv_past *)) their value before\n");
		log("enough clock ticks have elapsed (IEEE 1800-2017 16.9.3): the sampled\n");
		log("expression evaluated on the initial values of its variables.  Bits that\n");
		log("depend on an input or an uninitialised register stay free.\n");
		log("This pass is called by 'proc' and 'sva2check'.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing PROC_PAST_INIT pass ($past initial values).\n");
		extra_args(args, 1, design);
		int count = 0;
		for (auto mod : design->selected_modules())
			if (mod->processes.empty())
				count += past_init_module(mod);
		if (count)
			log("Set the initial value of %d $past register(s).\n", count);
	}
} ProcPastInitPass;

PRIVATE_NAMESPACE_END
