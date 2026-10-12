module tlet2 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a=(ctr==10);
  let eqctr(x) = (ctr == x);
  p_let:  assert property (@(posedge clk) a |-> ##1 eqctr(8'd11));   // PROVED
  p_let2: assert property (@(posedge clk) a |-> ##1 eqctr(8'd12));   // DIS @11
  p_arg:  assert property (@(posedge clk) eqctr(8'd10) |-> ##1 (ctr == 8'd11)); // PROVED
endmodule
