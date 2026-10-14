// Delta-minimized repro for the signature in NOTES.txt (seed 11 index 10,
// `always (s_nexttime (d ##3 c))`).  Any nexttime/s_nexttime (bracketed or
// not) whose operand is a genuine sequence operator hits the same reject.
module top(input clk, input a, input b);
  p0: assert property (@(posedge clk) nexttime (a ##1 b));
endmodule
