// `$` upper bounds (LRM 16.7, 16.9.2, 16.12.11): `$` lexes as an
// identifier and used to reach sva2check as an implicit wire named `$`
// ("non-constant bound"; ~80 corpus uses).  Closed design; attempts start
// after earlier occurrences so the bounds are anchored.
module tdollar(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic c, e;
  assign c = t == 2 || t == 4 || t == 7;
  assign e = t == 2 || t == 4 || t == 5 || t == 7;
  // ##[m:$] in an antecedent: c at 4 and 7 after the attempt at 3
  d_a:  assert property (@(posedge clk) (t == 3 ##[1:$] c) |-> (t == 4 || t == 7)); // PROVED
  d_b:  assert property (@(posedge clk) (t == 3 ##[1:$] c) |-> t != 7); // DIS @7
  d_c:  assert property (@(posedge clk) (t == 3 ##[2:$] c) |-> t == 7); // PROVED
  // [*m:$]: runs anchored at the attempt
  r_a:  assert property (@(posedge clk) (t == 5 ##1 e[*1:$]) |-> 0); // PROVED
  r_b:  assert property (@(posedge clk) (t == 3 ##1 e[*2:$]) |-> t == 5); // PROVED
  r_c:  assert property (@(posedge clk) (t == 3 ##1 e[*1:$]) |-> t == 4); // DIS @5
  // [->m:$] / [=m:$]
  g_a:  assert property (@(posedge clk) (t == 3 ##0 c[->2:$]) |-> t == 7); // PROVED
  g_b:  assert property (@(posedge clk) (t == 1 ##0 c[->2:$]) |-> t == 4); // DIS @7
  n_a:  assert property (@(posedge clk) (t == 3 ##0 c[=2:$]) |-> t >= 7); // PROVED
  n_b:  assert property (@(posedge clk) (t == 3 ##0 c[=2:$]) |-> t != 9); // DIS @9
  // always [m:$]: every tick from start+m on (the window used to open one
  // tick late: a_b held)
  a_a:  assert property (@(posedge clk) t == 9 |-> always [2:$] t >= 11); // PROVED
  a_b:  assert property (@(posedge clk) t == 9 |-> always [1:$] t >= 11); // DIS @10
  // intersect with one bounded operand is bounded (PBT s2_i27 shape)
  i_a:  assert property (@(posedge clk) t == 3 |-> ((1 ##1 c[->1]) intersect (1 ##1 1))); // PROVED
  i_b:  assert property (@(posedge clk) (t == 4 ##0 ((1 ##1 c[->1]) intersect (1 ##3 1))) |-> 0); // DIS @7
  // the right operand cannot end after 5, so no intersection exists
  i_c:  assert property (@(posedge clk) t == 4 |-> ((1 ##1 c[->1]) intersect (1 ##1 1))); // DIS @5
endmodule
