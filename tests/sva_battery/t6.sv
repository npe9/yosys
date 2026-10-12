module t6 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b, c, d;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd12);
  assign c = (ctr == 8'd11);
  assign d = (ctr == 8'd13);

  p_ov_ok:    assert property (@(posedge clk) a |-> (ctr == 8'd10));       // PROVED
  p_ov_bad:   assert property (@(posedge clk) a |-> (ctr == 8'd11));       // DIS @10
  p_no_ok:    assert property (@(posedge clk) a |=> c);                    // PROVED
  p_no_bad:   assert property (@(posedge clk) a |=> b);                    // DIS @11
  p_chain2:   assert property (@(posedge clk) a ##2 b |-> (ctr == 8'd12)); // PROVED
  p_chain2b:  assert property (@(posedge clk) a ##2 b |-> d);              // DIS @12
  p_range:    assert property (@(posedge clk) a ##[1:3] b |-> (ctr == 8'd12)); // PROVED
  p_rangeb:   assert property (@(posedge clk) a ##[1:2] c |-> d);          // DIS @11
  p_star:     assert property (@(posedge clk) a ##[*] b |-> (ctr == 8'd12)); // PROVED
  p_pl:       assert property (@(posedge clk) a ##[+] b |-> (ctr == 8'd12)); // PROVED
  p_nested:   assert property (@(posedge clk) a |-> (a |=> (ctr == 8'd11)));  // PROVED
  p_nestedb:  assert property (@(posedge clk) a |-> (a |=> (ctr == 8'd12)));  // DIS @11
  p_strong_ok:  assert property (@(posedge clk) a |=> c ##1 (ctr == 8'd12)); // PROVED
  p_strong_b:   assert property (@(posedge clk) a |=> b ##2 d);            // DIS ~15
  p_not:      assert property (@(posedge clk) not (ctr == 8'd7));          // DIS @7
endmodule
