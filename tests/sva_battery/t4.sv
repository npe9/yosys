module t4 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  a_three: assert property (@(posedge clk) ctr != 8'd3);
endmodule
