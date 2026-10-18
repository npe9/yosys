// Anchored per-attempt counting for [=] inside ## chains (LRM F.3.4.2.3:
// b[=m] = b[->m] ##1 !b[*0:$]; the trailing !b run is blocked by any
// further b, so a 3rd b inside the window kills every exact match).
// ctr saturates at 7 for the same reason as tanc1 (see there).
// IC3-CTG: 1  (the p_blk vacuity proof is counter/rotation-heavy: CTG
// generalization closes it in ~1s vs ~15s plain, measured 2026-10-15)
module tanc2 (input logic clk);
  logic [2:0] ctr = 0;
  always_ff @(posedge clk)
    if (ctr != 3'd7)
      ctr <= ctr + 3'd1;
  logic a, b, b3, c;
  assign a  = (ctr == 3'd1);
  assign b  = (ctr == 3'd2) || (ctr == 3'd3) || (ctr == 3'd4);
  assign b3 = (ctr == 3'd2) || (ctr == 3'd3) || (ctr == 3'd6);
  assign c  = (ctr == 3'd5);

  // b at 2,3,4: exact [->2] ends at ctr 3, and b[=2] = b[->2] ##1
  // !b[*0:$] ends THERE too (the [*0] case, LRM 16.9.2); b@4 only cuts the
  // later ends.  So the antecedent matches at ctr 3 and the property
  // fails.  This expectation used to read PROVED, written against the
  // monitor that required a !b tick after the last occurrence; the C4
  // oracle (oracle.py, the independent ref_sva model) refuted it
  // (2026-10-18) and ic3 had been proving a false property.
  p_blk: assert property (@(posedge clk) a ##1 b[=2] |-> 1'b0);  // DIS @3

  // positive control: b at 2,3,6; exact [->2] ends at ctr 3, idle 4.., so
  // [=2] ends at 4 with c at 5 completing -> match -> violation.
  p_ok:  assert property (@(posedge clk) a ##1 b3[=2] ##1 c |-> 1'b0);  // DIS @5
endmodule
