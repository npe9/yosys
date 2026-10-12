module t_bndi(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a = (ctr == 8'd10);
  sequence s(x); ##[1:2] x; endsequence
  c_ok:  assert property (@(posedge clk) a |=> s(ctr == 8'd12));  // PROVED: completes @12
  c_bad: assert property (@(posedge clk) a |=> s(ctr == 8'd13));  // PROVED
  c_b2:  assert property (@(posedge clk) a |-> s(ctr == 8'd11));  // PROVED: ends @11, cons true
  c_b3:  assert property (@(posedge clk) a |-> s(ctr == 8'd12));  // PROVED: ends @12, cons true
endmodule
