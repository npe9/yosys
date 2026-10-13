// Crash/gap regression (sva_fuzz C-1-176): a constant property body was
// skipped as "malformed spec", leaving the x-reading frontend placeholder
// (fired spuriously in sim).  Constant bodies now lower directly to the
// $check A port (the "property holds" condition: assert bad := EN && !A).
module tconst1 (input logic clk, input logic a);
  p0: assert property (@(posedge clk) 1'b1); // PROVED
  p1: assert property (@(posedge clk) 1'b0); // DIS @0
  p2: assert property (@(posedge clk) always (a |-> 1'b1)); // PROVED
  p3: assert property (@(posedge clk) always (a |=> 1'b0)); // DIS @1
endmodule
