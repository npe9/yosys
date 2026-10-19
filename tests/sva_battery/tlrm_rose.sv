// IEEE 1800-2009 16.9.3 Figure 16-3 (value change expressions), transcribed
// for the battery (register v3 C1, LRM-example half, 2026-10-19).
//   e1 = $rose(req), e2 = $fell(ack)
// Sampled values read off the figure (an edge on tick k's line is the
// value sampled at k):  req low @1 2, high @3..8, low @9..;  ack high
// @1..5, low @6..  Step k is LRM tick k; tick 0 repeats tick 1's values.
module tlrm_rose(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic req, ack;
  assign req = t >= 3 && t <= 8;
  assign ack = t <= 5;
  // "At clock tick 3, e1 occurs because the value of req at clock tick 2
  // was low and the value at clock tick 3 is high."  The figure shows no
  // other occurrence of e1.
  e1_at3:   assert property (@(posedge clk) t == 3 |-> $rose(req)); // PROVED
  e1_only:  assert property (@(posedge clk) $rose(req) |-> t == 3); // PROVED
  // "e2 occurs at clock tick 6 because the value of ack was sampled as
  // high at clock tick 5 and sampled as low at clock tick 6."
  e2_at6:   assert property (@(posedge clk) t == 6 |-> $fell(ack)); // PROVED
  e2_only:  assert property (@(posedge clk) $fell(ack) |-> t == 6); // PROVED
  e1_not4:  assert property (@(posedge clk) t == 4 |-> $rose(req)); // DIS @4
endmodule
