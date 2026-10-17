// sva_pbt case seed=2 index=0
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) ((!b)[*3] |-> (((a && (a || b)) and (($fell(d) || $past(c)) || (d || 1'b0))) |-> $past(b, 2))));
endmodule
