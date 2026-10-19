// A clocked SEQUENCE declaration used as the whole property (LRM 16.16:
// the clock is inferred from the sequence): `sequence s; @(posedge clk)
// ...; endsequence  assert property (s);`.  sva2check used to stop at the
// sequence's `clocked` node ("not a spec"), leave a placeholder, and ic3
// printed a verdict on it -- the tautology p_taut came out DISPROVED
// (register v3 C1, found through sv-tests chapter 16, 2026-10-19).
module tseqclk(input logic clk, input logic a);
  logic r;
  always_ff @(posedge clk) r <= a;
  sequence s_taut;
    @(posedge clk) $rose(r) || !$rose(r);
  endsequence
  sequence s_bad;
    @(posedge clk) $rose(r);
  endsequence
  sequence s_two;
    @(posedge clk) a ##1 r;
  endsequence
  p_taut: assert property (s_taut);   // PROVED
  p_bad:  assert property (s_bad);    // DIS @0
  p_inl:  assert property (@(posedge clk) $rose(r));   // DIS @0
  p_two:  assert property (s_two);    // DIS @1 (LRM @0: the weak sequence fails once a is low at its first tick; engine reports at the sequence end, known class F5 / register D5)
endmodule
