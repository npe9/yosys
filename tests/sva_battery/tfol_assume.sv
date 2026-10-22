// LRM 16.13.11's example `initial a1: assume property (@(posedge clk)
// reset[*5] #=# always !reset);` -- reset for the first 5 ticks, then 0
// for ever.  Before 2026-10-21 the assumption constrained nothing (the
// followed-by was an implication and the nested always one tick): r_on was
// DISPROVED at step 0.  (r_off, `t >= 5 |-> !reset`, holds -- BMC to 24 --
// but ic3 does not prove it within 120 s; not gated here.)
module tfol_assume (input logic clk, input logic reset);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  initial a1: assume property (@(posedge clk) reset[*5] #=# always !reset);
  r_on:  assert property (@(posedge clk) t <= 4 |-> reset);   // PROVED
  r_neg: assert property (@(posedge clk) t >= 6 |-> reset);   // DIS @6
endmodule
