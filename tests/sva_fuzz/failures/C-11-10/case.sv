// sva_fuzz case seed=11 index=10
module top(input clk, input a, input b, input c, input d, input [1:0] sel);
  logic [2:0] cnt;
  always_ff @(posedge clk)
    if (a) cnt <= cnt + 1;
  p0: assert property (@(posedge clk) always (s_nexttime (d ##3 c)));
endmodule
