// Positive controls for the circuit-mutation leg (conformance/run_mutation.py,
// register v3 C3).  Each property states the class the leg must give it;
// a classifier that reports "0 SURVIVED" on the corpus is only believed
// because it reports SURVIVED and FREE here.  Not part of the green
// battery (x-prefix): run_mutation.py --battery checks these lines.
module xmut(input logic clk, input logic a, input logic b);
  logic [3:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 4'd1;
  logic r;
  always_ff @(posedge clk) r <= a;
  logic [3:0] q;
  always_ff @(posedge clk) q <= q + {3'd0, b};

  // a tautology over a register (4-bit ctr is always below 16): no mutation
  // of the circuit can falsify it.  (`r || !r` is constant-folded before
  // the engine and lands in FREE instead, honestly: nothing feeds it.)
  p_taut: assert property (@(posedge clk) a |-> (ctr < 5'd16));       // PROVED  MUT: SURVIVED
  // depends on primary inputs only: no circuit cell in its cone
  p_free: assert property (@(posedge clk) a |-> a);                   // PROVED  MUT: FREE
  // real properties of the circuit: a mutant in their cone disproves them
  p_r:    assert property (@(posedge clk) a |=> r);                   // PROVED  MUT: CAUGHT
  p_ctr:  assert property (@(posedge clk) ctr == 4'd3 |=> ctr == 4'd4); // PROVED  MUT: CAUGHT
  p_q:    assert property (@(posedge clk) !b |=> $stable(q));         // PROVED  MUT: CAUGHT
  // disproved on the circuit: a mutant of the counter makes the cex vanish
  p_dis:  assert property (@(posedge clk) ctr != 4'd15);              // DIS @15  MUT: HEALED
  // disproved on inputs alone: nothing in the circuit can heal it
  p_dis_free: assert property (@(posedge clk) a |-> b);               // DIS @0  MUT: FREE-DIS
endmodule
