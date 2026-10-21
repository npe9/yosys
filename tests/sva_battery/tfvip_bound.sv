// The AXI4 FVIP (SVA-AXI4-FVIP 250f1ff) against the FVIP's own crossbar
// example, run from another session (2026-10-21); found four frontend /
// sva2check gaps, all pinned here:
//  (1) a delay bound that is a declaration formal
//      (`property handshake_max_wait(valid, ready, timeout); valid & !ready
//      |-> ##[1:timeout] ready;`, actual cfg.MAXWAIT) was "used in property
//      position has no finite bound": the static length analysis looked
//      into the declaration without the instance's actuals, and cached the
//      unbound -1 per cell.  Now it binds the actuals, follows them (also a
//      package declaration's zero-padded formal port), and never caches a
//      length that depended on a binding -- the same declaration with
//      actuals 2 and 3 below.
//  (2) `let calc_max_size(w) = $clog2(w);` was "$clog2 inside a concurrent
//      assertion needs a constant argument" at the declaration, before any
//      caller: a non-constant argument is now a small circuit that folds
//      once the actual is spliced in.
//  (3) `$isunbounded` (20.6.3) was an unresolved function.
//  (4) labelled assertions in a generate loop all got the same cell name
//      ("Cannot add procedural assertion ... already created"; stock yosys
//      0.51 too); they are named per scope now (sym[1].ap, sym[2].ap).  The
//      loop below is the regression pin: a duplicate name fails the file.
//  (5) an expression over a package declaration's formal
//      (`(!aresetn || $rose(aresetn)) |-> !valid`, the FVIP's
//      exit_from_reset) crashed (rc=-11): the actual arrives zero-padded
//      to 32 bits, the copied `$logic_not` kept A_WIDTH 1, the internal
//      cell checker rejected it, and its error recursed inside the
//      parallel check (upstream logger asserts no multithreading).  The
//      padding is dropped now, and a genuinely wider actual (an untyped
//      formal) carries the width parameter: nz(w4) reads all 4 bits.
//  (6) a clock taken from the instance body: tfvip_clk.sv.
// Trace: v @ 1, r @ 4, tt = t[2:0], rn from 2, vv @ 5, w4 = 2 @ 6.
package tfvip_pkg;
  typedef struct packed { int unsigned MAXWAIT; bit ON; } cfg_t;
  // unclocked package declaration (the FVIP's form)
  property hmw_p(valid, ready, timeout); valid & !ready |-> ##[1:timeout] ready; endproperty
  property exit_from_reset(aresetn, valid); (!aresetn || $rose(aresetn)) |-> !valid; endproperty
  property nz(a); a != 0; endproperty
  property lnot(a); !a; endproperty
endpackage

module tfvip_bound import tfvip_pkg::*;
  #(parameter cfg_t cfg = '{MAXWAIT: 3, ON: 1}, parameter int unsigned MW = 3)
  (input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic v, r, rst;
  logic [2:0] tt;
  assign v = t == 1;
  assign r = t == 4;
  assign rst = 1'b0;
  assign tt = t[2:0];
  logic rn, vv;
  logic [3:0] w4;
  assign rn = t >= 2;
  assign vv = t == 5;
  assign w4 = (t == 6) ? 4'd2 : 4'd0;
  default clocking @(posedge clk); endclocking

  property hmw(valid, ready, timeout); valid & !ready |-> ##[1:timeout] ready; endproperty
  sequence w(n); ##[1:n] r; endsequence
  property q(valid, m); valid |-> w(m); endproperty

  b_lit3: assert property (hmw(v, r, 3));                       // PROVED
  b_lit2: assert property (hmw(v, r, 2));                       // DIS @3
  b_lit5: assert property (hmw(v, r, 5));                       // PROVED
  b_par:  assert property (hmw(v, r, MW));                      // PROVED
  b_cfg:  assert property (disable iff (rst) hmw(v, r, cfg.MAXWAIT)); // PROVED
  b_pkg3: assert property (hmw_p(v, r, 3));                     // PROVED
  b_pkg2: assert property (hmw_p(v, r, 2));                     // DIS @3
  b_nst3: assert property (q(v, 3));                            // PROVED
  b_nst2: assert property (q(v, 2));                            // DIS @3
  if (cfg.ON) begin: deadlock_check
    b_gen: assert property (disable iff (rst) hmw_p(v, r, cfg.MAXWAIT)); // holds (deadlock_check.b_gen: scoped, read by hand)
  end

  // (2) $clog2 over a let formal
  let lg(x) = $clog2(x);
  property sz_ok(size, strb); size <= lg(strb); endproperty
  c_lg:   assert property (lg(1) == 0 && lg(4) == 2 && lg(5) == 3 && lg(16) == 4 && lg(17) == 5); // PROVED
  c_sz:   assert property (t <= 4 |-> sz_ok(tt, 16));           // PROVED
  c_szn:  assert property (sz_ok(tt, 16));                      // DIS @5

  // (3) $isunbounded
  localparam bit UB_MW = $isunbounded(MW);
  localparam bit UB_D = $isunbounded($);
  u_ub:   assert property (UB_MW == 1'b0 && UB_D == 1'b1);      // PROVED

  // (5) expressions over a package declaration's formals
  l_ok:   assert property (exit_from_reset(rn, vv));            // PROVED
  l_bad:  assert property (exit_from_reset(rn, t == 4'd2));     // DIS @2
  l_w:    assert property (t == 6 |-> nz(w4));                  // PROVED
  l_wn:   assert property (lnot(w4));                           // DIS @6

  // (4) generate-scoped labels
  for (genvar i = 1; i < 3; i++) begin: sym
    ap: assert property (v |-> ##[1:2+i] r);
  end
endmodule
