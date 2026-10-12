module t20c(input clk, input a);
  reg pa = 1'b0;
  always @(posedge clk) pa <= a;
  assert property (@(posedge clk) !(a && pa));
endmodule
