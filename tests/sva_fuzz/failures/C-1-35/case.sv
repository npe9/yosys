// sva_fuzz case seed=1 index=35
module top(input clk, input a, input b, input c);
  logic [1:0] cnt;
  always_ff @(posedge clk)
    if (a) cnt <= cnt + 1;
  p0: assert property (@(posedge clk) (((b and (b ##1 $past(b,1))) ##3 (((1'b1 ##3 $rose(cnt[1])) ##2 a[*4]) ##1 ($fell(cnt[0])[*3:4] or cnt[0]))) |=> (if (($rose(c) -> a)) ((b ##3 c) |-> cnt[1]) else (b |-> cnt[0]))));
endmodule
