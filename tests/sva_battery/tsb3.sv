module tsb3 (input logic clk, input logic rst_n);
  logic [7:0] ctr;
  always_ff @(posedge clk) if (rst_n) ctr <= ctr + 8'd1;
  logic a, c; assign a=(ctr==10); assign c=(ctr==12);
  sequence scx(xx); ##1 xx; endsequence
  t1: assert property (@(posedge clk) scx(a) |-> (ctr == 8'd11));   // DIS @10: a@10, scx ends 11
  t2: assert property (@(posedge clk) a |-> scx(c));                // DIS @11: c@12
  t3: assert property (@(posedge clk) a |-> (##1 c));               // DIS @11: direct ##1 same shape
  t4: assert property (@(posedge clk) (##1 a) |-> (ctr == 8'd10));  // PROVED
endmodule
