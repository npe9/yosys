module t42_tb;
  logic clk = 0;
  wire [7:0] y;
  t42 dut(.clk(clk), .y(y));
  always #5 clk = ~clk;
  always @(negedge clk) $display("y=%02x (expect 33)", y);
  initial begin #100 $finish; end
endmodule
