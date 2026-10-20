// Property operators whose operands resolve at different ticks (LRM
// 16.12.4; PBT classes F6 / F10).  The operands' fail pulses are merged
// over attempts; pairing them at the same tick combined DIFFERENT
// attempts.  With fixed failure offsets the earlier operand is delayed
// onto the later one (register v3 F10).  Closed design.
module tpalign(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic a, b;
  assign a = t == 3;
  assign b = t == 6;
  // or: the attempt at 3 fails on the left at 3 and on the right (offset
  // 2) at 5 -> the or fails at 5; the twin's right side holds there
  o_late: assert property (@(posedge clk) (t != 3) or (a |-> ##2 0)); // DIS @5
  o_ok:   assert property (@(posedge clk) (t != 3) or (a |-> ##2 (t == 5))); // PROVED
  // the attempt at 4: left fails at 4 (t==4), right: nexttime[2] at 6 sees b
  o_bad: assert property (@(posedge clk) (t != 4) or (nexttime[2] !b)); // DIS @6
  // implies: left holds at 3 (offset 0), right fails at 5 for that attempt
  i_bad: assert property (@(posedge clk) (t == 3) implies (nexttime[2] (t != 5))); // DIS @5
  // implies whose consequent fails only for OTHER attempts' antecedents
  i_ok:  assert property (@(posedge clk) (t == 3) implies (nexttime[2] (t == 5))); // PROVED
  // iff: both sides at different offsets
  f_ok:  assert property (@(posedge clk) (t == 2) iff (nexttime[1] (t == 3))); // PROVED
  f_bad: assert property (@(posedge clk) (t == 2) iff (nexttime[1] (t == 4))); // DIS @3
  // s_eventually over a temporal operand (offset 1): window [1:2] after 2
  // holds at 3 or 4 iff (x |=> b): at 5 |=> 6 sees b
  e_ok:  assert property (@(posedge clk) t == 3 |-> s_eventually [1:2] (1 |=> (t == 5 || t == 6))); // PROVED
  e_bad: assert property (@(posedge clk) t == 3 |-> s_eventually [1:2] (1 |=> t == 9)); // DIS @6
endmodule
