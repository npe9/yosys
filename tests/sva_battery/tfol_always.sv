// Two FALSE-PROOF classes found transcribing LRM 16.13.11 (2026-10-21):
//  (1) followed-by `s #-# p` / `s #=# p` was lowered as the implication
//      `s |-> p` / `s |=> p`, which holds vacuously when s does not match;
//      followed-by FAILS then (LRM 16.13.9: not (s |-> not p)).  Now exact
//      for a fixed-length s (one match point); a variable-length or
//      empty-capable s is refused (tests/verilog/sva_followed_by_varlen.ys).
//  (2) an unranged `always p` that is NOT the whole assertion (the
//      consequent of an implication, the body of an initial assertion)
//      checked p at its start tick only; it is now `always [0:$] p`.  The
//      previous build (34f5a0401) PROVES 7 of the 10 *_n below (f_short_n,
//      f_sh_n, g_fb_n, g_imp_n, h_imp_n, h_impd_n, h_ini_n).
// (`initial` sits on its own line so the battery reads the label.)
// Trace: r_ok @ 0..2, r_long @ 0..3, r_short @ 0..1, r2 @ 0..2 and 7,
// x everywhere but 7, y from 3 on.
module tfol_always (input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic r_ok, r_long, r_short, r2, x, y;
  assign r_ok = t <= 2;
  assign r_long = t <= 3;
  assign r_short = t <= 1;
  assign r2 = t <= 2 || t == 7;
  assign x = t != 7;
  assign y = t >= 3;

  // (1) followed-by
  initial
  f_ok:      assert property (@(posedge clk) r_ok[*3] #=# always !r_ok);       // PROVED
  initial
  f_long_n:  assert property (@(posedge clk) r_long[*3] #=# always !r_long);   // DIS @3
  initial
  f_short_n: assert property (@(posedge clk) r_short[*3] #=# always !r_short); // DIS @2
  f_ov_ok:   assert property (@(posedge clk) t == 0 |-> r_ok[*3] #-# !r_long);         // DIS @2
  f_ov2:     assert property (@(posedge clk) t == 1 |-> r_ok[*2] #-# r_long);          // PROVED
  f_sh_n:    assert property (@(posedge clk) t == 0 |-> r_short[*3] #=# always !r_short); // DIS @2
  // (2) unranged always, nested
  h_imp_n:   assert property (@(posedge clk) t == 3 |-> always x);   // DIS @7
  h_impd_n:  assert property (@(posedge clk) t == 3 |=> always x);   // DIS @7
  h_rng_n:   assert property (@(posedge clk) t == 3 |-> always [0:$] x); // DIS @7
  h_imp_ok:  assert property (@(posedge clk) t == 3 |-> always y);   // PROVED
  initial
  h_ini_n: assert property (@(posedge clk) always x);        // DIS @7
  initial
  g_fb_n:  assert property (@(posedge clk) r2[*3] #=# always !r2); // DIS @7
  initial
  g_imp_n: assert property (@(posedge clk) r2[*3] |=> always !r2); // DIS @7
  // a top-level `always p` keeps its meaning (an attempt every tick)
  h_top_ok:  assert property (@(posedge clk) always (t >= 0));       // PROVED
  h_top_n:   assert property (@(posedge clk) always x);              // DIS @7
endmodule
