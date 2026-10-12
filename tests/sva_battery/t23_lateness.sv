module t23(input clk);
  reg [3:0] cnt = 0;
  always @(posedge clk) cnt <= cnt + 1;
  wire a = (cnt == 1) || (cnt == 2);  // violation of attempt@1 visible at cnt==2
  assert property (@(posedge clk) a |-> ##1 !a);
endmodule
