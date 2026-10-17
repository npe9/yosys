// sva_pbt case seed=3 index=40
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) (((($rose(b) && 1'b0) || (1'b0 && r)) ##[2:4] (((!(cnt == 2'd0)) || (!$rose(a))) and (b || ((cnt == 2'd0) || d)))) |=> (cnt == 2'd3)));
endmodule
