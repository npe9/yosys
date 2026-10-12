module tassume2(input clk, input a);
  assume property (@(posedge clk) a |-> ##1 !a);
  a1: assert property (@(posedge clk) a |-> ##1 !a);   // PROVED (same as assume)
  a2: assert property (@(posedge clk) !(a && $past(a,1))); // PROVED under assume
endmodule
