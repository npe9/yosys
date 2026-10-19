// Repetitions whose attempt starts AFTER earlier occurrences of the body.
// Discriminating pins: every older nonconsecutive/run pin started its
// attempt at tick 0/1, where a time-0 count equals the per-attempt count.
//   [->]/[=]: the anchored snapshot ring read the slot of the attempt
//   starting at the END tick (g_a3 false disproof, g_both_bad false
//   proof, both tagged exact).  [*m:hi>8] / [*] / s[+]: the run forms
//   ignored `start` (r_x, r_y, d_plus) or latched forever after one copy
//   (s_plus).  Both now use exact live-count sets.  Closed design.
module tnc_anchor(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic b, c, e, d;
  assign b = t == 2 || t == 3;
  assign c = t == 2 || t == 4 || t == 7;
  assign e = t == 2 || t == 4 || t == 5 || t == 7;
  assign d = t == 2 || t == 7 || t == 8;
  // b[=2] ends AT the 2nd b (empty !b tail) and at later !b ticks
  f2_end:  assert property (@(posedge clk) (t == 0 ##1 b[=2]) |-> t != 3); // DIS @3
  f2_tail: assert property (@(posedge clk) (t == 0 ##1 b[=2]) |-> t != 5); // DIS @5
  f2_ok:   assert property (@(posedge clk) (t == 0 ##1 b[=2]) |-> t >= 3); // PROVED
  // overlapping attempts on c[->2]: from 1 -> 4, from 3 -> 7
  g_a1:       assert property (@(posedge clk) (t == 1 ##0 c[->2]) |-> t == 4); // PROVED
  g_a3:       assert property (@(posedge clk) (t == 3 ##0 c[->2]) |-> t == 7); // PROVED
  g_both:     assert property (@(posedge clk) ((t == 1 || t == 3) ##0 c[->2]) |-> (t == 4 || t == 7)); // PROVED
  g_both_bad: assert property (@(posedge clk) ((t == 1 || t == 3) ##0 c[->2]) |-> t == 4); // DIS @7
  g_rng:      assert property (@(posedge clk) (t == 3 ##0 c[->1:2]) |-> (t == 4 || t == 7)); // PROVED
  n_rep:      assert property (@(posedge clk) ((t == 3 ##0 c[=1]) ##0 t == 6) |-> 0); // DIS @6
  // consecutive runs anchored at their start
  r_x:    assert property (@(posedge clk) (t == 5 ##1 e[*1:20]) |-> 0); // PROVED
  r_y:    assert property (@(posedge clk) (t == 3 ##1 e[*1:20]) |-> (t == 4 || t == 5)); // PROVED
  r_y2:   assert property (@(posedge clk) (t == 3 ##1 e[*2:20]) |-> t == 5); // PROVED
  d_plus: assert property (@(posedge clk) (t == 5 ##1 d[+]) |-> 0); // PROVED
  d_ok:   assert property (@(posedge clk) (t == 6 ##1 d[+]) |-> (t == 7 || t == 8)); // PROVED
  // copies 2..4 / 4..5 (c@6 low ends the run)
  s_plus:  assert property (@(posedge clk) (t == 1 ##1 (c ##1 !c)[+]) |-> (t == 3 || t == 5)); // PROVED
  s_plus3: assert property (@(posedge clk) (t == 1 ##1 (c ##1 !c)[+]) |-> t == 3); // DIS @5
endmodule
