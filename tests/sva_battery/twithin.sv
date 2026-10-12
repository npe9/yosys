module twithin (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a,b,c,d,e,f,g;
  assign a=(ctr==10); assign b=(ctr==11); assign c=(ctr==12);
  assign d=(ctr==13); assign e=(ctr==14); assign f=(ctr==12); assign g=(ctr==13);

  w_end:  assert property (@(posedge clk) ((c within (b ##2 d)) |-> (ctr == 8'd13)));        // PROVED
  w_late: assert property (@(posedge clk) ((f ##1 g) within (b ##3 e)) |-> (ctr == 8'd14));  // PROVED (late a-start)
  w_bad:  assert property (@(posedge clk) ((e within (b ##2 d)) |-> 1'b0));                  // vacuous: e@14 never in 11..13 -> PROVED
  w_bad2: assert property (@(posedge clk) ((c within (b ##2 d)) |-> (ctr == 8'd12)));       // DIS @13

  weak_ok:  assert property (@(posedge clk) a |-> weak(##1 b));    // PROVED (b@11 completes)
  weak_bad: assert property (@(posedge clk) a |-> weak(##1 c));    // DIS @11
endmodule
