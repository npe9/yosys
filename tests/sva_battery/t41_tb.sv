module t41_tb;
  logic clk = 0;
  logic [3:0] tag = 4'b0100;
  wire [7:0] y;
  t41 dut(.clk(clk), .tag(tag), .y(y));
  always #5 clk = ~clk;
  always @(negedge clk) $display("y=%02x (expect 33)", y);
  initial begin #100 $finish; end
endmodule
