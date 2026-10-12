module tsim_assume(input logic clk, input logic rst_n);
  reg [3:0] vec = 4'b0100;  // shift pattern: produces 1,1 at some point? use a counter instead
  logic [3:0] ctr;
  always_ff @(posedge clk) if (rst_n) ctr <= ctr + 4'd1;
  logic a;
  assign a = (ctr == 4'd2) || (ctr == 4'd3);  // two consecutive a's at 2,3
  ap: assert property (@(posedge clk) a |-> ##1 !a);  // must FIRE at ctr=3
endmodule
