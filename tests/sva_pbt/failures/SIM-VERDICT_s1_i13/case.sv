// sva_pbt case seed=1 index=13
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) (((a ##[1:2] $stable(c)) intersect $stable(b)[=2]) |=> (if ((r || ((cnt == 2'd1) && r))) ($fell(a) || (!$rose(d))) else $rose(d))));
endmodule
