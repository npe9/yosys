module t3 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) begin
    if (ctr < 8'd100)
      ctr <= ctr + 8'd1;
  end
  a_bound: assert property (@(posedge clk) ctr <= 8'd100);
  a_fifty: assert property (@(posedge clk) ctr != 8'd50);
  a_true:  assert property (@(posedge clk) (ctr != 8'd7) || (ctr == 8'd7));
endmodule
