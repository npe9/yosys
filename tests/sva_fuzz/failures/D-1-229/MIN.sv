// minimized from the p0 property of this directory's case.sv by
// delta-substitution while the sva2check crash persisted.
module top(input clk, input a, input b);
  p0: assert property (@(posedge clk) always ($fell(b) throughout a));
endmodule
