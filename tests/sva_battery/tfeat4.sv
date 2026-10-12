module tfeat4(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b, c;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd11);
  assign c = (ctr == 8'd12);

  // first_match across `or`: branch (a ##1 b) ends 11, branch (a ##2 c)
  // ends 12; first keeps 11.
  p_fmo_ok:  assert property (@(posedge clk) first_match((a ##1 b) or (a ##2 c)) |-> (ctr <= 8'd11)); // PROVED
  p_fmo_bad: assert property (@(posedge clk) first_match((a ##1 b) or (a ##2 c)) |-> (ctr <= 8'd10)); // DIS @11

  // first_match of a chain with two ranged delays.  e = (ctr==12||13):
  // a ##[1:2] e ##[1:2] (ctr==13 || ctr==14) parses: e@12,c@13 (end 13);
  // e@12,c@14 (end 14).  first keeps 13, plain keeps both.
  logic e;
  assign e = (ctr == 8'd12) || (ctr == 8'd13);
  p_fmc_ok:  assert property (@(posedge clk) first_match(a ##[1:2] e ##[1:2] (ctr == 8'd13 || ctr == 8'd14)) |-> (ctr <= 8'd13)); // PROVED
  p_fmc_bad: assert property (@(posedge clk) first_match(a ##[1:2] e ##[1:2] (ctr == 8'd13 || ctr == 8'd14)) |-> (ctr <= 8'd12)); // DIS @13
  // plain version: the 14-end breaks ctr<=13
  p_pln_bad: assert property (@(posedge clk) (a ##[1:2] e ##[1:2] (ctr == 8'd13 || ctr == 8'd14)) |-> (ctr <= 8'd13)); // DIS @14

  // first_match on the consequent side completes strongly at the earliest
  // end: a@10, then b@11, c one or two later (12 or 13): first end 12.
  // The strong-expiry deadline is static (max offset 2 -> timeout one tick
  // after the truncated end), so the failure clocks at 13, not 12.
  p_fms_ok:  assert property (@(posedge clk) a |=> first_match(b ##[1:2] c) ##0 (ctr == 8'd12)); // PROVED
  p_fms_bad: assert property (@(posedge clk) a |=> first_match(b ##[1:2] c) ##0 (ctr == 8'd13)); // DIS @13 (frame 14: static strong expiry)
endmodule
