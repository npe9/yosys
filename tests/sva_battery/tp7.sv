module tp7(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a=(ctr==10);
  sequence s8(g); ##1 (ctr == g + 8'd2); endsequence
  property p7;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> s8(v);
  endproperty
  p7_ok: assert property (@(posedge clk) p7);
endmodule
