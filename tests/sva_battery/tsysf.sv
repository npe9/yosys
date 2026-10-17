// System functions inside concurrent assertions (register v3 C3 finding,
// 2026-10-17): every `$f(...)` call in a property expression used to become
// a `$sva` call cell whose output nothing drove, so `!$isunknown(x)`
// (OpenTitan's ASSERT_KNOWN), `$countones(x) <= 1`, `$size(x) == 4` were
// verdicts on a free 1-bit wire (DIS @0, no circuit in the cone).  Now:
// $isunknown is constant 0 (two-state formal), $countones/$onehot/$onehot0
// are a popcount, $bits/$size/$clog2 fold, anything else is an error.
module tsysf(input logic clk, input logic [3:0] v, output logic [3:0] q);
  always_ff @(posedge clk) q <= v;
  localparam int N = 4;
  s_known:  assert property (@(posedge clk) !$isunknown(q));            // PROVED
  s_known2: assert property (@(posedge clk) q[0] |-> !$isunknown(v));   // PROVED
  s_cnt:    assert property (@(posedge clk) $countones(q) <= 4);        // PROVED
  s_cnt1:   assert property (@(posedge clk) $countones(q) == 5);        // DIS @0
  s_cnt3:   assert property (@(posedge clk) $countones(q) != 3);        // DIS @1
  s_oh:     assert property (@(posedge clk) q == 4'b0001 |-> $onehot(q));    // PROVED
  s_oh0:    assert property (@(posedge clk) $onehot0(q) |-> $countones(q) <= 1); // PROVED
  s_ohd:    assert property (@(posedge clk) $onehot(q));                // DIS @0
  s_oh0d:   assert property (@(posedge clk) $onehot0(q));               // DIS @1
  s_size:   assert property (@(posedge clk) $size(q) == 4);             // PROVED
  s_bits:   assert property (@(posedge clk) $bits({q, v}) == 8);        // PROVED
  s_clog:   assert property (@(posedge clk) $clog2(N) == 2);            // PROVED
endmodule
