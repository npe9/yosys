// IEEE 1800-2009 16.9.2 (repetition in sequences): the prose-only
// equivalences, transcribed for the battery (register v3 C1, prose half,
// 2026-10-20).  a, b, c are free inputs; one attempt starts at t == 1
// (st).  For each LRM equivalence L == R, both directions are checked as
// "every match end of the attempt on one side is a match end of the same
// attempt on the other": (st ##0 L) |-> sR.triggered, where sR is the
// named sequence st ##0 R (.triggered: some match of sR ends now; only the
// attempt at t == 1 can match).  The c_* twins are near-misses the LRM
// text rules out; each fails at its shortest witness.
module tlrm_rep(input logic clk, input logic a, input logic b, input logic c);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic st;
  assign st = t == 4'd1;

  // "a ##1 b ##1 b ##1 b ##1 c ... specified more succinctly: a ##1 b [*3] ##1 c"
  sequence e1l; @(posedge clk) st ##0 (a ##1 b ##1 b ##1 b ##1 c); endsequence
  sequence e1r; @(posedge clk) st ##0 (a ##1 b [*3] ##1 c); endsequence
  e1_lr: assert property (@(posedge clk) e1l |-> e1r.triggered); // PROVED
  e1_rl: assert property (@(posedge clk) e1r |-> e1l.triggered); // PROVED

  // "b ##1 ( a[*0] ##0 c) produces no match of the sequence"
  e2_none:  assert property (@(posedge clk) st ##0 (b ##1 (a[*0] ##0 c)) |-> 1'b0); // PROVED
  // "(seq ##0 empty) does not result in a match"
  e2_none2: assert property (@(posedge clk) st ##0 (b ##0 a[*0]) |-> 1'b0); // PROVED
  c2_some:  assert property (@(posedge clk) st ##0 (b ##1 c) |-> 1'b0); // DIS @2

  // "b ##1 a[*0:1] ##2 c is equivalent to (b ##2 c) or (b ##1 a ##2 c)"
  sequence e3l; @(posedge clk) st ##0 (b ##1 a[*0:1] ##2 c); endsequence
  sequence e3r; @(posedge clk) st ##0 ((b ##2 c) or (b ##1 a ##2 c)); endsequence
  sequence c3r; @(posedge clk) st ##0 (b ##2 c); endsequence
  e3_lr: assert property (@(posedge clk) e3l |-> e3r.triggered); // PROVED
  e3_rl: assert property (@(posedge clk) e3r |-> e3l.triggered); // PROVED
  c3_lr: assert property (@(posedge clk) e3l |-> c3r.triggered); // DIS @4

  // "`true ##3 (a [*3])  // means `true ##1 `true ##1 `true ##1 a ##1 a ##1 a"
  sequence e4l; @(posedge clk) st ##0 (1'b1 ##3 (a [*3])); endsequence
  sequence e4r; @(posedge clk) st ##0 (1'b1 ##1 1'b1 ##1 1'b1 ##1 a ##1 a ##1 a); endsequence
  e4_lr: assert property (@(posedge clk) e4l |-> e4r.triggered); // PROVED
  e4_rl: assert property (@(posedge clk) e4r |-> e4l.triggered); // PROVED

  // "(`true ##2 a) [*3] // means `true ##1 `true ##1 a ##1 `true ##1 `true
  //  ##1 a ##1 `true ##1 `true ##1 a"
  sequence e5l; @(posedge clk) st ##0 ((1'b1 ##2 a) [*3]); endsequence
  sequence e5r; @(posedge clk) st ##0 (1'b1 ##1 1'b1 ##1 a ##1 1'b1 ##1 1'b1 ##1 a ##1 1'b1 ##1 1'b1 ##1 a); endsequence
  e5_lr: assert property (@(posedge clk) e5l |-> e5r.triggered); // PROVED
  e5_rl: assert property (@(posedge clk) e5r |-> e5l.triggered); // PROVED

  // "(a ##2 b)[*1:5] is equivalent to (a ##2 b) or (a ##2 b ##1 a ##2 b) or ..."
  // Gated at [*1:3] (ic3 decides it at once; [*1:4] and the LRM's [*1:5]
  // time out at 120 s).  The LRM's [*1:5] is in xlrm_rep.sv: one attempt,
  // every match ends by tick 15, so its BMC leg to step 24 is exhaustive.
  sequence e6l; @(posedge clk) st ##0 ((a ##2 b) [*1:3]); endsequence
  sequence e6r; @(posedge clk) st ##0 ((a ##2 b)
      or (a ##2 b ##1 a ##2 b)
      or (a ##2 b ##1 a ##2 b ##1 a ##2 b)); endsequence
  e6_lr: assert property (@(posedge clk) e6l |-> e6r.triggered); // PROVED
  e6_rl: assert property (@(posedge clk) e6r |-> e6l.triggered); // PROVED

  // "(a[*0:3] ##1 b ##1 c) is equivalent to (b ##1 c) or (a ##1 b ##1 c) or ..."
  sequence e7l; @(posedge clk) st ##0 (a[*0:3] ##1 b ##1 c); endsequence
  sequence e7r; @(posedge clk) st ##0 ((b ##1 c)
      or (a ##1 b ##1 c)
      or (a ##1 a ##1 b ##1 c)
      or (a ##1 a ##1 a ##1 b ##1 c)); endsequence
  e7_lr: assert property (@(posedge clk) e7l |-> e7r.triggered); // PROVED
  e7_rl: assert property (@(posedge clk) e7r |-> e7l.triggered); // PROVED

  // "[+] is an equivalent representation of [*1:$]"; "a ##1 b [*1:$] ##1 c
  // matches ... a on the first tick, c on the last, b at every tick strictly
  // in between" (three or more ticks: b ##1 b[*0:$])
  sequence e8l; @(posedge clk) st ##0 (a ##1 b [*1:$] ##1 c); endsequence
  sequence e8r; @(posedge clk) st ##0 (a ##1 b [+] ##1 c); endsequence
  sequence e8s; @(posedge clk) st ##0 (a ##1 b ##1 b [*] ##1 c); endsequence
  sequence c8l; @(posedge clk) st ##0 (a ##1 b [*0:$] ##1 c); endsequence
  e8_lr: assert property (@(posedge clk) e8l |-> e8r.triggered); // PROVED
  e8_rl: assert property (@(posedge clk) e8r |-> e8l.triggered); // PROVED
  e8_ls: assert property (@(posedge clk) e8l |-> e8s.triggered); // PROVED
  e8_sl: assert property (@(posedge clk) e8s |-> e8l.triggered); // PROVED
  c8_lr: assert property (@(posedge clk) c8l |-> e8l.triggered); // DIS @2

  // "(empty ##n seq), where n is greater than 0, is equivalent to (##(n-1) seq)"
  sequence e9l; @(posedge clk) st ##0 (a[*0] ##2 b); endsequence
  sequence e9r; @(posedge clk) st ##0 (##1 b); endsequence
  e9_lr: assert property (@(posedge clk) e9l |-> e9r.triggered); // PROVED
  e9_rl: assert property (@(posedge clk) e9r |-> e9l.triggered); // PROVED
  // "(seq ##n empty), where n is greater than 0, is equivalent to (seq ##(n-1) `true)"
  sequence e10l; @(posedge clk) st ##0 (b ##2 a[*0]); endsequence
  sequence e10r; @(posedge clk) st ##0 (b ##1 1'b1); endsequence
  e10_lr: assert property (@(posedge clk) e10l |-> e10r.triggered); // PROVED
  e10_rl: assert property (@(posedge clk) e10r |-> e10l.triggered); // PROVED
endmodule
