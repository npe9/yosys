// sva_fuzz case seed=1 index=231
module top(input clk, input a, input b, input c);
  logic [1:0] cnt;
  always_ff @(posedge clk)
    if (a) cnt <= cnt + 1;
  p0: assert property (@(posedge clk) always (((cnt[1] throughout ((($past(c,1) throughout (cnt[1])) ##2 c))) ##2 c) ##3 ((cnt[0][*3:4] or cnt[0]) ##3 a)));
endmodule
