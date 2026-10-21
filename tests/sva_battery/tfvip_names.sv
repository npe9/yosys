`default_nettype none
// Bare references to declarations (2026-10-21, AXI4-Stream FVIP):
//  (1) `assert property (using_bus_type_correctly)` under `default_nettype
//      none` was "Identifier ... is implicitly declared": the bare name is
//      how sva2check finds the declaration (an undriven wire named after
//      it), not an implicit net.  A name that matches a sequence /
//      property / let declaration of the module or a package is accepted,
//      at module level and inside generate scopes (no scope-prefixed wire:
//      the declaration is looked up by its bare name).
//  (2) the clock of a leading argument-free clocked sequence instance
//      (`sequence s_a; @(posedge clk) a; endsequence  assert property
//      (s_a |=> b)`, LRM 16.16) was not inferred ("property is not a $sva
//      spec"); d1 is the explicitly clocked twin of q1.
// Trace: a @ 2, b @ 3.
module tfvip_names #(parameter int BUS_TYPE = 1) (input wire clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic a, b;
  assign a = t == 2;
  assign b = t == 3;

  property using_bus_type_correctly; @(posedge clk) !(BUS_TYPE > 1); endproperty
  property p_b_after_a; @(posedge clk) a |=> b; endproperty
  sequence s_a; @(posedge clk) a; endsequence
  sequence s_ab; @(posedge clk) a ##1 b; endsequence

  n_top: assert property (using_bus_type_correctly);   // PROVED
  n_pab: assert property (p_b_after_a);                 // PROVED
  if (BUS_TYPE == 1) begin: setup_checks
    n_gen: assert property (using_bus_type_correctly); // holds (setup_checks.n_gen: scoped, read by hand)
  end

  q1: assert property (s_a |=> b);         // PROVED
  q2: assert property (s_a |-> b);         // DIS @2
  q3: assert property (s_ab |-> b);        // PROVED
  q4: assert property (s_ab |=> b);        // DIS @4
  d1: assert property (@(posedge clk) a |=> b);  // PROVED
endmodule
`default_nettype wire
