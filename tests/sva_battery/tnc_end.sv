// b[=n] ends AT the n-th occurrence as well as at every following !b tick:
// b[=n] = b[->n] ##1 !b[*0:$] and the [*0] case is the occurrence itself
// (IEEE 1800-2017 16.9.2).  sva2check used to require a !b tick after the
// last occurrence, so a third b right after the second hid every end and
// ic3 PROVED p_cut (register v3 C4: found by oracle.py, 2026-10-18).
module tnc_end(input logic clk);
  logic [2:0] ctr = 0;
  always_ff @(posedge clk) if (ctr != 3'd7) ctr <= ctr + 3'd1;
  logic b, b2;
  assign b  = (ctr == 3'd1) || (ctr == 3'd2) || (ctr == 3'd3);   // a 3rd b right after the 2nd
  assign b2 = (ctr == 3'd1) || (ctr == 3'd2);                    // exactly two
  p_cut:  assert property (@(posedge clk) (ctr == 3'd1) ##0 b[=2] |-> 1'b0);   // DIS @2
  p_two:  assert property (@(posedge clk) (ctr == 3'd1) ##0 b2[=2] |-> 1'b0);  // DIS @2
  p_goto: assert property (@(posedge clk) (ctr == 3'd1) ##0 b[->2] |-> 1'b0);  // DIS @2
  // the later ends still exist: with exactly two b, [=2] also ends at the
  // !b ticks after the 2nd occurrence
  p_late: assert property (@(posedge clk) (ctr == 3'd1) ##0 b2[=2] |-> (ctr == 3'd2)); // DIS @3
endmodule
