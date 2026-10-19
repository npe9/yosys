// IEEE 1800-2009 16.9.5 Figure 16-7 (`te1 and te2` of Boolean operands)
// and 16.9.7 Figure 16-9 (`te1 or te2`), transcribed for the battery
// (register v3 C1, LRM-example half, 2026-10-19).  The two figures have
// different traces; a1/a2 are Figure 16-7's te1/te2, o1/o2 Figure 16-9's.
// Read off the figures (sampled values):
//   a1 @ 1 3 8 13 14        a2 @ 1..6 8..12 14
//   o1 @ 1 3 8 11 12 14     o2 @ 1..6 8 9 10 14
// Step k is LRM tick k (tick 0 is a filler, every signal low).
// `not (a1 and a2)` was refused ("'not' of a temporal property") until
// sva2check read an and/or of sequence operands as the sequence it is;
// the *_not* twins check that reading fires where the sequence matches.
module tlrm_bool(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic a1, a2, o1, o2;
  assign a1 = t == 1 || t == 3 || t == 8 || t == 13 || t == 14;
  assign a2 = (t >= 1 && t <= 6) || (t >= 8 && t <= 12) || t == 14;
  assign o1 = t == 1 || t == 3 || t == 8 || t == 11 || t == 12 || t == 14;
  assign o2 = (t >= 1 && t <= 6) || (t >= 8 && t <= 10) || t == 14;
  // 16-7 "The sequence matches at clock tick 1, 3, 8, and 14 ... At all
  // other clock ticks, match of the AND operation fails"
  f7_match: assert property (@(posedge clk) (t == 1 || t == 3 || t == 8 || t == 14) |-> (a1 and a2)); // PROVED
  f7_fail:  assert property (@(posedge clk) (t >= 1 && t <= 14 && !(t == 1 || t == 3 || t == 8 || t == 14)) |-> not (a1 and a2)); // PROVED
  f7_at2:   assert property (@(posedge clk) t == 2 |-> (a1 and a2)); // DIS @2
  f7_not1:  assert property (@(posedge clk) t == 1 |-> not (a1 and a2)); // DIS @1
  // 16-9 "The composite sequence does not match at clock ticks 7 and 13
  // ... At all other clock ticks, the composite sequence matches"
  f9_match: assert property (@(posedge clk) (t >= 1 && t <= 14 && t != 7 && t != 13) |-> (o1 or o2)); // PROVED
  f9_fail:  assert property (@(posedge clk) (t == 7 || t == 13) |-> not (o1 or o2)); // PROVED
  f9_at7:   assert property (@(posedge clk) (t == 7 || t == 13) |-> (o1 or o2)); // DIS @7
  f9_not8:  assert property (@(posedge clk) t == 8 |-> not (o1 or o2)); // DIS @8
endmodule
