module tc4(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a=(ctr==10);
  sequence s8(g); ##1 (ctr == g + 8'd2); endsequence
  property p4; logic [7:0] w; @(posedge clk) (a, w = ctr) |=> s8(w); endproperty
  c4: assert property (@(posedge clk) p4);
endmodule
