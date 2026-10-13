// sva_fuzz case seed=1 index=143
module top(input clk, input a, input b, input c, input d);
  logic [1:0] cnt;
  always_ff @(posedge clk) cnt <= cnt + 1;
  p0: assert property (@(posedge clk) (((($rose(d) throughout ((cnt[0] ##2 a))) ##1 ((cnt[0] ##3 a) intersect cnt[0][*3])) or b) |-> ((cnt[1] throughout ((1'b0 throughout ((d ##2 cnt[1]))))) |-> a)));
endmodule
