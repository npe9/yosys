// sva_pbt case seed=3 index=5
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) (first_match(b) |=> (b until_with ($rose(c) && (c || (cnt == 2'd3))))));
endmodule
