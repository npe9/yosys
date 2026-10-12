module t_min4(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a = (ctr == 8'd10);
  property p4(sig, k);
    logic [7:0] got;
    @(posedge clk) (sig, got = ctr) |=> (ctr == got + k);
  endproperty
  p_p4_ok: assert property (@(posedge clk) p4(a, 8'd1));   // PROVED
endmodule
