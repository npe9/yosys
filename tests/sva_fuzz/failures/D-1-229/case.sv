// sva_fuzz case seed=1 index=229
module top(input clk, input a, input b, input c);
  logic [2:0] cnt;
  always_ff @(posedge clk) cnt <= cnt + 1;
  p0: assert property (@(posedge clk) (first_match(((($past(a,1) throughout (b)) intersect c) ##3 ((cnt[2] within c) ##2 cnt[2][*2:3]))) |-> strong (b[*2:3])));
endmodule
