// IEEE 1800-2009 16.13.13 (eventually), the liveness examples (register v3
// C1, 2026-10-21):
//  p3 `always s_eventually a` ("a infinitely often") at the top of an
//     assertion: the assertion already attempts every tick, so it is
//     checked as `s_eventually a` (it was "unbounded s_eventually is
//     liveness").  Nested or in an initial block it stays loud.
//  p7 `s_eventually [m:$] a` as an implication's consequent: a temporal
//     response opened m ticks later (it was "give it a finite upper bound").
// Still loud: p2 `s_eventually always a` (eventual persistence).
// Liveness disproofs carry no step.  Trace: e @ 9 only, g from 8 on
// (t saturates at 15).
module tlrm_event (input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic e, g;
  assign e = t == 9;
  assign g = t >= 8;
  default clocking @(posedge clk); endclocking
  p3_ok:  assert property (always s_eventually g);               // PROVED
  p3_n:   assert property (always s_eventually e);               // DIS
  p7_ok:  assert property (t == 3 |-> s_eventually [2:$] e);     // PROVED
  p7_bd:  assert property (t == 7 |-> s_eventually [2:$] e);     // PROVED
  p7_n:   assert property (t == 8 |-> s_eventually [2:$] e);     // DIS
  p1_ok:  assert property (t == 3 |-> s_eventually e);           // PROVED
endmodule
