// sva_pbt case seed=1 index=39
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) ((always[0:0] $stable(b)) or (nexttime[2] (((d && $stable(b)) || (cnt == 2'd3)) |=> r))));
endmodule
