module t20(input clk, input a);
  reg pa = 1'b0;
  always @(posedge clk) pa <= a;
  // temporal env constraint: after any a, the next cycle is not a
  assume property (@(posedge clk) a |-> ##1 !a);
  // holds only under the assume (no two consecutive a's)
  assert property (@(posedge clk) !(a && pa));
endmodule
