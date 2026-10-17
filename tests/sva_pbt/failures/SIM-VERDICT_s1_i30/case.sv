// sva_pbt case seed=1 index=30
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) disable iff (rst) (((c || cnt[0])[*1:2] and (a ##[3:5] $rose(a))) |=> (if ((b || a)) ((c || $stable(d)) |=> $past(a, 2)) else ((a && b) || $rose(a)))));
endmodule
