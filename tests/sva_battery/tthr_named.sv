// A named sequence as an operand (`g throughout seq`): the frontend leaves
// a bare identifier wire, and sva2check's length helpers took it as length
// 1 -- a one-tick deadline, so p_named/p_plain were DISPROVED at frame 1
// while the identical inline form proved (register v3 C1, found through
// sv-tests 16.7--sequence-throughout, 2026-10-19).  The _bad twins check
// the fix does not just make everything prove: gb drops at cnt 4, inside
// the windows [t, t+2] of the attempts starting at cnt 2..4.  The
// failure is decided at 4 (LRM); the expectations below are ic3's.
module tthr_named(input logic clk);
  logic [2:0] cnt = 0;
  always_ff @(posedge clk) if (cnt != 3'd7) cnt <= cnt + 3'd1;
  logic req, gnt0, gnt1, g, gb;
  assign req  = 1'b1;
  assign g    = 1'b1;
  assign gb   = cnt != 3'd4;
  assign gnt0 = cnt >= 3'd2;
  assign gnt1 = cnt >= 3'd2;
  sequence seq;
    @(posedge clk) (req ##2 gnt0) intersect (req ##[1:3] gnt1);
  endsequence
  sequence seq_noclk;
    (req ##2 gnt0) intersect (req ##[1:3] gnt1);
  endsequence
  p_named:  assert property (@(posedge clk) g throughout seq);         // PROVED
  p_plain:  assert property (@(posedge clk) g throughout seq_noclk);   // PROVED
  p_inline: assert property (@(posedge clk) g throughout ((req ##2 gnt0) intersect (req ##[1:3] gnt1))); // PROVED
  b_named:  assert property (@(posedge clk) gb throughout seq);        // DIS @5 (LRM @4: the attempt at 2 cannot reach a common end once gb is low at 4; the engine reports at the window deadline, known class F5 / register D5)
  b_plain:  assert property (@(posedge clk) gb throughout seq_noclk);  // DIS @5 (LRM @4: the attempt at 2 cannot reach a common end once gb is low at 4; the engine reports at the window deadline, known class F5 / register D5)
  b_inline: assert property (@(posedge clk) gb throughout ((req ##2 gnt0) intersect (req ##[1:3] gnt1))); // DIS @5 (LRM @4: the attempt at 2 cannot reach a common end once gb is low at 4; the engine reports at the window deadline, known class F5 / register D5)
endmodule
