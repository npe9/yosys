module tc1(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a=(ctr==10);
  c1: assert property (@(posedge clk) (a, w = ctr) |=> (w == 8'd10));
endmodule
