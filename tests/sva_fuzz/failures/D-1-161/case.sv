// sva_fuzz case seed=1 index=161
module top(input clk, input a, input b, input c);
  logic [1:0] cnt;
  always_ff @(posedge clk) cnt <= cnt + 1;
  p0: assert property (@(posedge clk) (not (cnt[0] throughout (($fell(cnt[1]) throughout ($rose(b)))))));
endmodule
