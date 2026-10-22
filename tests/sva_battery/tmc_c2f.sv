// SVA2CHECK-FLAGS: -clk2fflogic
// PREP-EXTRA-AFTER-MEMORY: clk2fflogic
// sva2check -clk2fflogic (2026-10-22): under the clk2fflogic prep a formal
// step is a global step, not a tick of every clock.  The compiled $check
// used to be evaluated at EVERY step on current values: asserts were
// over-strict (q_g4 failed although the sampled g is never 4), and an
// assumption constrained its inputs between ticks -- a_c2 and the
// immediate a_imm were PROVED (false: x at step 5 is unconstrained) --
// and a cover was reached between ticks (c_y).  With the flag the verdict
// of a tick at step k is the step k-1 value at an edge at k.
// Deterministic, input-free clocks: g counts 0..15 on $global_clock; a
// tick at step k samples step k-1 (rtl_ref checks that with plain
// registers).  c ticks at 4, 8; c1 at 4, 6, 10; c0 at 2, 8; c2 at 6.
module tmc_c2f (input logic x);
  logic [3:0] g = 4'd0;
  always @($global_clock) if (g != 4'd15) g <= g + 4'd1;
  wire c  = (g == 4) || (g == 8);
  wire c0 = (g == 2) || (g == 8);
  wire c1 = (g == 4) || (g == 6) || (g == 10);
  wire c2 = (g == 6);
  wire a  = (g >= 1 && g <= 3);
  wire b  = (g >= 5 && g <= 11);
  wire y  = (g == 5);
  // plain-RTL reference for the sampling convention
  logic r0 = 1'b0, r1 = 1'b1;
  always @(posedge c0) r0 <= a;
  always @(posedge c1) r1 <= b;
  always @($global_clock)
    if (g == 4'd5) rtl_ref: assert (r0 == 1'b1 && r1 == 1'b0);   // PROVED

  m_x: assume property (@(posedge c) !x);                      // x(3), x(7) only
  a_c2: assert property (@(posedge c2) !x);                     // DIS @6
  always @($global_clock)
    a_imm: assert (!x || g != 4'd5);      // DIS @5
  q_g3: assert property (@(posedge c1) g != 4'd3);              // DIS @4
  q_g4: assert property (@(posedge c1) g != 4'd4);              // PROVED
  q_b:  assert property (@(posedge c1) (g == 4'd3) |-> !b);     // PROVED
  q_nb: assert property (@(posedge c0) a |=> !b);               // DIS @8
  c_y:  cover property (@(posedge c) y);                        // UNREACHABLE
  c_b:  cover property (@(posedge c1) b);                       // REACHED @6
  // initial-block checks start at the FIRST tick of their clock (c3:
  // step 3, sampled g = 2), not at step 0: without the flag i_bad was
  // PROVED (evaluated at step 0, where g = 0)
  wire c3 = (g == 3) || (g == 6);
  initial begin
    i_ok: assert property (@(posedge c3) g == 4'd2);            // PROVED
  end
  initial begin
    i_bad: assert property (@(posedge c3) g == 4'd0);           // DIS @3
  end
  // liveness discharges only at ticks: y4 is true at step 4 only, which
  // no tick of c3 samples (approx clk2fflogic-live-ticks)
  wire y4 = (g == 4);
  l_never: assert property (@(posedge c3) s_eventually y4);      // DIS
endmodule
