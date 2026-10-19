// IEEE 1800-2009 16.13.6 Figures 16-14 / 16-15 / 16-16 (data_end,
// data_end_rule1 with a precondition, data_end_rule2 without), transcribed
// for the battery (register v3 C1, LRM-example half, 2026-10-19).  Sampled
// values read off Figure 16-15 (16-14 and 16-16 show the same trace; an
// edge on tick k's line is the value sampled at k -- "frame toggles high
// at clock tick 7", "irdy transitions to high at clock tick 8"):
//   data_phase low @1, high @2..8, low @9..   irdy high @1..3, low @4..7,
//   high @8..   trdy high throughout          stop high @1..5, low @6..8,
//   high @9..   frame low @1..6, high @7..
// The LRM's `define data_end_exp is written out in place.  Step k is LRM
// tick k; tick 0 repeats tick 1's values.
module tlrm_impl(input logic mclk);
  logic [3:0] t = 0;
  always_ff @(posedge mclk) if (t != 4'd15) t <= t + 4'd1;
  logic data_phase, irdy, trdy, stop, frame;
  assign data_phase = t >= 2 && t <= 8;
  assign irdy  = t <= 3 || t >= 8;
  assign trdy  = 1'b1;
  assign stop  = t <= 5 || t >= 9;
  assign frame = t >= 7;
  // 16-14 "At clock tick 6, data_end is true because stop gets asserted
  // while irdy is asserted" (the attempt at 6 of property data_end)
  de_at6:   assert property (@(posedge mclk) t == 6 |-> (data_phase |-> ((irdy==0) && ($fell(trdy) || $fell(stop))))); // PROVED
  dex_only: assert property (@(posedge mclk) (data_phase && ((irdy==0)&&($fell(trdy)||$fell(stop)))) |-> t == 6); // PROVED
  // 16-15 data_end_rule1 holds: "matching the sequence specification
  // completely for the attempt that began at clock tick 6" (at tick 8)
  rule1:     assert property (@(posedge mclk) (data_phase && ((irdy==0)&&($fell(trdy)||$fell(stop)))) |-> ##[1:2] $rose(frame) ##1 $rose(irdy)); // PROVED
  rule1_end: assert property (@(posedge mclk) t == 6 |-> ##[1:2] $rose(frame) ##1 $rose(irdy) ##0 t == 8); // PROVED
  // 16-16 data_end_rule2 (no precondition), per attempt: "the rising edge
  // of signal frame does not occur at clock tick 2 or 3; therefore, the
  // property fails at clock tick 1.  Similarly, there is a failure at clock
  // ticks 2, 3, and 4.  For attempts starting at clock ticks 5 and 6 ...
  // a match.  All later attempts to match the sequence fail".  The LRM
  // names the failing attempt; the step is the tick the failure is decided
  // (no $rose(frame) in the [1:2] window: attempt k fails at k+2).
  r2_a1: assert property (@(posedge mclk) t == 1 |-> ##[1:2] $rose(frame) ##1 $rose(irdy)); // DIS @4 (LRM @3: no $rose(frame) at 2 or 3 leaves no thread; the engine reports at the end of the consequent window, known class F5 / register D5)
  r2_a4: assert property (@(posedge mclk) t == 4 |-> ##[1:2] $rose(frame) ##1 $rose(irdy)); // DIS @7 (LRM @6: no $rose(frame) at 5 or 6 leaves no thread; the engine reports at the end of the consequent window, known class F5 / register D5)
  r2_a5: assert property (@(posedge mclk) t == 5 |-> ##[1:2] $rose(frame) ##1 $rose(irdy)); // PROVED
  r2_a6: assert property (@(posedge mclk) t == 6 |-> ##[1:2] $rose(frame) ##1 $rose(irdy)); // PROVED
  r2_a7: assert property (@(posedge mclk) t == 7 |-> ##[1:2] $rose(frame) ##1 $rose(irdy)); // DIS @10 (LRM @9: no $rose(frame) at 8 or 9 leaves no thread; the engine reports at the end of the consequent window, known class F5 / register D5)
  r2_a12: assert property (@(posedge mclk) t == 12 |-> ##[1:2] $rose(frame) ##1 $rose(irdy)); // DIS @15 (LRM @14: no $rose(frame) at 13 or 14 leaves no thread; the engine reports at the end of the consequent window, known class F5 / register D5)
endmodule
