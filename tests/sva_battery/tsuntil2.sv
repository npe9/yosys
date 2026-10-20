// Temporal-q strong until: p s_until qseq == (p until qseq-match) && F
// qseq-match.  q's match pulse (attempted at every tick, the LRM property
// promotion of a sequence) feeds the weak-until monitor's q input and the
// <name>$live twin's A.  run_battery folds the twin into the base verdict.
// p5 regression-tests seq_end's bounded repetition path through the
// s_until twin's F q.  The former p3 (`1 s_until (b[*0:1])`) and p4
// (`always (a |-> (b[*0:1]))`) relied on the empty match of b[*0:1]
// counting as a match at the attempt tick; a sequence used as a property
// must not admit an empty match (LRM 16.13.22), and sva2check now refuses
// both shapes (register v3 E1).
// 2026-10-16 expectations corrected (sva_pbt F1): with a=0 and b=0 at step
// 0 the attempt starting at 0 fails at its own start step (no q match can
// start at 0, p false); DIS @1 was the old monitor's one-step blind spot.
module tsuntil2 (input logic clk, input logic a, input logic b);
  p0: assert property (@(posedge clk) a s_until (b[*2])); // DIS @0
  p1: assert property (@(posedge clk) 1'b1 s_until (b[*1])); // DIS
  p2: assert property (@(posedge clk) a s_until_with (b[*2])); // DIS @0
  p5: assert property (@(posedge clk) 1'b1 s_until (b[*2])); // DIS
endmodule
