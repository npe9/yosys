module tsb (input logic clk, input logic rst_n);
  logic [7:0] ctr;
  always_ff @(posedge clk) if (rst_n) ctr <= ctr + 8'd1;
  logic a, c; assign a=(ctr==10); assign c=(ctr==12);
  sequence sc(x); ##1 x; endsequence
  sequence sb(x, y); sc(x) ##0 sc(y); endsequence
  p_sb_ok: assert property (@(posedge clk) sb(a, c) |-> (ctr == 8'd12));
endmodule
