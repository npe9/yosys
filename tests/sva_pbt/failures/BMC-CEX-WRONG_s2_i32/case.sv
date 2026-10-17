// sva_pbt case seed=2 index=32
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) disable iff (rst) (((($fell(c) && a) ##1 a) |-> (s_eventually[2:3] (d || (d || $fell(d))))) or ((d || c) || a)));
endmodule
