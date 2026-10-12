module tassume3(input clk, input a);
  reg pa = 1'b0;
  always @(posedge clk) pa <= a;
  assume property (@(posedge clk) a |-> ##1 !a);
  a1: assert property (@(posedge clk) !(a && pa));     // PROVED under assume
  a2: assert property (@(posedge clk) a |-> !pa);      // PROVED under assume
endmodule
