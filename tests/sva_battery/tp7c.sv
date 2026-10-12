module tp7c(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a; assign a=(ctr==10);
  // caller local flows into a sequence instance actual
  property q1; logic [7:0] w;
    @(posedge clk) (a, w = ctr) |=> (w == 8'd10);   // capture@10, read@11: PROVED
  endproperty
  c1: assert property (@(posedge clk) q1);          // PROVED
  property q2; logic [7:0] w;
    @(posedge clk) (a, w = ctr) |=> (ctr == w);     // 11==10 -> DIS
  endproperty
  c2: assert property (@(posedge clk) q2);          // DIS @11
  sequence s8(g); ##1 (ctr == g + 8'd2); endsequence
  property q3; logic [7:0] w;
    @(posedge clk) (a, w = ctr) |-> s8(w);          // |-> overlap: ##1@11: 11==12 -> DIS
  endproperty
  c3: assert property (@(posedge clk) q3);          // DIS @11
  property q4; logic [7:0] w;
    @(posedge clk) (a, w = ctr) |=> s8(w);          // ##1@12: 12==10+2 -> PROVED
  endproperty
  c4: assert property (@(posedge clk) q4);          // PROVED
endmodule
