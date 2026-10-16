// Conductor (prove.py) leg pin: a depth-0 counterexample, an ordinary
// depth-1 counterexample, and a trivial proof in one file, so the
// first-to-finish merge and per-property engine attribution are all
// exercised.  Which lane settles p_fast is a wall-clock race (ic3's
// -kind base case matches one BMC round), so the gate checks verdicts
// and attribution plumbing, not the winner.
module tprove0(input logic clk, input logic a, input logic b);
  p_fast: assert property (@(posedge clk) a |-> !a);          // DIS @0
  p_dis1: assert property (@(posedge clk) a ##1 b |-> !b);    // DIS @1
  p_ok:   assert property (@(posedge clk) a |-> a);           // PROVED
endmodule
