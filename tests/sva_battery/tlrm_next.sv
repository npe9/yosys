// IEEE 1800-2009 16.13.10 (nexttime) and 16.13.11 (always): the prose
// examples, transcribed for the battery (register v3 C1, 2026-10-21).
// Two gaps closed with it:
//  (1) `nexttime always a`, `s_nexttime s_eventually a` (the LRM's own p3-p6)
//      were syntax errors: the nexttime / not operand excluded the
//      always / eventually prefix forms.
//  (2) `a |-> nexttime[k] s_eventually b` was "unbounded s_eventually is
//      liveness": the temporal-response lowering now opens the obligation
//      k ticks later.
// The formal model's traces are infinite, so the weak and strong forms
// (nexttime / s_nexttime, always / s_always over a bounded range) agree.
// The n*/w* checks anchor each LRM property at a fixed tick; the *_n twins
// are their failing counterparts.  Liveness disproofs carry no step.
// Trace: a @ 3, b @ 3..5, c from 4 on, d @ 10 only (t saturates at 15).
module tlrm_next (input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic a, b, c, d;
  assign a = t == 3;
  assign b = t >= 3 && t <= 5;
  assign c = t >= 4;
  assign d = t == 10;
  default clocking @(posedge clk); endclocking

  // 16.13.10 p1, p2: a at the next tick
  n1:   assert property (t == 2 |-> nexttime a);                 // PROVED
  n1_n: assert property (t == 3 |-> nexttime a);                 // DIS @4
  n2:   assert property (t == 2 |-> s_nexttime a);               // PROVED
  // p3, p4: a at every tick from the next one
  n3:   assert property (t == 3 |-> nexttime always c);          // PROVED
  n3_n: assert property (t == 2 |-> nexttime always c);          // DIS @3
  n4:   assert property (t == 3 |-> s_nexttime always c);        // PROVED
  // p5, p6: a sometime in the strict future
  n5:   assert property (t == 9 |-> nexttime s_eventually d);    // PROVED
  n5_n: assert property (t == 10 |-> nexttime s_eventually d);   // DIS
  n5_c: assert property (t == 10 |-> s_eventually d);            // PROVED
  n6:   assert property (t == 9 |-> s_nexttime s_eventually d);  // PROVED
  // p7, p8: a at the second future tick
  n7:   assert property (t == 1 |-> nexttime [2] a);             // PROVED
  n7_n: assert property (t == 2 |-> nexttime [2] a);             // DIS @4
  n8:   assert property (t == 1 |-> s_nexttime [2] a);           // PROVED
  n9:   assert property (t == 8 |-> nexttime [2] s_eventually d); // PROVED
  n9_n: assert property (t == 9 |-> nexttime [2] s_eventually d); // DIS

  // 16.13.11 p1: if a then b at the next tick, c at every tick after that
  w1:   assert property (a ##1 b |=> always c);                  // PROVED
  // p2, p3: a at each of the 2nd..5th ticks after the start
  w2:   assert property (t == 1 |-> always [2:4] b);             // PROVED
  w2_n: assert property (t == 1 |-> always [2:5] b);             // DIS @6
  w3:   assert property (t == 1 |-> s_always [2:4] b);           // PROVED
  // p4: a at every tick at least 2 after the start
  w4:   assert property (t == 2 |-> always [2:$] c);             // PROVED
  w4_n: assert property (t == 1 |-> always [2:$] c);             // DIS @3
endmodule
