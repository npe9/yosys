module tassume(input clk, input a);
  assume property (@(posedge clk) !a);
  a1: assert property (@(posedge clk) !a);   // PROVED iff assume enforced
  a2: assert property (@(posedge clk) a);    // DIS @0
endmodule
