module t_bnd(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a = (ctr == 8'd10);
  c: assert property (@(posedge clk) a |-> ##[1:2] (ctr == 8'd11));
endmodule
