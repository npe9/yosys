module ts1(input logic clk);
  logic [3:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 4'd1;
  logic a;
  assign a = (ctr == 4'd2) || (ctr == 4'd3);
  s1: assert property (@(posedge clk) a |-> ##1 !a);
endmodule
