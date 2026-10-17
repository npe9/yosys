// sva_pbt case seed=1 index=34
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) ((if ((!(cnt == 2'd1))) (always[0:2] (c || (r && $stable(a)))) else ((b || $rose(d)) |-> (!(d || d)))) implies ((d || b) && ($stable(c) || d))));
endmodule
