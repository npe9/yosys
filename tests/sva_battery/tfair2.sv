// Fairness + safety interplay.  The $fair assumption (assume property
// (s_eventually ...)) has no SMT-LIB encoding: write_smt2 skips it with a
// warning, so the BMC leg checks the safety properties over a LARGER
// environment (all paths, fair or not).  A BMC PASS is therefore sound; a
// BMC CEX on such a design may be an unfair (spurious) path and needs the
// ic3 leg for a verdict.  tfair2 picks safety properties whose cex paths
// are fair anyway, so both legs agree step for step.
module tfair2 (input logic clk, input logic tick);
  logic [2:0] c = 0;
  always_ff @(posedge clk)
    if (tick) c <= c + 1;

  // fairness: tick fires infinitely often (drops the all-stall path from
  // the ic3 semantics; invisible to the BMC leg)
  assume property (@(posedge clk) s_eventually tick);

  // 1: c wraps mod 8: after a state with c==7 the next state is 0
  //    (ticked) or 7 (stalled) -- holds on every path, fair or not.
  s_wrap: assert property (@(posedge clk) c == 3'd7 |-> ##1 (c == 3'd0 || c == 3'd7));  // PROVED

  // 2: c[0] never set -- violated as soon as tick fires once (c==1); the
  //    cex path tick=1,1,1,... is fair, so both legs DIS at the same step.
  s_mod2: assert property (@(posedge clk) c[0] == 1'b0);  // DIS @1

  // 3: liveness under fairness: c[0]==1 at every odd value, visited
  //    infinitely often on any fair path -- false on the all-stall path,
  //    so this is ic3-only (write_smt2 skips the liveness leg).
  s_prog: assert property (@(posedge clk) s_eventually (c[0] == 1'b1));  // PROVED
endmodule
