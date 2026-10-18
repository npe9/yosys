// Regression for nexttime/s_nexttime over sequence operands (sva_fuzz
// C-11-10): prop_fail's nexttime case re-dispatched the operand as a
// property, so an LRM-legal sequence operand (sequence_expr is a
// property_expr) errored as "unsupported property operator '##'.  The
// operand now routes through prop_dispatch (sequences complete strongly,
// mirroring the strong/weak cases).
module tnext1 (input logic clk, input logic a, input logic b);
  p0: assert property (@(posedge clk) nexttime (a ##1 b)); // DIS @2 (LRM @1: the weak sequence fails once a is low at step 1; engine reports at the sequence's end, known class F5 / register D5)
  p1: assert property (@(posedge clk) always (a |-> nexttime b)); // DIS @1
  p2: assert property (@(posedge clk) always (a |-> s_nexttime [2] b)); // DIS @2
  p3: assert property (@(posedge clk) always (nexttime (a[*2]))); // DIS @2
endmodule
