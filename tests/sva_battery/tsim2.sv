module tsim2(input logic clk, input logic rst_n);
  logic [3:0] ctr;
  always_ff @(posedge clk) if (rst_n) ctr <= ctr + 4'd1;
  logic a;
  assign a = (ctr == 4'd2) || (ctr == 4'd3);
  s1: assert property (@(posedge clk) a |-> ##1 !a);  // FIRE @3
  s2: assert property (@(posedge clk) a |=> !a);      // FIRE @3
  s3: assert property (@(posedge clk) a |-> ##1 1'b0);// FIRE @3 (const false 1 later)
  s4: assert property (@(posedge clk) a ##1 !a);      // antecedent form
endmodule
