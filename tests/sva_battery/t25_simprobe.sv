module t25(input clk);
  reg [3:0] cnt = 0;
  always @(posedge clk) cnt <= cnt + 1;
  assert property (@(posedge clk) cnt != 2);
endmodule
