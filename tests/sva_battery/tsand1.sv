// Regression for sequence `and` lowering (sva_fuzz class C): common-start
// union ending at the later operand end (LRM 16.7.3 style).  p3 covers the
// bare-sequence-in-property-position route (property-and of two strong
// completions, equivalent for bounded operands).
module tsand1 (input logic clk, input logic a, input logic b);
  p0: assert property (@(posedge clk) always ((a and (a ##1 a)))); // DIS @0
  p1: assert property (@(posedge clk) always ((a && b) |-> ((a[*1]) and (b[*1])))); // PROVED
  p2: assert property (@(posedge clk) always (a |-> ((a[*1]) and (b[*2])))); // DIS @1
  p3: assert property (@(posedge clk) always ((a[*2]) and (b[*2]))); // DIS @1
endmodule
