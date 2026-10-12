module t20b(input clk, input a);
  reg pa = 1'b0;
  always @(posedge clk) pa <= a;
  restrict property (@(posedge clk) a |-> ##1 !a);
  assert property (@(posedge clk) !(a && pa));
endmodule
