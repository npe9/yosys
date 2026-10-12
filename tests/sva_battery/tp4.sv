module tp4 (input logic clk, input logic rst_n);
  logic [7:0] ctr;
  always_ff @(posedge clk) if (rst_n) ctr <= ctr + 8'd1;
  logic a; assign a=(ctr==10);
  property p4(sig, k);
    logic [7:0] got;
    @(posedge clk) (sig, got = ctr) |=> (ctr == got + k);
  endproperty
  ok: assert property (@(posedge clk) p4(a, 8'd1));
endmodule
