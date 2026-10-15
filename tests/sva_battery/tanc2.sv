// Anchored per-attempt counting for [=] inside ## chains (LRM F.3.4.2.3:
// b[=m] = b[->m] ##1 !b[*0:$]; the trailing !b run is blocked by any
// further b, so a 3rd b inside the window kills every exact match).
// ctr saturates at 7 for the same reason as tanc1 (see there).
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

  // b at 2,3,4: exact [->2] ends at ctr 3; the !b[*0:$] tail needs tick 4
  // to be !b, but b@4 blocks it -> no [=2] end at all -> vacuous.  The
  // old >= counter treated ctr 4 as an end of the [->2] part (3rd
  // occurrence, k >= 2) and matched [=2] from ctr 5 on.
  p_blk: assert property (@(posedge clk) a ##1 b[=2] |-> 1'b0);  // PROVED

  // positive control: b at 2,3,6; exact [->2] ends at ctr 3, idle 4.., so
  // [=2] ends at 4 with c at 5 completing -> match -> violation.
  p_ok:  assert property (@(posedge clk) a ##1 b3[=2] ##1 c |-> 1'b0);  // DIS @5
endmodule
