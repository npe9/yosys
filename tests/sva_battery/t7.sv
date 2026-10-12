module t7 (input logic clk);
  // IC3-BMC: 300  (ctr-wrap CEXes sit at depth 256/259; the bounded
  // reachability pre-pass finds them in ~300 cheap solves instead of
  // ~256 IC3 frames)
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic lvl, p3;
  assign lvl = (ctr >= 8'd3) && (ctr <= 8'd8);
  assign p3  = (ctr[1:0] == 2'b11);

  p_star2_ok: assert property (@(posedge clk) lvl[*2] |-> lvl);            // PROVED
  p_star2_b:  assert property (@(posedge clk) lvl[*2] |-> (ctr <= 8'd7));  // DIS @8
  p_star3_ok: assert property (@(posedge clk) lvl[*3] |-> (ctr <= 8'd8));  // PROVED
  p_star3_b:  assert property (@(posedge clk) lvl[*3] |-> (ctr <= 8'd7));  // DIS @8
  p_rng_ok:   assert property (@(posedge clk) lvl[*2:4] |-> (ctr <= 8'd8)); // PROVED
  p_rng_b:    assert property (@(posedge clk) lvl[*2:4] |-> (ctr <= 8'd7)); // DIS @8
  p_goto_ok:  assert property (@(posedge clk) p3[->2] |-> (ctr >= 8'd7));   // DIS @259
  p_goto_b:   assert property (@(posedge clk) p3[->2] |-> (ctr >= 8'd8));   // DIS @259
  p_eq_ok:    assert property (@(posedge clk) p3[=2] |-> (ctr >= 8'd8));    // DIS @256
  p_eq_b:     assert property (@(posedge clk) p3[=2] |-> (ctr >= 8'd9));    // DIS @256
  p_unb_ok:   assert property (@(posedge clk) (ctr==3) ##[*] (ctr==200) |-> (ctr == 8'd200)); // PROVED
  p_unb_b:    assert property (@(posedge clk) (ctr==3) ##[*] (ctr==200) |-> (ctr == 8'd199)); // DIS @200
  p_ssq_ok:   assert property (@(posedge clk) (p3 ##[0:4] p3) |-> (ctr >= 8'd3)); // PROVED
  p_ssq_b:    assert property (@(posedge clk) (p3 ##[0:4] p3) |-> (ctr >= 8'd4)); // DIS (match (3,3)@3)
endmodule
