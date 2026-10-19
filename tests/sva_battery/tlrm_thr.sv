// IEEE 1800-2009 16.9.9 Figures 16-12 / 16-13 (throughout) and 16.9.10
// (within, on the trace of Figure 16-13), transcribed for the battery
// (register v3 C1, LRM-example half, 2026-10-19).  Sampled values read off
// the figures; an edge drawn on tick k's line is the value sampled at k
// (the prose fixes the convention: "burst_mode is high at clock tick 1 and
// low at clock tick 2", "at clock tick 9 burst_mode becomes high"):
//   Figure 16-12: burst_mode high @1, low @2..8, high @9..
//                 irdy high @1 2, low @3..11, high @12..
//                 trdy high @1..3, low @4..10, high @11..
//   Figure 16-13: as 16-12 except burst_mode low @2..11, high @12..
// Step k is LRM tick k; tick 0 repeats tick 1's values.
// The plain ic3 generalizer thrashes on the PROVED pair over this closed
// 4-bit-counter trace (f13_match: > 5 min alone, > 25 min for the file
// under load); CTG generalization proves it in ~2 s.
// IC3-CTG: 1
module tlrm_thr(input logic mclk);
  logic [3:0] t = 0;
  always_ff @(posedge mclk) if (t != 4'd15) t <= t + 4'd1;
  logic bm12, bm13, irdy, trdy;
  assign bm12 = t <= 1 || t >= 9;
  assign bm13 = t <= 1 || t >= 12;
  assign irdy = t <= 2 || t >= 12;
  assign trdy = t <= 3 || t >= 11;
  sequence burst_rule1_f12;
    @(posedge mclk)
      $fell(bm12) ##0
      ((!bm12) throughout (##2 ((trdy==0)&&(irdy==0)) [*7]));
  endsequence
  sequence burst_rule1_f13;
    @(posedge mclk)
      $fell(bm13) ##0
      ((!bm13) throughout (##2 ((trdy==0)&&(irdy==0)) [*7]));
  endsequence
  // 16-12 "at clock tick 9 burst_mode becomes high, thereby failing to
  // match according to the rules for throughout"
  f12_fail: assert property (@(posedge mclk) t == 2 |-> burst_rule1_f12); // DIS @10 (LRM @9: the LRM text and figure put the failure at tick 9; the engine reports at the end of the consequent window, known class F5 / register D5)
  // 16-13 "there would be a match of burst_rule1 from clock tick 2 to
  // clock tick 10"
  f13_match: assert property (@(posedge mclk) t == 2 |-> burst_rule1_f13 ##0 t == 10); // PROVED
  // 16.9.10 "!trdy[*7] within ($fell(irdy) ##1 !irdy[*8]) matches from
  // clock tick 3 to clock tick 11 on the trace shown in Figure 16-13"
  f13_within: assert property (@(posedge mclk) t == 3 |-> (!trdy[*7] within ($fell(irdy) ##1 !irdy[*8])) ##0 t == 11); // PROVED
endmodule
