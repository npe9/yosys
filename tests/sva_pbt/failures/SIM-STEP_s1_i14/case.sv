// sva_pbt case seed=1 index=14
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) ((nexttime[2] (if (((!a) || (cnt[1] && r))) (b || (a || c)) else $past(c))) or (a && ((cnt == 2'd1) && c))));
endmodule
