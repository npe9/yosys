// sva_fuzz case seed=1 index=176
module top(input clk, input a, input b, input c);
  logic [1:0] cnt;
  always_ff @(posedge clk) cnt <= cnt + 1;
  p0: assert property (@(posedge clk) 1'b1);
endmodule
