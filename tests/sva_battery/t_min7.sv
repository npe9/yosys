module t_min7(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a = (ctr == 8'd10);
  sequence s8(g); ##1 (ctr == g + 8'd2); endsequence
  property p7;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> s8(v);
  endproperty
  c: assert property (@(posedge clk) p7);   // PROVED
endmodule
