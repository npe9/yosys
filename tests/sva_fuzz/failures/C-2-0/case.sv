// sva_fuzz case seed=2 index=0
module top(input clk, input a, input b, input c);
  logic [1:0] cnt;
  always_ff @(posedge clk) cnt <= cnt + 1;
  p0: assert property (@(posedge clk) always ((b ##1 (((cnt[0] ##2 cnt[1]) and first_match(cnt[0])) ##1 a)) |-> ((((c throughout (b)) ##2 (b ##3 $stable(cnt[0]))) intersect b) ##3 cnt[1])));
endmodule
