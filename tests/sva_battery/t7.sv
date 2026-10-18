module t7 (input logic clk);
  // IC3-BMC: 300  (ctr-wrap CEXes sit at depth 256/259; the bounded
  // reachability pre-pass finds them in ~300 cheap solves instead of
  // ~256 IC3 frames.  p_goto_ok/p_eq_ok wrap because a concurrent
  // assertion attempts at every tick: with occ = occurrences so far,
  // s[->m:n] matches at every occurrence from the m-th on (the anchored
  // k-in-[m,n] window is existentially covered by attempts -- e.g. the
  // tick-259 match is a k=3 match for the attempt at tick 247), so the
  // first wrap state (ctr==3 at tick 259) violates p_goto_ok.)
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
  p_goto_b:   assert property (@(posedge clk) p3[->2] |-> (ctr >= 8'd8));   // DIS @7
  // p3 at ctr 3, 7, 11, ...: p3[=2] ends at the 2nd occurrence (ctr 7,
  // the [*0] case of the !p3 tail, LRM 16.9.2) and at the !p3 ticks after
  // it.  Both fail at 7 (ctr 7 < 8).  These read DIS @256 / DIS @8 until
  // the C4 oracle refuted them (2026-10-18): the monitor skipped the end
  // at the occurrence itself.
  p_eq_ok:    assert property (@(posedge clk) p3[=2] |-> (ctr >= 8'd8));    // DIS @7
  p_eq_b:     assert property (@(posedge clk) p3[=2] |-> (ctr >= 8'd9));    // DIS @7
  p_unb_ok:   assert property (@(posedge clk) (ctr==3) ##[*] (ctr==200) |-> (ctr == 8'd200)); // PROVED
  p_unb_b:    assert property (@(posedge clk) (ctr==3) ##[*] (ctr==200) |-> (ctr == 8'd199)); // DIS @200
  p_ssq_ok:   assert property (@(posedge clk) (p3 ##[0:4] p3) |-> (ctr >= 8'd3)); // PROVED
  p_ssq_b:    assert property (@(posedge clk) (p3 ##[0:4] p3) |-> (ctr >= 8'd4)); // DIS (match (3,3)@3)
endmodule
