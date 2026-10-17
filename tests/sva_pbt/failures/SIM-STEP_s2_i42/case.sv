// sva_pbt case seed=2 index=42
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) disable iff (r) (if ((cnt == 2'd0)) (nexttime[2] (if (d) (cnt == 2'd3) else (($rose(b) || b) && c))) else (b |=> ((b || d) && r))));
endmodule
