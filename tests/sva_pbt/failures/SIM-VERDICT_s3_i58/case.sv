// sva_pbt case seed=3 index=58
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) disable iff ($stable(b)) (((b && ((cnt == 2'd2) && r)) ##2 ($past(d, 2) ##1 ((cnt == 2'd2) && $past(a, 2)))) |=> ((cnt == 2'd3)[->1] |=> $stable(b))));
endmodule
