// Nested-temporal response: antecedent |->/|=> (s_eventually b) / (weak
// eventually b) with b a plain boolean.  sva2check lowers it to a single
// pending-bit FF on the property clock (D = !b && (open || pend), open =
// antecedent end, |=> delayed one tick onto the first window tick) and a
// FLAVOR "liveness" $check with A = !pend || b (ic3: bad = pend && !b),
// proved by the native gfp engine.  All legs are the single liveness
// check, so DIS expectations carry no @N (liveness disproofs are depth 0
// by design).  ctr runs 0,1,2,...; once gates the one-shot pulses so a,
// b4 and blate fire exactly once (ticks 4, 5, 6) instead of every wrap.
// IC3-TIMEOUT: 300
module tresp1 (input logic clk);
  logic [7:0] ctr;
  logic once, a, b4, blate;
  always_ff @(posedge clk) begin
    ctr   <= ctr + 8'd1;
    a     <= ctr == 8'd3 && !once;
    b4    <= ctr == 8'd4 && !once;
    blate <= ctr == 8'd5 && !once;
    if (ctr == 8'd6)
      once <= 1'b1;
  end

  // one-shot antecedent at 4, b one tick later: the |=> window [5,inf)
  // sees b.
  r1: assert property (@(posedge clk) a |=> s_eventually blate);  // PROVED
  // one-shot antecedent, b never (weak eventually form): pend from 6 on.
  r2: assert property (@(posedge clk) a |=> eventually 1'b0);     // DIS
  // antecedent every 16 ticks, b every 16 ticks four later: every window
  // still sees b after the antecedent's single occurrence wraps around.
  r3: assert property (@(posedge clk) (ctr[3:0] == 4'd0) |=> s_eventually (ctr[3:0] == 4'd4));  // PROVED
  // antecedent every other tick, b on the alternating ticks: the open
  // pulse and b coincide every odd tick (set/clear interleave stress).
  r4: assert property (@(posedge clk) (ctr[0] == 1'b0) |=> s_eventually (ctr[0] == 1'b1));      // PROVED
  // antecedent and b the same one-shot: under |=> the window [5,inf)
  // never sees b (it fired at 4, strictly before the window).
  r5: assert property (@(posedge clk) a |=> s_eventually a);      // DIS
  // |-> coincidence: b on the antecedent end tick satisfies the
  // overlapped window [4,inf) -- a same-tick b must clear the bit.
  r6: assert property (@(posedge clk) a |-> s_eventually a);      // PROVED
  // |=> coincidence: b exactly one tick after the antecedent, on the
  // first window tick [5,inf): the open pulse lands there and b clears.
  r7: assert property (@(posedge clk) a |=> eventually b4);       // PROVED
endmodule
