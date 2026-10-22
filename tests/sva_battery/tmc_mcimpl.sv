// SVA2CHECK-FLAGS: -clk2fflogic
// PREP-EXTRA-AFTER-MEMORY: clk2fflogic
// LRM 16.14.2 multiclock implication and 16.14.5 .matched (2026-10-22).
// |=> starts the consequent at the nearest STRICTLY future tick of its
// clock after the antecedent end; |-> at the nearest possibly
// overlapping one.  The v1 handoff read the antecedent at the current
// step and needed it to persist until the inner clock sampled it (a
// one-step antecedent was never seen) and checked the consequent one
// inner tick late: n_strict and o_near were PROVED (false).  Cross-clock
// .matched used to error; the source match is now held until the first
// destination tick after it.  Deterministic clocks as in tmc_c2f.sv:
// c0 ticks at 2, 8, 12; c1 at 4, 6, 10; c1b at 2, 6, 10 (coincides with
// c0 at 2).  A tick at step k samples step k-1.
module tmc_mcimpl ();
  logic [3:0] g = 4'd0;
  always @($global_clock) if (g != 4'd15) g <= g + 4'd1;
  wire c0  = (g == 2) || (g == 8) || (g == 12);
  wire c1  = (g == 4) || (g == 6) || (g == 10);
  wire c1b = (g == 2) || (g == 6) || (g == 10);
  wire a   = (g == 1);               // sampled true at the c0 tick of step 2 only
  wire b3  = (g != 3);               // sampled false at the c1 tick of step 4 only
  wire b1  = (g != 1);               // sampled false at a tick of step 2 only
  wire d5  = (g != 5);               // sampled false at a tick of step 6 only
  wire d9  = (g != 9);               // sampled false at a tick of step 10 only

  n_strict: assert property (@(posedge c0) a |=> @(posedge c1) b3);   // DIS @4
  o_near:   assert property (@(posedge c0) a |-> @(posedge c1) b3);   // DIS @4
  n_ok:     assert property (@(posedge c0) a |=> @(posedge c1) d5);   // PROVED
  // coinciding edges (step 2): |-> checks there, |=> at the next c1b tick
  o_same:   assert property (@(posedge c0) a |-> @(posedge c1b) b1);  // DIS @2
  n_same:   assert property (@(posedge c0) a |=> @(posedge c1b) b1);  // PROVED
  // two-tick consequents (failure reported at the last tick); parenthesized:
  // `@(c) 1 ##1 d` parses as `(@(c) 1) ##1 d` and clock flow into the
  // concatenation is a loud error
  o_seq:    assert property (@(posedge c0) a |-> @(posedge c1) (1 ##1 d5));   // DIS @6
  o_seqs:   assert property (@(posedge c0) a |-> @(posedge c1b) (1 ##1 d5));  // DIS @6
  n_seqs:   assert property (@(posedge c0) a |=> @(posedge c1b) (1 ##1 d9));  // DIS @10

  // .matched: q1 matches at the c1 tick of step 4 only
  sequence q1; @(posedge c1) (g == 4'd3); endsequence
  mt_2:   assert property (@(posedge c0) (g == 4'd1)  |-> !q1.matched);  // PROVED
  mt_8:   assert property (@(posedge c0) (g == 4'd7)  |->  q1.matched);  // PROVED
  mt_12:  assert property (@(posedge c0) (g == 4'd11) |-> !q1.matched);  // PROVED
  mt_bad: assert property (@(posedge c0) (g == 4'd11) |->  q1.matched);  // DIS @12
endmodule
