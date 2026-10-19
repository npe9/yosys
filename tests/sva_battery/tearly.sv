// sva2check -early: a sequence failure is reported at the tick no thread
// of the attempt survives -- the LRM's tick -- instead of at the
// sequence's deadline (register v3 D5; the deadline form is one or more
// ticks late, PBT class F5).  Default for assume/restrict (tassume_early);
// opt-in for asserts, which keep their verdicts either way.  Closed
// design: t counts ticks.
// SVA2CHECK-FLAGS: -early
module tearly(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic b, frame, irdy, g, x;
  assign b     = t == 2;
  assign frame = t >= 7;                 // LRM Fig 16-16: rises at 7
  assign irdy  = t <= 3 || t >= 8;       //                rises at 8
  assign g     = t != 4;
  assign x     = t >= 1;
  // back-to-back attempts; the one at 3 has b low (deadline form: @4)
  e_fixed: assert property (@(posedge clk) (t == 2 || t == 3) |-> (b ##1 1)); // DIS @3
  // data_end_rule2's attempt at 1: no $rose(frame) at 2 or 3 (deadline: @4)
  e_range: assert property (@(posedge clk) t == 1 |-> ##[1:2] $rose(frame) ##1 $rose(irdy)); // DIS @3
  e_ok:    assert property (@(posedge clk) t == 5 |-> ##[1:2] $rose(frame) ##1 $rose(irdy)); // PROVED
  // a throughout window cut by g low at 4 (deadline: @5)
  e_thr:   assert property (@(posedge clk) t == 2 |-> (g throughout (1 ##3 1))); // DIS @4
  // a sequence property whose first element fails at the attempt's own
  // start tick (deadline: @1)
  e_seq:   assert property (@(posedge clk) x ##1 1); // DIS @0
endmodule
