// Register v3 B5 (verdict hygiene): a cover property is reported as
// NOT-CHECKED by the ic3 leg (D4 will make it reachability), never folded
// into a PROVED count; a constant-body proof prints as PROVED(const).
module tb5_cover(input logic clk, input logic a, output logic [3:0] q);
  always_ff @(posedge clk) q <= q + {3'd0, a};
  p_mono:  assert property (@(posedge clk) a |=> q != $past(q));  // PROVED
  p_known: assert property (@(posedge clk) !$isunknown(q));        // PROVED
  c_wrap:  cover property (@(posedge clk) q == 4'd15 ##1 q == 4'd0); // NOT-CHECKED
endmodule
