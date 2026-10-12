module tp7b(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a=(ctr==10);
  sequence s8(g); ##1 (ctr == g + 8'd2); endsequence
  // NOTE: v is an IMPLICIT 1-bit wire here (match item to an undeclared
  // name); v1/v2's property is false at 11 (11 != 10+2).
  v1: assert property (@(posedge clk) (a, v = ctr) |=> (ctr == v + 8'd2));  // DIS @11
  property p; logic [7:0] v; @(posedge clk) (a, v = ctr) |=> (ctr == v + 8'd2); endproperty
  v2: assert property (@(posedge clk) p);                                    // DIS @11 (11 != 12)
  v3: assert property (@(posedge clk) a |=> s8(8'd10));                       // PROVED (##1 -> check at 12, 12==10+2)
  v4: assert property (@(posedge clk) (a, w = ctr) |=> s8(w));               // DIS @12 (w implicit 1-bit = 0)
endmodule
