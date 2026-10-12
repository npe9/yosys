module tsb2 (input logic clk, input logic rst_n);
  logic [7:0] ctr;
  always_ff @(posedge clk) if (rst_n) ctr <= ctr + 8'd1;
  logic a, c; assign a=(ctr==10); assign c=(ctr==12);
  sequence scx(xx); ##1 xx; endsequence
  sequence sbq(qq, rr); scx(qq) ##0 scx(rr); endsequence
  p_ok: assert property (@(posedge clk) sbq(a, c) |-> (ctr == 8'd12));
endmodule
