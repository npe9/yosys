// Checker blocks (IEEE 1800-2009 17): checker-local variables with
// sequence local-variable semantics, two instances of the same checker
// with different bindings. The local `got` is renamed per instance, so
// the two copies never share state: u2's copy of p_ok (auto-suffixed
// duplicate label, visible in the ic3 output as bad_p_ok$N) proves where
// u1's disproves, and vice versa for p_b.
// ctr free-runs 0,1,2,...; a pulses at ctr==10.
module tchk4 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a;
  assign a = (ctr == 8'd10);

  checker lc(sig, k);
    logic [7:0] got;
    // local var sampled at sig, read one cycle later
    p_ok:  assert property (@(posedge clk) (sig, got = ctr) |=> (ctr == got + k));      // DIS @11 (u1 k=2)
    p_b:   assert property (@(posedge clk) (sig, got = ctr) |=> (ctr == got + k + 8'd1)); // DIS @11
    p2_ok: assert property (@(posedge clk) (sig, got = ctr) |=> (ctr > got));          // PROVED
  endchecker

  lc u1 (.sig(a), .k(8'd2));  // keyed copies (bare labels)
  lc u2 (.sig(a), .k(8'd1));  // suffixed copies: p_ok$N PROVED, p_b$N DIS @11, p2_ok$N PROVED
endmodule
