// sva_pbt case seed=1 index=22
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) ((s_eventually[0:2] (if ($rose(b)) $rose(c) else (((cnt == 2'd2) || cnt[0]) && ((cnt == 2'd1) || r)))) or ($past(c)[=2] |=> cnt[0])));
endmodule
