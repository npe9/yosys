/*
 *  In-tree checker for safety certificates of external model checkers;
 *  see aiger_cert.h.
 */

#include "passes/ic3/aiger_cert.h"
#include "kernel/satgen.h"
#include <fstream>
#include <sstream>

YOSYS_NAMESPACE_BEGIN

namespace ic3_cert {

namespace {

bool read_uint(std::istream &f, unsigned &v)
{
	// binary AIGER delta encoding: 7 bits per byte, high bit = more
	v = 0;
	int shift = 0;
	while (true) {
		int c = f.get();
		if (c == EOF)
			return false;
		v |= (unsigned)(c & 0x7f) << shift;
		if (!(c & 0x80))
			return true;
		shift += 7;
		if (shift > 28)
			return false;
	}
}

// per-time-frame values of one circuit's literals
struct Frame {
	ezSAT *ez;
	const Aig *aig;
	dict<int, int> var;	// AIGER variable -> ez literal (inputs, latches, gates)
	dict<int, const std::array<int, 3> *> gate;

	Frame(ezSAT *ez, const Aig *aig) : ez(ez), aig(aig)
	{
		for (auto &a : aig->ands)
			gate[a[0] >> 1] = &a;
	}
	int lit(int l)
	{
		int v = l >> 1;
		int x;
		if (v == 0)
			x = ez->CONST_FALSE;
		else {
			auto it = var.find(v);
			if (it != var.end())
				x = it->second;
			else {
				auto git = gate.find(v);
				if (git == gate.end()) {
					// an unassigned input or latch: a free variable
					x = ez->literal();
				} else {
					int a = lit((*git->second)[1]), b = lit((*git->second)[2]);
					x = ez->AND(a, b);
				}
				var[v] = x;
			}
		}
		return (l & 1) ? ez->NOT(x) : x;
	}
	void set(int l, int x) { var[l >> 1] = (l & 1) ? ez->NOT(x) : x; }
	int all(const std::vector<int> &lits, bool negate)
	{
		std::vector<int> v;
		for (int l : lits)
			v.push_back(negate ? ez->NOT(lit(l)) : lit(l));
		return v.empty() ? ez->CONST_TRUE : ez->expression(ezSAT::OpAnd, v);
	}
	int constraints() { return all(aig->constraints, false); }
	int property() { return all(aig->bad_lits(), true); }
	// R: latches with a 0/1 reset take it (uninitialized latches are free)
	int reset()
	{
		std::vector<int> v;
		for (int i = 0; i < GetSize(aig->latch_lit); i++) {
			int r = aig->latch_reset[i];
			if (r == 0 || r == 1)
				v.push_back(r ? lit(aig->latch_lit[i]) : ez->NOT(lit(aig->latch_lit[i])));
		}
		return v.empty() ? ez->CONST_TRUE : ez->expression(ezSAT::OpAnd, v);
	}
};

// a symbol "= <lit>": the model literal this witness element simulates
bool mapped(const Aig &w, char kind, int idx, int &lit)
{
	auto it = w.symbols.find(stringf("%c%d", kind, idx));
	if (it == w.symbols.end())
		return false;
	const std::string &s = it->second;
	size_t p = s.find_first_not_of(' ');
	if (p == std::string::npos || s[p] != '=')
		return false;
	lit = atoi(s.c_str() + p + 1);
	return true;
}

// is `cond` valid?  (UNSAT of its negation; a timeout is a failure)
bool valid(ezSAT *ez, int cond, double timeout_s, const char *name, std::string &why)
{
	if (timeout_s > 0)
		ez->setSolverTimeout(std::max(1, (int)timeout_s));
	bool sat = ez->solve(ez->NOT(cond));
	if (ez->getSolverTimeoutStatus()) {
		why = stringf("%s check timed out", name);
		return false;
	}
	if (sat) {
		why = stringf("%s check failed", name);
		return false;
	}
	return true;
}

}

bool Aig::parse(const std::string &file, std::string &err)
{
	std::ifstream f(file, std::ios::binary);
	if (!f) {
		err = "cannot open " + file;
		return false;
	}
	std::string header;
	std::getline(f, header);
	std::istringstream hs(header);
	std::string fmt;
	int I = 0, L = 0, O = 0, A = 0, B = 0, C = 0, J = 0, F = 0;
	hs >> fmt >> maxvar >> I >> L >> O >> A;
	if (!(hs >> B)) B = 0;
	if (!(hs >> C)) C = 0;
	if (!(hs >> J)) J = 0;
	if (!(hs >> F)) F = 0;
	if (fmt != "aag" && fmt != "aig") {
		err = "not an AIGER file: " + file;
		return false;
	}
	if (J || F) {
		err = "justice/fairness sections are not a safety certificate";
		return false;
	}
	bool binary = fmt == "aig";
	auto line_ints = [&](std::vector<int> &out) {
		std::string l;
		if (!std::getline(f, l))
			return false;
		std::istringstream ls(l);
		int x;
		while (ls >> x)
			out.push_back(x);
		return true;
	};
	for (int i = 0; i < I; i++) {
		if (binary)
			inputs.push_back(2 * (i + 1));
		else {
			std::vector<int> v;
			if (!line_ints(v) || v.size() != 1) { err = "bad input line"; return false; }
			inputs.push_back(v[0]);
		}
	}
	for (int i = 0; i < L; i++) {
		std::vector<int> v;
		if (!line_ints(v)) { err = "bad latch line"; return false; }
		int lit = binary ? 2 * (I + i + 1) : (v.empty() ? -1 : v[0]);
		size_t o = binary ? 0 : 1;
		if (v.size() < o + 1) { err = "bad latch line"; return false; }
		int next = v[o];
		int reset = v.size() > o + 1 ? v[o + 1] : 0;
		if (reset != 0 && reset != 1 && reset != lit) {
			err = "latch reset to another literal is not supported";
			return false;
		}
		latch_lit.push_back(lit);
		latch_next.push_back(next);
		latch_reset.push_back(reset == lit ? -1 : reset);
	}
	auto read_list = [&](int n, std::vector<int> &out) {
		for (int i = 0; i < n; i++) {
			std::vector<int> v;
			if (!line_ints(v) || v.size() != 1)
				return false;
			out.push_back(v[0]);
		}
		return true;
	};
	if (!read_list(O, outputs) || !read_list(B, bads) || !read_list(C, constraints)) {
		err = "bad output/bad/constraint line";
		return false;
	}
	for (int i = 0; i < A; i++) {
		if (binary) {
			unsigned d0, d1;
			int lhs = 2 * (I + L + i + 1);
			if (!read_uint(f, d0) || !read_uint(f, d1)) { err = "truncated and-gate section"; return false; }
			int r0 = lhs - (int)d0, r1 = r0 - (int)d1;
			ands.push_back({lhs, r0, r1});
		} else {
			std::vector<int> v;
			if (!line_ints(v) || v.size() != 3) { err = "bad and line"; return false; }
			ands.push_back({v[0], v[1], v[2]});
		}
	}
	std::string l;
	while (std::getline(f, l)) {
		if (l.empty())
			continue;
		if (l[0] == 'c')
			break;
		size_t sp = l.find(' ');
		if (sp == std::string::npos)
			continue;
		symbols[l.substr(0, sp)] = l.substr(sp + 1);
	}
	return true;
}

bool check_witness(const Aig &model, const Aig &w, double timeout_s, std::string &why)
{
	// K: witness inputs/latches mapped onto model literals
	std::vector<int> win_map(w.inputs.size(), -1), wl_map(w.latch_lit.size(), -1);
	for (int i = 0; i < GetSize(w.inputs); i++) {
		int l;
		if (mapped(w, 'i', i, l))
			win_map[i] = l;
	}
	for (int i = 0; i < GetSize(w.latch_lit); i++) {
		int l;
		if (mapped(w, 'l', i, l))
			wl_map[i] = l;
	}
	for (int l : win_map)
		if (l > 2 * model.maxvar + 1) { why = "mapping names a literal outside the model"; return false; }
	for (int l : wl_map)
		if (l > 2 * model.maxvar + 1) { why = "mapping names a literal outside the model"; return false; }

	// the witness frame that simulates a model frame: mapped elements are
	// the model literals' values, unmapped ones free
	auto sim_frame = [&](Frame &mf, Frame &wf) {
		for (int i = 0; i < GetSize(w.inputs); i++)
			if (win_map[i] >= 0)
				wf.set(w.inputs[i], mf.lit(win_map[i]));
		for (int i = 0; i < GetSize(w.latch_lit); i++)
			if (wl_map[i] >= 0)
				wf.set(w.latch_lit[i], mf.lit(wl_map[i]));
	};

	// reset: R_s ^ C_s -> R'_s{K} ^ C'_s
	{
		ezSatPtr ez;
		Frame ms(ez.get(), &model), ws(ez.get(), &w);
		sim_frame(ms, ws);
		std::vector<int> rk;
		for (int i = 0; i < GetSize(w.latch_lit); i++) {
			int r = w.latch_reset[i];
			if (wl_map[i] >= 0 && (r == 0 || r == 1))
				rk.push_back(r ? ws.lit(w.latch_lit[i]) : ez->NOT(ws.lit(w.latch_lit[i])));
		}
		rk.push_back(ws.constraints());
		int cond = ez->OR(ez->NOT(ez->AND(ms.reset(), ms.constraints())), ez->expression(ezSAT::OpAnd, rk));
		if (!valid(ez.get(), cond, timeout_s, "reset", why))
			return false;
	}
	// transition: F_st ^ C_s ^ C_t ^ C'_s -> F'_st{K} ^ C'_t
	{
		ezSatPtr ez;
		Frame ms(ez.get(), &model), mt(ez.get(), &model), ws(ez.get(), &w), wt(ez.get(), &w);
		std::vector<int> f;
		for (int i = 0; i < GetSize(model.latch_lit); i++)
			f.push_back(ez->IFF(mt.lit(model.latch_lit[i]), ms.lit(model.latch_next[i])));
		sim_frame(ms, ws);
		sim_frame(mt, wt);
		std::vector<int> fk;
		for (int i = 0; i < GetSize(w.latch_lit); i++)
			if (wl_map[i] >= 0)
				fk.push_back(ez->IFF(wt.lit(w.latch_lit[i]), ws.lit(w.latch_next[i])));
		fk.push_back(wt.constraints());
		int ante = ez->AND(f.empty() ? ez->CONST_TRUE : ez->expression(ezSAT::OpAnd, f),
				ez->AND(ms.constraints(), mt.constraints(), ws.constraints()));
		int cond = ez->OR(ez->NOT(ante), ez->expression(ezSAT::OpAnd, fk));
		if (!valid(ez.get(), cond, timeout_s, "transition", why))
			return false;
	}
	// property: C_s ^ C'_s ^ P'_s -> P_s
	{
		ezSatPtr ez;
		Frame ms(ez.get(), &model), ws(ez.get(), &w);
		sim_frame(ms, ws);
		int cond = ez->OR(ez->NOT(ez->AND(ms.constraints(), ws.constraints(), ws.property())), ms.property());
		if (!valid(ez.get(), cond, timeout_s, "property", why))
			return false;
	}
	// base: R'_s{L'} ^ C'_s -> P'_s (the witness on its own)
	{
		ezSatPtr ez;
		Frame ws(ez.get(), &w);
		int cond = ez->OR(ez->NOT(ez->AND(ws.reset(), ws.constraints())), ws.property());
		if (!valid(ez.get(), cond, timeout_s, "base", why))
			return false;
	}
	// step: F'_st{L'} ^ C'_s ^ C'_t ^ P'_s -> P'_t
	{
		ezSatPtr ez;
		Frame ws(ez.get(), &w), wt(ez.get(), &w);
		std::vector<int> f;
		for (int i = 0; i < GetSize(w.latch_lit); i++)
			f.push_back(ez->IFF(wt.lit(w.latch_lit[i]), ws.lit(w.latch_next[i])));
		int ante = ez->AND(f.empty() ? ez->CONST_TRUE : ez->expression(ezSAT::OpAnd, f),
				ez->AND(ws.constraints(), wt.constraints(), ws.property()));
		int cond = ez->OR(ez->NOT(ante), wt.property());
		if (!valid(ez.get(), cond, timeout_s, "step", why))
			return false;
	}
	return true;
}

bool check_invariant(const Aig &model, const Aig &inv, double timeout_s, std::string &why, bool negate)
{
	if (GetSize(inv.outputs) != 1 || !inv.latch_lit.empty()) {
		why = stringf("invariant shape: %d outputs / %d latches", GetSize(inv.outputs), GetSize(inv.latch_lit));
		return false;
	}
	// invariant input k -> model latch: abc names them "pi<latch index>"
	// (inputs the invariant does not use are dropped); without names, by
	// position.  The mapping only decides WHICH formula is checked: any
	// formula over the model latches that passes the three checks below is
	// an inductive invariant of the model, however it was derived.
	std::vector<int> imap(inv.inputs.size(), -1);
	for (int i = 0; i < GetSize(inv.inputs); i++) {
		auto it = inv.symbols.find(stringf("i%d", i));
		if (it != inv.symbols.end() && it->second.compare(0, 2, "pi") == 0)
			imap[i] = atoi(it->second.c_str() + 2);
		else
			imap[i] = i;
		if (imap[i] < 0 || imap[i] >= GetSize(model.latch_lit)) {
			why = stringf("invariant input %d maps to latch %d of %d", i, imap[i], GetSize(model.latch_lit));
			return false;
		}
	}
	auto inv_at = [&](Frame &mf, Frame &iv) {
		for (int i = 0; i < GetSize(inv.inputs); i++)
			iv.set(inv.inputs[i], mf.lit(model.latch_lit[imap[i]]));
		return iv.lit(inv.outputs[0] ^ (negate ? 1 : 0));
	};
	{
		ezSatPtr ez;
		Frame ms(ez.get(), &model), is(ez.get(), &inv);
		int cond = ez->OR(ez->NOT(ez->AND(ms.reset(), ms.constraints())), inv_at(ms, is));
		if (!valid(ez.get(), cond, timeout_s, "invariant init", why))
			return false;
	}
	{
		ezSatPtr ez;
		Frame ms(ez.get(), &model), mt(ez.get(), &model), is(ez.get(), &inv), it(ez.get(), &inv);
		std::vector<int> f;
		for (int i = 0; i < GetSize(model.latch_lit); i++)
			f.push_back(ez->IFF(mt.lit(model.latch_lit[i]), ms.lit(model.latch_next[i])));
		int ante = ez->AND(f.empty() ? ez->CONST_TRUE : ez->expression(ezSAT::OpAnd, f),
				ez->AND(inv_at(ms, is), ms.constraints(), mt.constraints()));
		int cond = ez->OR(ez->NOT(ante), inv_at(mt, it));
		if (!valid(ez.get(), cond, timeout_s, "invariant step", why))
			return false;
	}
	{
		ezSatPtr ez;
		Frame ms(ez.get(), &model), is(ez.get(), &inv);
		int cond = ez->OR(ez->NOT(ez->AND(inv_at(ms, is), ms.constraints())), ms.property());
		if (!valid(ez.get(), cond, timeout_s, "invariant safety", why))
			return false;
	}
	return true;
}

}

YOSYS_NAMESPACE_END
