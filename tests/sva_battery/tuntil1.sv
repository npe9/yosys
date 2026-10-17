// Strong until family: p s_until q == (p until q) && F q, lowered as the
// weak-until monitor on the main $check plus a "<name>$live" liveness
// twin (F q) proved by ic3's gfp engine.  run_battery folds the twin
// into the base verdict: all legs proved -> PROVED, any leg disproved
// -> DIS (depth from the safety leg when it is the failing one).
// 2026-10-16 expectation corrected (sva_pbt F1): the attempt starting at
// ctr=101 has neither p (ctr<100) nor q (even) at its own start step and
// fails there (LRM 16.12.10); the old monitor could not fire on the start
// step, so a1 looked PROVED.
module tuntil1 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;

  // q = even ctr recurs every 2 cycles; p holds past each q while an
  // attempt can be open (ctr < 100 covers the wrap): both legs prove.
  a1: assert property (@(posedge clk) (ctr < 8'd100) s_until (ctr[0] == 1'b0));       // DIS @101
  // until_with: at ctr==100 p has dropped while q holds; the monitor
  // grants the q cycle itself, then fails on the next tick.
  a2: assert property (@(posedge clk) (ctr < 8'd100) s_until_with (ctr[0] == 1'b0));  // DIS @100 (with: p required at the q step; ctr=100 is even and not < 100)
  // q = 1'b0 never holds: the safety leg (p = 1'b1) is vacuous, the F q
  // twin disproves.  Bare DIS: liveness disproofs carry no step.
  a3: assert property (@(posedge clk) 1'b1 s_until (1'b0));                           // DIS
  // p = 1'b1 with a recurrent q: both legs prove.
  a4: assert property (@(posedge clk) 1'b1 s_until_with (ctr[0] == 1'b0));            // PROVED
endmodule
