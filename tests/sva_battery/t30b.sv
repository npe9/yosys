module t30b(input clk);
  reg [3:0] cnt = 0;
  always @(posedge clk) cnt <= cnt + 1;
  wire a = (cnt == 1) || (cnt == 2);
  assert property (@(posedge clk) a |-> (cnt==2));
endmodule
