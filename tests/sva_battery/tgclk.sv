// `global clocking` (IEEE 1800-2017 14.14; 2026-10-21): `@($global_clock)` in
// a concurrent assertion is the global clocking event.  It used to become an
// undriven implicit `$global_clock` net (marked sv_degraded) clocking the
// property.  The global clocking event here is posedge clk, so g_* must give
// the same verdicts as the explicit c_* twins.  Trace: a @ 2..4.
module tgclk (input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic a;
  assign a = t >= 2 && t <= 4;
  global clocking gck @(posedge clk); endclocking
  g_ok: assert property (@($global_clock) t == 2 |-> a ##1 a ##1 a);  // PROVED
  g_n:  assert property (@($global_clock) t == 2 |-> a ##3 a);        // DIS @5
  c_ok: assert property (@(posedge clk) t == 2 |-> a ##1 a ##1 a);    // PROVED
  c_n:  assert property (@(posedge clk) t == 2 |-> a ##3 a);          // DIS @5
endmodule
