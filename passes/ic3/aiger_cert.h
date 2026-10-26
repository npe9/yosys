/*
 *  In-tree checker for safety certificates of external model checkers
 *  (register E0 / E7).  An external "proved" counts only after its
 *  certificate passes here, so a proof never rests on the engine's word.
 */

#ifndef IC3_AIGER_CERT_H
#define IC3_AIGER_CERT_H

#include "kernel/yosys.h"

YOSYS_NAMESPACE_BEGIN

namespace ic3_cert {

// An AIGER 1.9 circuit (ascii "aag" or binary "aig").  Latch resets are
// 0, 1 or the latch itself (uninitialized); anything else is rejected.
struct Aig {
	int maxvar = 0;
	std::vector<int> inputs;		// literals
	std::vector<int> latch_lit, latch_next, latch_reset;
	std::vector<int> outputs, bads, constraints;
	std::vector<std::array<int, 3>> ands;	// lhs, rhs0, rhs1
	// symbol table: "i<k>" / "l<k>" / "o<k>" / "b<k>" -> name
	dict<std::string, std::string> symbols;

	bool parse(const std::string &file, std::string &err);
	// the safety property: every bad (or, with no bad section, every
	// output) is 0
	const std::vector<int> &bad_lits() const { return bads.empty() ? outputs : bads; }
};

// Certifaiger witness-circuit check (Yu, Froleyks, Biere et al.;
// github.com/Froleyks/certifaiger), safety part, with K = the witness
// inputs/latches mapped by "= <model literal>" symbols:
//   reset       R_s ^ C_s                     -> R'_s{K} ^ C'_s
//   transition  F_st ^ C_s ^ C_t ^ C'_s       -> F'_st{K} ^ C'_t
//   property    C_s ^ C'_s ^ P'_s             -> P_s
//   base        R'_s{L'} ^ C'_s               -> P'_s
//   step        F'_st{L'} ^ C'_s ^ C'_t ^ P'_s -> P'_t
// (R_s / F_st are the model's full reset and transition: every real model
// state and step, which is what the shared-element conditions quantify
// over.)  Returns true iff all five hold; `why` names the failing check.
bool check_witness(const Aig &model, const Aig &witness, double timeout_s, std::string &why);

// An inductive invariant over the model's latches, given as a
// combinational circuit whose inputs are model latches (named "pi<k>" by
// abc's inv_get, else in order) and whose single output is the invariant:
//   init   R_s ^ C_s           -> I_s
//   step   I_s ^ C_s ^ F_st ^ C_t -> I_t
//   safe   I_s ^ C_s           -> P_s
// negate: the circuit's output is the complement of the invariant (abc's
// inv_get writes the network that is 1 where the invariant is violated)
bool check_invariant(const Aig &model, const Aig &inv, double timeout_s, std::string &why, bool negate = false);

// A native ic3 proof (register E7): the inductive frame as CNF over model
// latches (clause literal +(l+1): latch l is 1, -(l+1): latch l is 0) and
// one bad literal of the model.  The native frames block PREDECESSORS of
// bad states, not bad states themselves, so safety is checked one step
// shifted:
//   init    R_s ^ C_s                -> I_s
//   base    R_s ^ C_s                -> !B_s
//   step    I_s ^ C_s ^ F_st ^ C_t   -> I_t
//   safety  I_s ^ C_s ^ F_st ^ C_t   -> !B_t
// Every reachable state is in I (init, step), B fails in the initial
// state (base) and in every state with a predecessor (safety).
bool check_clauses(const Aig &model, int bad, const std::vector<std::vector<int>> &clauses, double timeout_s, std::string &why);

// A native k-induction proof at depth k, re-checked on the model (the
// certificate is k itself), with constraints on the steps up to and
// including the failing one:
//   base  R_0 ^ C_0..C_d -> !B_d                  for d = 0..k-1
//   step  C_0..C_k ^ !B_0..!B_{k-1} -> !B_k
bool check_kinduction(const Aig &model, int bad, int k, double timeout_s, std::string &why);

}

YOSYS_NAMESPACE_END

#endif
