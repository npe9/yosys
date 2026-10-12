module t43_tb;
  logic [3:0] tag = 4'b0100;
  wire [7:0] y;
  t43 dut(.tag(tag), .y(y));
  always #5 clk = ~clk;
  always @(negedge clk) $display("y=%02x (expect 33)", y);
  initial begin #30 $finish; end
endmodule
