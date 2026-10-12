module tlet3 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a=(ctr==10);
  let eqctr(x) = (ctr == x);
  sequence slet; ##1 eqctr(8'd11); endsequence
  p_let:  assert property (@(posedge clk) a |-> slet);    // PROVED
  p_let2: assert property (@(posedge clk) a |-> sletb);  // DIS @11
  sequence sletb; ##1 eqctr(8'd12); endsequence
endmodule
