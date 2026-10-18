// Non-contiguous repetition ([=m:n]/[->m:n]) of a bounded sequence body:
// the counter machinery counts OCCURRENCES, where an occurrence pulse is
// the body's match end attempted at every tick (overlapping attempts
// merge, the documented approximation).  Antecedent use needs no
// deadline; in property position (e.g. an implication consequent) the
// end time is genuinely unbounded (gaps between occurrences), so the
// strong-completion deadline stays a clear error there.  Locals in the
// body are rejected clearly.
module tnoncons1 (input logic clk, input logic a, input logic b);
  p0: assert property (@(posedge clk) always (((a ##1 b)[->1]) |-> b)); // PROVED
  // p1/p3: [->]/[=] of a SEQUENCE is a fork extension (the LRM allows
  // Boolean operands only); it follows the Boolean rule b[=n] = b[->n] ##1
  // !b[*0:$], so the match also ends AT the n-th occurrence.  a high at
  // 0..2: a[*2] occurs at 1 and 2, [=2] ends at 2 with a high -> fails @2.
  // These read @4 / @3 before the [=] end fix (register v3 C4, 2026-10-18).
  p1: assert property (@(posedge clk) always (((a[*2])[=2]) |-> (!a))); // DIS @2
  p2: assert property (@(posedge clk) always (((a ##1 b)[->1]) |=> !a)); // DIS @2
  p3: assert property (@(posedge clk) always (((a ##2 b)[=1]) |-> (!b || !a))); // DIS @2
endmodule
