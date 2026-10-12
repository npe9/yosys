module t5 (input logic clk, input logic a, b, c);
  p1: assert property (@(posedge clk) a ##2 b |-> c);
  p2: assert property (@(posedge clk) a[*2:3] ##1 b |-> c);
endmodule
