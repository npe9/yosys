// Empty matches have no tick (LRM 16.9.2.1): (empty ##n s) = ##(n-1) s,
// (s ##n empty) = s ##(n-1) `true for n > 0, and ##0 next to an empty
// match does not match.  Implication antecedents (Annex F): |-> ignores an
// empty match, |=> = (R ##1 1) |-> P checks P at the attempt tick.  The
// engine used to give [*0..] / [*] an end AT the start tick (one tick
// late inside ##, a match for |->, one tick late for |=>): every pin
// below except the *_ok twins was wrong on cf538aac9, tagged exact
// (register v3 E1).  Closed design.
module tempty(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic c, d, e, q;
  assign c = t == 2 || t == 4 || t == 7;
  assign d = t == 2 || t == 7 || t == 8;
  assign e = t == 2 || t == 4 || t == 5 || t == 7;
  assign q = t == 3 || t == 9;
  // tail empty: t==6 ##1 d[*0:1] ##1 d matches at 7 (empty) and 8
  e_mid:    assert property (@(posedge clk) (t == 6 ##1 d[*0:1] ##1 d) |-> t == 8); // DIS @7
  e_mid_ok: assert property (@(posedge clk) (t == 6 ##1 d[*0:1] ##1 d) |-> (t == 7 || t == 8)); // PROVED
  e_mid_no: assert property (@(posedge clk) (t == 5 ##1 d[*0:1] ##1 d) |-> 0); // PROVED
  // the corpus idiom x[*0:N] ##1 y: y may hold at the start tick
  e_idiom:  assert property (@(posedge clk) t == 3 |-> e[*0:3] ##1 q); // PROVED
  e_idiomf: assert property (@(posedge clk) (t == 8 ##0 (e[*0:3] ##1 q)) |-> 0); // PROVED
  // ##0 next to an empty match: no match
  e_zero:   assert property (@(posedge clk) (t == 6 ##0 d[*0:1]) |-> 0); // PROVED
  // ranged: t==5 ##[1:2] d[*0:1] ends at 5, 6 (empty) and 7 (d@7)
  e_rng_ok: assert property (@(posedge clk) (t == 5 ##[1:2] d[*0:1]) |-> (t >= 5 && t <= 7)); // PROVED
  e_rng:    assert property (@(posedge clk) (t == 5 ##[1:2] d[*0:1]) |-> t != 5); // DIS @5
  // head empty: (d[*0:1] ##2 c) from 1 = c at 2 (empty) or d@1 & c@3
  e_head:   assert property (@(posedge clk) (t == 1 ##0 (d[*0:1] ##2 c)) |-> 0); // DIS @2
  // implication antecedents
  e_impl:   assert property (@(posedge clk) d[*0:1] |-> d); // PROVED
  e_nimpl:  assert property (@(posedge clk) d[*0:1] |=> t != 0); // DIS @0
endmodule
