module tlet (input logic clk, input logic rst_n);
  logic [7:0] ctr;
  always_ff @(posedge clk) if (rst_n) ctr <= ctr + 8'd1;
  logic a; assign a=(ctr==10);
  let eqctr(x) = (ctr == x);
  p_let:  assert property (@(posedge clk) a |-> ##1 eqctr(8'd11));
  p_direct: assert property (@(posedge clk) a |-> ##1 (ctr == 8'd11));
endmodule
