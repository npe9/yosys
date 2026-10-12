module t22(input clk, input a);
  assume property (@(posedge clk) a |-> ##1 !a);
  assert property (@(posedge clk) a |-> ##1 !a);
endmodule
