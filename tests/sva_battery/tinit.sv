// Concurrent checks in an initial block are ONE attempt, at the first
// clock tick (LRM 16.15.6; `expect`, 16.18, likewise -- it parses only
// unlabeled, so the battery reaches it through sv-tests 16.17).  Two bugs
// made them vacuous (register v3 C1, 2026-10-19):
//   * async2sync gated the compiled check with $initstate, so only step 0
//     was ever checked -- a ##1 failure was never seen;
//   * ic3 read $initstate as constant 0 (SatGen's setInitState was never
//     called), so p_now, p_seq were PROVED with free inputs and the
//     initial assumptions q_x/q_y constrained nothing (p_x0/p_y1 DISPROVED).
// sva2check now compiles them as clocked checks whose single attempt
// starts in the initial state; ic3 models $initstate as a state bit.
module tinit(input logic clk, input logic a, input logic b, input logic x, input logic y);
  logic f = 0, g = 0;
  always_ff @(posedge clk) begin f <= 1'b1; g <= f; end
  // (assumptions whose violation tick is exact: a sequence assumption
  // `x ##1 y` is enforced only at its end, F5, and a one-step BMC trace
  // slips past it)
  initial begin
    q_x: assume property (@(posedge clk) x);
    q_y: assume property (@(posedge clk) x |=> y);
  end
  initial begin
    p_now: assert property (@(posedge clk) a);          // DIS @0
    p_seq: assert property (@(posedge clk) 1 ##1 b);    // DIS @1
    p_ok:  assert property (@(posedge clk) a |-> a);    // PROVED
  end
  p_x0:  assert property (@(posedge clk) !f |-> x);           // PROVED
  p_y1:  assert property (@(posedge clk) (f && !g) |-> y);    // PROVED
  p_xfree: assert property (@(posedge clk) x);                // DIS @1
endmodule
