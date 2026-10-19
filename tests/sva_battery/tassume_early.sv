// A sequence ASSUMPTION is enforced at the tick its attempt dies, not at
// the sequence's deadline (register v3 D5, sva2check's default for
// assume/restrict).  With the deadline form, a trace whose attempt at
// step 1 is already dead (x low) still satisfied the assumption up to
// step 2, and ic3 -- which looks one frame past the bad state -- returned
// it: p_x was DISPROVED at depth 1, a counterexample the LRM rejects.
// p_y / p_y2 check the assumption constrains no more than it says: y is
// forced only from step 2 on.
module tassume_early(input logic clk, input logic x, input logic y);
  logic [1:0] s = 0;
  always_ff @(posedge clk) if (s != 2'd3) s <= s + 2'd1;
  q_xy: assume property (@(posedge clk) x ##2 y);
  p_x:  assert property (@(posedge clk) x);              // PROVED
  p_y:  assert property (@(posedge clk) y);              // DIS @0
  p_y2: assert property (@(posedge clk) s >= 2'd2 |-> y); // PROVED
endmodule
