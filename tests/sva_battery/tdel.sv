module tdel (input logic clk, input logic rst_n);
  logic [7:0] ctr;
  always_ff @(posedge clk) if (rst_n) ctr <= ctr + 8'd1;
  logic a, b;
  assign a=(ctr==10); assign b=(ctr==11);
  c1: assert property (@(posedge clk) a |-> ##1 b);       // DIS @11 (b@11)
  c2: assert property (@(posedge clk) a |-> ##2 b);       // DIS @12
  c3: assert property (@(posedge clk) a |=> ##1 b);       // DIS @12
  c4: assert property (@(posedge clk) a |=> b);           // DIS @11
  c5: assert property (@(posedge clk) ##1 a |-> (ctr == 8'd10)); // PROVED? ##1 a antecedent: a@t-1... ant_end@10 means a@10?? attempt@9->a@10 end@10; |-> ctr==10 @10 ✓
endmodule
