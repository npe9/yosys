// Temporal-q strong until: p s_until qseq == (p until qseq-match) && F
// qseq-match.  q's match pulse (attempted at every tick, the LRM property
// promotion of a sequence) feeds the weak-until monitor's q input and the
// <name>$live twin's A.  run_battery folds the twin into the base verdict.
// p4/p5 regression-test the [*0:N] empty-match tap in seq_end's bounded
// repetition path (rep_chain_end taps copies >= 1 only; the empty parse's
// end at the start tick was missing, found via the s_until twin's F q).
// 2026-10-16 expectations corrected (sva_pbt F1): with a=0 and b=0 at step
// 0 the attempt starting at 0 fails at its own start step (no q match can
// start at 0, p false); DIS @1 was the old monitor's one-step blind spot.
module tsuntil2 (input logic clk, input logic a, input logic b);
  p0: assert property (@(posedge clk) a s_until (b[*2])); // DIS @0
  p1: assert property (@(posedge clk) 1'b1 s_until (b[*1])); // DIS
  p2: assert property (@(posedge clk) a s_until_with (b[*2])); // DIS @0
  p3: assert property (@(posedge clk) 1'b1 s_until (b[*0:1])); // PROVED
  p4: assert property (@(posedge clk) always (a |-> (b[*0:1]))); // PROVED
  p5: assert property (@(posedge clk) 1'b1 s_until (b[*2])); // DIS
endmodule
