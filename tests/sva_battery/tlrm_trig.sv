// IEEE 1800-2009 16.9.11 (detecting the end point of a sequence):
// the prose examples, transcribed for the battery (register v3 C1, prose
// half, 2026-10-21).  `e2(ready,proc1,proc2).triggered` (the LRM's rule2)
// was a syntax error until the parser rewrote it the way the LRM says it
// reads: "rule2 is equivalent to rule2a" (a hidden argument-free
// declaration whose body is the instance).
// Part 1, fixed trace (signals are functions of the tick counter t):
//   ready @ 1 (rises at 1), proc1 @ 2, proc2 @ 3: e1 runs 1..3
//   reset @ 1, inst @ 2, branch_back @ 4
// "e1 must match one clock tick after inst" (it ENDS at 3, inst at 2)
// "If the method triggered is replaced with an instance of sequence e1, a
// match of e1 must start one clock tick after inst" (it would start at 3).
// Part 2, free inputs (x*): rule2 == rule2a, and rule2 == rule (the
// actuals are e1's own signals), both directions for one attempt at t == 1.
module tlrm_trig(input logic sysclk, input logic xready, xproc1, xproc2, xreset, xinst, xbb);
  logic [3:0] t = 0;
  always_ff @(posedge sysclk) if (t != 4'd15) t <= t + 4'd1;
  logic ready, proc1, proc2, reset, inst, branch_back, st;
  assign ready = t == 1;
  assign proc1 = t == 2;
  assign proc2 = t == 3;
  assign reset = t == 1;
  assign inst = t == 2;
  assign branch_back = t == 4;
  assign st = t == 1;

  sequence e1; @(posedge sysclk) $rose(ready) ##1 proc1 ##1 proc2; endsequence
  sequence rule; @(posedge sysclk) reset ##1 inst ##1 e1.triggered ##1 branch_back; endsequence
  sequence rule_inst; @(posedge sysclk) reset ##1 inst ##1 e1 ##1 branch_back; endsequence

  t_end:   assert property (@(posedge sysclk) t == 3 |-> e1.triggered);  // PROVED
  t_rule:  assert property (@(posedge sysclk) t == 1 |-> rule);          // PROVED (ends at 4)
  t_trig4: assert property (@(posedge sysclk) t == 4 |-> rule.triggered); // PROVED
  t_inst:  assert property (@(posedge sysclk) t == 1 |-> rule_inst);     // DIS @6 (LRM @3: $rose(ready) is false at 3; the engine reports at the consequent deadline, known class F5 / register D5)
  t_none:  assert property (@(posedge sysclk) t == 2 |-> e1.triggered);  // DIS @2
  // the method on an instance whose body samples a formal ($rose(a)): the
  // hidden declaration must read the actual, not a free formal wire
  // (fork 21b973cf5)
  sequence e1f(a, b, c); @(posedge sysclk) $rose(a) ##1 b ##1 c; endsequence
  t_trigf:  assert property (@(posedge sysclk) t == 3 |-> e1f(ready, proc1, proc2).triggered); // PROVED
  t_trigfn: assert property (@(posedge sysclk) t == 4 |-> e1f(ready, proc1, proc2).triggered); // DIS @4

  // Part 2
  sequence xe1; @(posedge sysclk) $rose(xready) ##1 xproc1 ##1 xproc2; endsequence
  sequence xrule; @(posedge sysclk) st ##0 (xreset ##1 xinst ##1 xe1.triggered ##1 xbb); endsequence
  sequence e2(a,b,c); @(posedge sysclk) $rose(a) ##1 b ##1 c; endsequence
  sequence rule2; @(posedge sysclk) st ##0 (xreset ##1 xinst ##1 e2(xready,xproc1,xproc2).triggered ##1 xbb); endsequence
  sequence e2_instantiated; e2(xready,xproc1,xproc2); endsequence
  sequence rule2a; @(posedge sysclk) st ##0 (xreset ##1 xinst ##1 e2_instantiated.triggered ##1 xbb); endsequence
  x_2_2a: assert property (@(posedge sysclk) rule2 |-> rule2a.triggered); // PROVED
  x_2a_2: assert property (@(posedge sysclk) rule2a |-> rule2.triggered); // PROVED
  x_2_r:  assert property (@(posedge sysclk) rule2 |-> xrule.triggered);  // PROVED
  x_r_2:  assert property (@(posedge sysclk) xrule |-> rule2.triggered);  // PROVED
  // the instance form is NOT equivalent: e2 ends at the third tick of
  // rule2, an instance would start there (xready rising at 1, xproc1 at 2,
  // xproc2 at 3, xreset at 1, xinst at 2, xbb at 4)
  sequence rule2i; @(posedge sysclk) st ##0 (xreset ##1 xinst ##1 e2(xready,xproc1,xproc2) ##1 xbb); endsequence
  x_2_i:  assert property (@(posedge sysclk) rule2 |-> rule2i.triggered); // DIS @4
endmodule
