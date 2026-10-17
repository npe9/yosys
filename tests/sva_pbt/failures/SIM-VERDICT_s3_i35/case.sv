// sva_pbt case seed=3 index=35
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) ((((!b) && (r && d))[=1] or (r || (b || a))) |=> (s_eventually[1:3] ((!($stable(a) && a)) within (r || $fell(c))))));
endmodule
