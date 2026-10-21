// Repetition of a parenthesized sequence inside a NAMED sequence
// declaration (2026-10-21).  sva2check aborted (uncaught dict::at) on
// `sequence s; (a ##1 b)[*2]; endsequence`: the repetition branch took its
// body cell from the inlined clone's port, a stub whose ports were never
// filled.  Found transcribing the 16.9.2 prose equivalences (tlrm_rep.sv).
// Trace: a at 1, 3, 5; b at 2, 4, 6.
module tseqrep_decl(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic a, b;
  assign a = t == 1 || t == 3 || t == 5;
  assign b = t == 2 || t == 4 || t == 6;
  sequence s2;  (a ##1 b) [*2]; endsequence
  sequence s13; (a ##1 b) [*1:3]; endsequence
  sequence sp;  (a ##1 b) [+]; endsequence
  sequence sc;  @(posedge clk) (1'b1 ##1 a) [*2]; endsequence
  r_two:   assert property (@(posedge clk) t == 1 |-> s2);        // PROVED (ends at 4)
  r_two3:  assert property (@(posedge clk) t == 3 |-> s2);        // PROVED (ends at 6)
  r_two5:  assert property (@(posedge clk) t == 5 |-> s2);        // DIS @8 (LRM @7: a is low at 7; the engine reports at the consequent deadline, known class F5 / register D5)
  r_trig:  assert property (@(posedge clk) t == 6 |-> s13.triggered); // PROVED (1..6)
  r_plus:  assert property (@(posedge clk) t == 6 |-> sp.triggered); // PROVED (1..6)
  r_plusb: assert property (@(posedge clk) t == 5 |-> sp.triggered); // DIS @5
  r_ant:   assert property (@(posedge clk) s2 |-> t == 4 || t == 6); // PROVED
  r_antb:  assert property (@(posedge clk) s2 |-> t == 4);        // DIS @6
  r_clk:   assert property (@(posedge clk) t == 0 |-> sc);        // PROVED (a at 1, 3)
  r_clkb:  assert property (@(posedge clk) t == 1 |-> sc);        // DIS @4 (LRM @2: a is low at 2; the engine reports at the consequent deadline, known class F5 / register D5)
endmodule
