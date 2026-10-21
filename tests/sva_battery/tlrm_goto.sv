// IEEE 1800-2009 16.9.2: the goto / nonconsecutive repetition
// equivalences stated in prose, transcribed for the battery (register v3
// C1, prose half, 2026-10-20).  Same scheme as tlrm_rep.sv: free a, b, c;
// one attempt at t == 1; L == R checked both ways by match ends of that
// attempt via .triggered of the other side.  The LRM's bounds are 2:10;
// a bounded repetition of a SEQUENCE is capped at 8 copies (loud
// sva2check limit), so the goto pair uses 2:8 (the equivalence holds for
// any bounds).  The nonconsecutive pair is gated at 2:3: at 2:8 ic3 does
// not decide n1_lr in 120 s (2:4 takes 53 s); 2:8 is in xlrm_rep.sv.
module tlrm_goto(input logic clk, input logic a, input logic b, input logic c);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic st;
  assign st = t == 4'd1;

  // "a ##1 b [->2:10] ##1 c ... is equivalent to a ##1 ((!b[*0:$] ##1 b)
  // [*2:10]) ##1 c"
  sequence g1l; @(posedge clk) st ##0 (a ##1 b [->2:8] ##1 c); endsequence
  sequence g1r; @(posedge clk) st ##0 (a ##1 ((!b [*0:$] ##1 b) [*2:8]) ##1 c); endsequence
  g1_lr: assert property (@(posedge clk) g1l |-> g1r.triggered); // PROVED
  g1_rl: assert property (@(posedge clk) g1r |-> g1l.triggered); // PROVED

  // "a ##1 b [=2:10] ##1 c ... is equivalent to a ##1 ((!b [*0:$] ##1 b)
  // [*2:10]) ##1 !b[*0:$] ##1 c"
  sequence n1l; @(posedge clk) st ##0 (a ##1 b [=2:3] ##1 c); endsequence
  sequence n1r; @(posedge clk) st ##0 (a ##1 ((!b [*0:$] ##1 b) [*2:3]) ##1 !b [*0:$] ##1 c); endsequence
  n1_lr: assert property (@(posedge clk) n1l |-> n1r.triggered); // PROVED
  n1_rl: assert property (@(posedge clk) n1r |-> n1l.triggered); // PROVED

  // goto ends AT the last b; nonconsecutive may extend over !b ticks:
  // every goto match is a nonconsecutive match, not conversely
  // (a@1 b@2 b@3 !b@4 c@5).
  sequence g2l; @(posedge clk) st ##0 (a ##1 b [->2] ##1 c); endsequence
  sequence n2l; @(posedge clk) st ##0 (a ##1 b [=2] ##1 c); endsequence
  g2_sub: assert property (@(posedge clk) g2l |-> n2l.triggered); // PROVED
  c2_sup: assert property (@(posedge clk) n2l |-> g2l.triggered); // DIS @5
endmodule
