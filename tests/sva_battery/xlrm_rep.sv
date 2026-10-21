// Stress (manual reading, not gated): the 16.9.2 prose equivalences at
// the LRM's own sizes, too slow for the gated ic3 leg (see tlrm_rep.sv /
// tlrm_goto.sv, which gate them at smaller bounds).  Same scheme: free
// a, b, c; one attempt at t == 1; both directions via .triggered.
//   e6 (a ##2 b)[*1:5]: every match ends by tick 15, so the BMC leg to
//      step 24 is exhaustive; ic3 does not finish in 120 s.
//   n1 b[=2:8] (2:10 in the LRM; sequence repetition is capped at 8):
//      unbounded (!b[*0:$]); ic3 decided n1_rl in 89 s, n1_lr not in 120 s.
module xlrm_rep(input logic clk, input logic a, input logic b, input logic c);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic st;
  assign st = t == 4'd1;
  sequence e6l; @(posedge clk) st ##0 ((a ##2 b) [*1:5]); endsequence
  sequence e6r; @(posedge clk) st ##0 ((a ##2 b)
      or (a ##2 b ##1 a ##2 b)
      or (a ##2 b ##1 a ##2 b ##1 a ##2 b)
      or (a ##2 b ##1 a ##2 b ##1 a ##2 b ##1 a ##2 b)
      or (a ##2 b ##1 a ##2 b ##1 a ##2 b ##1 a ##2 b ##1 a ##2 b)); endsequence
  e6_lr: assert property (@(posedge clk) e6l |-> e6r.triggered); // PROVED
  e6_rl: assert property (@(posedge clk) e6r |-> e6l.triggered); // PROVED
  sequence n1l; @(posedge clk) st ##0 (a ##1 b [=2:8] ##1 c); endsequence
  sequence n1r; @(posedge clk) st ##0 (a ##1 ((!b [*0:$] ##1 b) [*2:8]) ##1 !b [*0:$] ##1 c); endsequence
  n1_lr: assert property (@(posedge clk) n1l |-> n1r.triggered); // PROVED
  n1_rl: assert property (@(posedge clk) n1r |-> n1l.triggered); // PROVED
endmodule
