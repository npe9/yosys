// tpalign under the early form (`-early`; assume/restrict get it by
// default): a sequence reports when its attempt dies, so even a
// fixed-length sequence has no fixed failure offset.  Operands of `or` /
// `implies` / `iff` / `s_eventually` are therefore compiled in the
// deadline form (fixed offset, exact alignment; for an assumption the
// failure tick is late, tagged late-failure-tick).  p_or used to be
// aligned at the deadline offset while reporting early: DIS @5 tagged
// exact (false; attempt 4's right side matches at 5, attempt 5's left
// side holds).  The tagged unaligned pairing then PROVED p_or_bad (false).
// SVA2CHECK-FLAGS: -early
module tpalign_early(input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  p_or:  assert property (@(posedge clk) (t != 4) or ((t == 4) ##1 1)); // PROVED
  p_or_bad: assert property (@(posedge clk) (t != 4) or ((t == 4) ##1 (t == 9))); // DIS @5
endmodule
