// IEEE 1800-2009 16.9.5-16.9.7, Figures 16-5, 16-6, 16-8, 16-10, 16-11
// (and / intersect / or of sequences), transcribed for the battery
// (register v3 C1, LRM-example half, 2026-10-19).  The trace is read off
// the figures (sampled values; the figures share it):
//   te1 @ 1 2 8        te2 @ 9..14         te3 @ 2 3 8 9
//   te4 @ 3..6 10..14  te5 @ 4 5 6 12..14
// (te3 @ 9 appears in Figures 16-5 and 16-10 only; every statement below
// is about the attempt at tick 8, which te3 @ 9 does not touch.)
// Step k of the engine is LRM tick k (t counts the ticks; tick 0 is a
// filler with every te low).  Every expectation restates a sentence of the
// LRM text; the line cites it.
module tlrm_seqops(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic te1, te2, te3, te4, te5;
  assign te1 = t == 1 || t == 2 || t == 8;
  assign te2 = t >= 9 && t <= 14;
  assign te3 = t == 2 || t == 3 || t == 8 || t == 9;
  assign te4 = (t >= 3 && t <= 6) || (t >= 10 && t <= 14);
  assign te5 = (t >= 4 && t <= 6) || (t >= 12 && t <= 14);
  // 16-5 "a match is recognized for the composite sequence at clock tick 12"
  f5_at12:  assert property (@(posedge clk) t == 8 |-> ((te1 ##2 te2) and (te3 ##2 te4 ##2 te5)) ##0 t == 12); // PROVED
  f5_neg:   assert property (@(posedge clk) t == 8 |-> not ((te1 ##2 te2) and (te3 ##2 te4 ##2 te5))); // DIS @12
  f5_only:  assert property (@(posedge clk) t == 8 |-> not (((te1 ##2 te2) and (te3 ##2 te4 ##2 te5)) ##0 t != 12)); // PROVED
  // 16-6 "five matches of the composite sequence, four of them ending at
  // clock tick 12, and the fifth ending at clock tick 13"
  f6_at12:  assert property (@(posedge clk) t == 8 |-> ((te1 ##[1:5] te2) and (te3 ##2 te4 ##2 te5)) ##0 t == 12); // PROVED
  f6_at13:  assert property (@(posedge clk) t == 8 |-> ((te1 ##[1:5] te2) and (te3 ##2 te4 ##2 te5)) ##0 t == 13); // PROVED
  f6_only:  assert property (@(posedge clk) t == 8 |-> not (((te1 ##[1:5] te2) and (te3 ##2 te4 ##2 te5)) ##0 (t != 12 && t != 13))); // PROVED
  // 16-8 "there is only a single match at clock tick 12"
  f8_at12:  assert property (@(posedge clk) t == 8 |-> ((te1 ##[1:5] te2) intersect (te3 ##2 te4 ##2 te5)) ##0 t == 12); // PROVED
  f8_only:  assert property (@(posedge clk) t == 8 |-> not (((te1 ##[1:5] te2) intersect (te3 ##2 te4 ##2 te5)) ##0 t != 12)); // PROVED
  f8_not13: assert property (@(posedge clk) t == 8 |-> ((te1 ##[1:5] te2) intersect (te3 ##2 te4 ##2 te5)) ##0 t == 13); // DIS @13 (LRM @12: the LRM states no tick, but the bad prefix is complete at 12, where the only intersect match ends -- te3 ##2 te4 ##2 te5 cannot end later; the engine reports when the te1 ##[1:5] te2 window closes, known class F5 / register D5)
  // 16-10 "The first sequence matches at clock tick 10, and the second
  // sequence matches at clock tick 12.  Therefore, two matches"
  f10_at10: assert property (@(posedge clk) t == 8 |-> ((te1 ##2 te2) or (te3 ##2 te4 ##2 te5)) ##0 t == 10); // PROVED
  f10_at12: assert property (@(posedge clk) t == 8 |-> ((te1 ##2 te2) or (te3 ##2 te4 ##2 te5)) ##0 t == 12); // PROVED
  f10_neg:  assert property (@(posedge clk) t == 8 |-> not ((te1 ##2 te2) or (te3 ##2 te4 ##2 te5))); // DIS @10
  f10_only: assert property (@(posedge clk) t == 8 |-> not (((te1 ##2 te2) or (te3 ##2 te4 ##2 te5)) ##0 (t != 10 && t != 12))); // PROVED
  // 16-11 "one match at each of clock ticks 9, 10, 11, and 13 and has two
  // matches at clock tick 12"
  f11_at9:  assert property (@(posedge clk) t == 8 |-> ((te1 ##[1:5] te2) or (te3 ##2 te4 ##2 te5)) ##0 t == 9); // PROVED
  f11_at13: assert property (@(posedge clk) t == 8 |-> ((te1 ##[1:5] te2) or (te3 ##2 te4 ##2 te5)) ##0 t == 13); // PROVED
  f11_only: assert property (@(posedge clk) t == 8 |-> not (((te1 ##[1:5] te2) or (te3 ##2 te4 ##2 te5)) ##0 (t < 9 || t > 13))); // PROVED
endmodule
