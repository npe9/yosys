// Overlapping attempts must not lend each other matches (register v3 C1,
// LRM-example half, 2026-10-19; found when the transcribed Figure 16-7
// `not (te1 and te2)` fired at tick 13 off te2 at 12 and te1 at 13).
// Closed design: attempts start at back-to-back ticks; the earlier one
// matches, the later one cannot.  All of these were PROVED:
//   p_fixed   the |-> consequent's end window reached deadline ticks back,
//             so attempt 2's end at 3 excused attempt 3 (strong_seq_fail)
//   p_isect / p_within / p_and   the binary sequence operators paired one
//             attempt's operand end with another attempt's
// The ok_* twins have both attempts matchable and must stay PROVED.
module tattempts(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic a, b, x, y, y2, xw;
  assign a  = t == 2 || t == 3;
  assign b  = t == 2;
  assign x  = t == 3 || t == 4;
  assign y  = t == 4;
  assign y2 = t == 3 || t == 4;
  assign xw = t == 4 || t == 5;
  p_fixed:  assert property (@(posedge clk) (t == 2 || t == 3) |-> (b ##1 1)); // DIS @4 (LRM @3: b is low at 3; the engine reports at the consequent deadline, known class F5 / register D5)
  ok_fixed: assert property (@(posedge clk) (t == 2 || t == 3) |-> (a ##1 1)); // PROVED
  p_and:    assert property (@(posedge clk) (t == 2 || t == 3) |-> ((a ##1 1) and (b ##1 1))); // DIS @4 (LRM @3: b ##1 1 has no parse from 3; the engine reports at the consequent deadline, known class F5 / register D5)
  ok_and:   assert property (@(posedge clk) (t == 2 || t == 3) |-> ((a ##1 1) and a)); // PROVED
  p_isect:  assert property (@(posedge clk) (t == 3 || t == 4) |-> ((x ##1 1) intersect (y ##[0:1] 1))); // DIS @4 (the model too: x ##1 1 from 3 stays open until 4)
  ok_isect: assert property (@(posedge clk) (t == 3 || t == 4) |-> ((x ##1 1) intersect (y2 ##[0:1] 1))); // PROVED
  p_within: assert property (@(posedge clk) (t == 4 || t == 5) |-> ((t == 4) within (xw ##2 1))); // DIS @7
  ok_within: assert property (@(posedge clk) (t == 3 || t == 4) |-> ((t == 4) within (x ##2 1))); // PROVED
  // antecedent side: a cross-attempt pair would be a match that does not
  // exist (attempt 3 / 5 has no parse), the implication would fire
  a_and:    assert property (@(posedge clk) ((t == 2 || t == 3) ##0 ((a ##1 1) and (b ##1 1)) ##0 t == 4) |-> 1'b0); // PROVED
  a_isect:  assert property (@(posedge clk) ((t == 3 || t == 4) ##0 ((x ##1 1) intersect (y ##[0:1] 1)) ##0 t == 4) |-> 1'b0); // PROVED
  a_within: assert property (@(posedge clk) ((t == 4 || t == 5) ##0 ((t == 4) within (xw ##2 1)) ##0 t == 7) |-> 1'b0); // PROVED
  // the sequence `and` as a `not` operand (the Figure 16-7 shape)
  p_notand: assert property (@(posedge clk) t == 3 |-> not (a and b));  // PROVED
  b_notand: assert property (@(posedge clk) t == 2 |-> not (a and b));  // DIS @2
endmodule
