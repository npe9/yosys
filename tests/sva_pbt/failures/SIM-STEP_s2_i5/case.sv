// sva_pbt case seed=2 index=5
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) (if ((r && a)) (not c) else (((!(!cnt[0])) ##[2:4] ((c && $past(c, 2)) && (c && cnt[0]))) and (r && (!d)))));
endmodule
