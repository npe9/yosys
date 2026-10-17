// sva_pbt case seed=3 index=42
module top(input clk, input a, input b, input c, input d, input rst);
  reg r; initial r = 1'b0;
  always @(posedge clk) r <= a;
  reg [1:0] cnt; initial cnt = 2'd0;
  always @(posedge clk) cnt <= cnt + 2'd1;
  p0: assert property (@(posedge clk) disable iff (r) ((first_match((cnt == 2'd2)) implies (((!c) && d) until_with (b || $past(b, 2)))) or (nexttime[1] (b and (!(b && $rose(b)))))));
endmodule
