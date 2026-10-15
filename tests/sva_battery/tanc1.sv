// Anchored per-attempt counting for [->] inside ## chains (LRM F.3.4.2.3:
// b[->m] = (!b[*0:$] ##1 b)[*m], so the element ends ONLY at the m-th
// occurrence in the attempt's window; a later occurrence is a different
// parse, not a match).  The old merged-existential counter checked
// k >= m only, so a [->2] element also "ended" at the 3rd b, letting the
// chain continue from a tick where the exact element has no match.
// ctr SATURATES at 7 so the monitors quiesce after the pulse schedule
// (a free-running 8-bit ctr makes the exact-semantics proofs correlate
// two counters over a 256-periodic orbit, which bit-level IC3 closes
// very slowly -- a solver-stress shape, not a semantics test).
module tanc1 (input logic clk);
  logic [2:0] ctr = 0;
  always_ff @(posedge clk)
    if (ctr != 3'd7)
      ctr <= ctr + 3'd1;
  logic a, b, c1, c2;
  assign a  = (ctr == 3'd1);
  assign b  = (ctr == 3'd2) || (ctr == 3'd3) || (ctr == 3'd5);
  assign c1 = (ctr == 3'd4);   // follows the exact end (2nd b at ctr 3)
  assign c2 = (ctr == 3'd6);   // follows only the spurious 3rd-b "end"

  // exact: [->2] ends at ctr 3 only; c1 at 4 completes the chain, so the
  // implication fires and 1'b0 fails.  (Old >= semantics agreed here.)
  p_end2: assert property (@(posedge clk) a ##1 b[->2] ##1 c1 |-> 1'b0);  // DIS @4

  // exact: no chain match anywhere (the only exact end, ctr 3, is not
  // followed by c2) -> vacuous.  Old >= semantics: the 3rd b at ctr 5 is
  // also an "end" with c2 at 6 -> spurious violation.
  p_end3: assert property (@(posedge clk) a ##1 b[->2] ##1 c2 |-> 1'b0);  // PROVED
endmodule
