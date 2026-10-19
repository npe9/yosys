module t9 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b, c;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd12);
  assign c = (ctr == 8'd11);

  sequence s_impl(x, y);
    x ##1 y;
  endsequence

  property p_impl(x);
    @(posedge clk) x |=> c;
  endproperty

  property p_loc;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> (ctr == v + 8'd1);
  endproperty

  property p_locb;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> (ctr == v + 8'd2);
  endproperty

  // (a ##2 b) intersect (c ##1 b) never matches: both operands start at
  // the same tick, and a (ctr 10) and c (ctr 11) are never high together,
  // so p_int_ok and p_int_b hold vacuously (LRM 16.9.6).  p_int_b was
  // expected DIS @12, which encoded the cross-attempt pairing fixed in
  // register v3 C1 (2026-10-19): attempt 10's a ##2 b end at 12 paired with
  // attempt 11's c ##1 b end at 12.
  p_int_ok:   assert property (@(posedge clk) (a ##2 b) intersect (c ##1 b) |-> (ctr == 8'd12)); // PROVED
  p_int_b:    assert property (@(posedge clk) (a ##2 b) intersect (c ##1 b) |-> (ctr == 8'd13)); // PROVED
  p_seq_ok:   assert property (@(posedge clk) s_impl(a, c) |-> (ctr == 8'd11));  // PROVED
  p_seq_b:    assert property (@(posedge clk) s_impl(a, c) |-> (ctr == 8'd10));  // DIS @11
  p_prop_ok:  assert property (p_impl(a));                                       // PROVED
  p_prop_b:   assert property (p_impl(b));                                       // DIS @13
  p_loc_ok:   assert property (p_loc);                                           // PROVED
  p_loc_b:    assert property (p_locb);                                          // DIS @11
  p_with_ok:  assert property (@(posedge clk) (a within (a ##2 b)) |-> (ctr == 8'd12)); // PROVED
  p_with_b:   assert property (@(posedge clk) (c within (a ##2 b)) |-> (ctr == 8'd13)); // DIS @12
  p_weq_ok:   assert property (@(posedge clk) (ctr ==? 8'hff) |-> (ctr == 8'd255));     // PROVED
  p_weq_b:    assert property (@(posedge clk) (ctr ==? 8'hxf) |-> (ctr == 8'd15));      // DIS @31
endmodule
