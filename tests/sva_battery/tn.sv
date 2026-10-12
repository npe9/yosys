module tn (input logic clk, input logic rst_n);
  logic [7:0] ctr;
  always_ff @(posedge clk) if (rst_n) ctr <= ctr + 8'd1;
  logic a,b; assign a=(ctr==10); assign b=(ctr==11);
  property p6; @(posedge clk) a |=> b; endproperty
  p_name: assert property (@(posedge clk) p6);
  direct: assert property (@(posedge clk) a |=> b);
endmodule
