module t_min7f(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a = (ctr == 8'd10);
  property p7;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> ##1 (ctr == v + 8'd2);
  endproperty
  c: assert property (@(posedge clk) p7);   // PROVED
endmodule
