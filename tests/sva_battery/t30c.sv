module t30c(input clk);
  reg [3:0] cnt = 0;
  always @(posedge clk) cnt <= cnt + 1;
  wire a = (cnt == 1) || (cnt == 2);
  assert property (@(posedge clk) a |=> !a);
endmodule
