// Expressions over a declaration's FORMAL arguments (2026-10-21).  Two
// mechanisms read the undriven formal wire, so the verdict was on a FREE
// input, never on the actual:
//  (i) instance path: sampled-value cells over a formal
//      (`sequence g(a); $rose(a); endsequence`) were lowered on the formal
//      wire before any instance was inlined.  Now they are templates: each
//      instance gets a copy with its actual spliced in, lowered on the
//      check's clock.
//  (ii) root path: `assert property (p(x, y))` bound formals for BARE leaf
//      reads only; any expression over a formal (`!a`, `a && b`, `a == 1`,
//      `$rose(a)`, a disable condition) was not spliced.  Now the root spec
//      tree is copied with every expression port spliced through the bound
//      formals (composed across nested instances).  A memory read indexed
//      by a formal (`mem[a]`, caliptra fv_ecc_ram_tdp_file) is copied the
//      same way (its output is DATA, not Y: it was never chased).
// Found transcribing 16.9.11 (tlrm_trig.sv: rule2 vs rule).  The
// previous build (9eb7eef91): 19 of 33 wrong on both legs -- 16 false
// disproofs, 3 disproofs at the wrong tick (no false proof here: a free
// input can always be driven to fail).
// Trace: x @ 2..4, 7; y @ 3, 8.
module tsvformal(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic x, y;
  logic [7:0] mem [0:3];
  initial begin mem[0] = 8'd10; mem[1] = 8'd11; mem[2] = 8'd12; mem[3] = 8'd13; end
  assign x = (t >= 2 && t <= 4) || t == 7;
  assign y = t == 3 || t == 8;

  sequence s_rose(a);   @(posedge clk) $rose(a); endsequence
  sequence s_fell(a);   @(posedge clk) $fell(a); endsequence
  sequence s_stab(a);   @(posedge clk) $stable(a); endsequence
  sequence s_chg(a);    @(posedge clk) $changed(a); endsequence
  sequence s_past2(a);  @(posedge clk) $past(a, 2); endsequence
  sequence s_and(a, b); @(posedge clk) $rose(a && b); endsequence
  sequence s_then(a, b); @(posedge clk) $rose(a) ##1 b; endsequence
  property p_rose(a);   @(posedge clk) $rose(a) |-> ##1 a; endproperty
  property p_nest(a, b); @(posedge clk) s_then(a, b) |-> ##1 !b; endproperty

  f_rose:   assert property (@(posedge clk) t == 2 |-> s_rose(x));          // PROVED
  f_rose_n: assert property (@(posedge clk) t == 3 |-> s_rose(x));          // DIS @3
  f_rose_y: assert property (@(posedge clk) t == 3 |-> s_rose(y));          // PROVED (same decl, other actual)
  f_fell:   assert property (@(posedge clk) t == 5 |-> s_fell(x));          // PROVED
  f_fell_n: assert property (@(posedge clk) t == 4 |-> s_fell(x));          // DIS @4
  f_stab:   assert property (@(posedge clk) t == 4 |-> s_stab(x));          // PROVED
  f_stab_n: assert property (@(posedge clk) t == 7 |-> s_stab(x));          // DIS @7
  f_chg:    assert property (@(posedge clk) t == 8 |-> s_chg(x));           // PROVED
  f_chg_n:  assert property (@(posedge clk) t == 3 |-> s_chg(x));           // DIS @3
  f_past:   assert property (@(posedge clk) t == 9 |-> s_past2(x));         // PROVED (x @ 7)
  f_past_n: assert property (@(posedge clk) t == 8 |-> s_past2(x));         // DIS @8
  f_and:    assert property (@(posedge clk) t == 3 |-> s_and(x, y));        // PROVED
  f_and_n:  assert property (@(posedge clk) t == 8 |-> s_and(x, y));        // DIS @8
  f_prop:   assert property (p_rose(x));                                   // DIS @8 (x rises at 7, low at 8)
  f_prop2:  assert property (p_rose(y || x));                               // PROVED (y || x @ 2..4, 7, 8: rises at 2 and 7, high after both)
  f_nest:   assert property (p_nest(x, y));                                // PROVED
  f_nest_n: assert property (p_nest(x, x));                                // DIS @4 (rose 2, x 3, x still high at 4)
  f_bare:   assert property (@(posedge clk) s_rose(x) |-> t == 2 || t == 7); // PROVED
  f_bare_n: assert property (@(posedge clk) s_rose(x) |-> t == 2);           // DIS @7

  // (ii) root instances: expressions over formals, nesting, clock, disable
  property r_not(a); @(posedge clk) (t == 5) |-> !a; endproperty
  property r_and(a, b); @(posedge clk) (t == 3) |-> (a && b); endproperty
  property r_inner(b); @(posedge clk) t == 5 |-> !b; endproperty
  property r_outer(a); r_inner(a); endproperty
  property r_clk(c, a); @(posedge c) t == 2 |-> a; endproperty
  property r_dis(d, a); @(posedge clk) disable iff (d) t == 3 |-> !a; endproperty
  g_not:    assert property (r_not(x));          // PROVED (x low at 5)
  g_not_n:  assert property (r_not(!x));         // DIS @5
  g_and:    assert property (r_and(x, y));       // PROVED
  g_and_n:  assert property (r_and(x, !y));      // DIS @3
  g_nest:   assert property (r_outer(x));        // PROVED
  g_nest_n: assert property (r_outer(!x));       // DIS @5
  g_clk:    assert property (r_clk(clk, x));     // PROVED
  g_clk_n:  assert property (r_clk(clk, y));     // DIS @2
  g_dis:    assert property (r_dis(t == 3, x));  // PROVED (disabled at 3)
  g_dis_n:  assert property (r_dis(t == 4, x));  // DIS @3
  property r_mem(a); @(posedge clk) t == 2 |-> mem[a] == 8'd12; endproperty
  sequence s_mem(a); @(posedge clk) mem[a] == 8'd12; endsequence
  g_mem:    assert property (r_mem(t[1:0]));          // PROVED
  g_mem_n:  assert property (r_mem(t[1:0] + 2'd1));   // DIS @2
  f_mem:    assert property (@(posedge clk) t == 6 |-> s_mem(t[1:0])); // PROVED (6 mod 4 = 2)
  f_mem_n:  assert property (@(posedge clk) t == 5 |-> s_mem(t[1:0])); // DIS @5
endmodule
