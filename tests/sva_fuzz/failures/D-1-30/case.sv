// sva_fuzz case seed=1 index=30
module top(input clk, input a, input b, input c);
  logic [2:0] cnt;
  always_ff @(posedge clk) cnt <= cnt + 1;
  p0: assert property (@(posedge clk) (((b throughout ((($fell(b) || $rose(cnt[2])) throughout (($stable(cnt[1]) ##3 b))))) ##3 b) |-> ((((b && c) throughout (cnt[2])) ##2 $past(cnt[1],1)) |-> s_always[2:3] (((c ##2 cnt[2]) |-> (if ((cnt[2] -> b)) cnt[0] else cnt[2]))))));
endmodule
