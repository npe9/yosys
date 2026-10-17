// sva_pbt case seed=3 index=38
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) (((d ##[1:3] (($past(d) || $stable(c)) || $stable(d))) ##2 first_match($stable(a))) |-> ((not (!($rose(a) && a))) and ((!1'b0) || $past(d)))));
endmodule
