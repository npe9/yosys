module t21(input clk, input a);
  assert property (@(posedge clk) a |-> ##1 !a);
endmodule
