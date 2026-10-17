// sva_pbt case seed=3 index=50
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) disable iff ((!d)) (((!($past(c) || c))[->2] and (!(cnt[0] || b))) |=> ((($rose(b) || (c && c)) ##1 (!c)) |=> (((cnt == 2'd0) || b) || (!d)))));
endmodule
