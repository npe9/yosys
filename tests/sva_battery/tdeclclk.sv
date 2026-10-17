// A named property WITHOUT its own clock, asserted under an outer clock:
//   assert property (@(posedge clk) q)
// must compile q's body as the property (LRM 16.12: the clock of the
// assert applies to the referenced property).  Until 2026-10-17 this form
// was lowered to !q on the undriven name wire and "converted" without a
// word (p_bad would have been DIS @0 and p_ok DIS as well); the bare
// form `assert property (q)` with no clock anywhere is still (rightly) a
// "no usable sampling clock" skip.
module tdeclclk (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) begin
    if (ctr < 8'd100)
      ctr <= ctr + 8'd1;
  end
  property q_ok;  ctr == 8'd10 |-> ##1 ctr == 8'd11; endproperty
  property q_bad; ctr == 8'd10 |-> ##1 ctr == 8'd12; endproperty
  property q_w;   ctr == 8'd20 |-> weak(##[1:2] ctr == 8'd21); endproperty
  property q_loc; logic [7:0] x; (ctr == 8'd30, x = ctr) |-> ##2 ctr == x + 8'd2; endproperty
  property q_locb; logic [7:0] x; (ctr == 8'd30, x = ctr) |-> ##2 ctr == x + 8'd3; endproperty
  p_ok:   assert property (@(posedge clk) q_ok);    // PROVED
  p_bad:  assert property (@(posedge clk) q_bad);   // DIS @11
  p_w:    assert property (@(posedge clk) q_w);     // PROVED
  p_loc:  assert property (@(posedge clk) q_loc);   // PROVED
  p_locb: assert property (@(posedge clk) q_locb);  // DIS @32
endmodule
