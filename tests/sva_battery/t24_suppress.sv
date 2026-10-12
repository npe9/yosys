module t24(input clk);
  reg [3:0] cnt = 0;
  always @(posedge clk) cnt <= cnt + 1;
  // attempts at 1,2 (violation), 4,5 (violation); pair (1,2) completes... 
  // a(1)=1,a(2)=1 -> viol; completion recorded at 2; pair (4,5): fail tick 6,
  // win3(6)=a(2)&&!a(3)=1 -> suspected suppression of a REAL violation
  wire a = (cnt == 1) || (cnt == 2) || (cnt == 4) || (cnt == 5);
  assert property (@(posedge clk) a |-> ##1 !a);
endmodule
