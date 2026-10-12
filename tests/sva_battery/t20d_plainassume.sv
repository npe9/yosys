module t20d(input clk, input a);
  reg pa = 1'b0;
  always @(posedge clk) pa <= a;
  assume property (@(posedge clk) !a);
  assert property (@(posedge clk) !(a && pa));
endmodule
