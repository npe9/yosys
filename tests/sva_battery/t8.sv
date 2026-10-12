module t8 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic [3:0] sat;
  always_ff @(posedge clk) if (sat < 4'd9) sat <= sat + 4'd1;
  logic lvl, p3;
  assign lvl = (ctr >= 8'd3) && (ctr <= 8'd8);
  assign p3  = (ctr[1:0] == 2'b11);

  p_thr_ok:   assert property (@(posedge clk) (!p3) throughout (p3 ##[0:4] p3) |-> (ctr == 8'd99)); // PROVED (vacuous)
  p_thr_b:    assert property (@(posedge clk) lvl throughout (p3 ##[0:4] p3) |-> (ctr >= 8'd5)); // DIS @3
  p_ins_ok:   assert property (@(posedge clk) (ctr inside {[4:6], 9}) |-> (ctr <= 8'd9));  // PROVED
  p_ins_b:    assert property (@(posedge clk) (ctr inside {[4:6], 9}) |-> (ctr != 8'd9));  // DIS @9
  p_nxt_ok:   assert property (@(posedge clk) (ctr==2) |-> nexttime (sat <= 4'd9));        // PROVED
  p_nxt_b:    assert property (@(posedge clk) nexttime (ctr != 8'd42));                   // DIS @42
  p_alw_ok:   assert property (@(posedge clk) (ctr==3) |-> s_always[1:3] (ctr >= 8'd3));   // PROVED
  p_alw_b:    assert property (@(posedge clk) (ctr==3) |-> s_always[1:3] (ctr <= 8'd5));   // DIS @6
  p_ev_ok:    assert property (@(posedge clk) (ctr==3) |-> eventually[2:5] (sat >= 4'd8)); // PROVED
  p_ev_b:     assert property (@(posedge clk) (ctr==3) |-> eventually[2:5] (sat == 4'd3)); // DIS @8
  p_utl_ok:   assert property (@(posedge clk) (ctr==2) |-> ((ctr < 8'd6) until (ctr == 8'd6)));  // PROVED
  p_utl_b:    assert property (@(posedge clk) (ctr==2) |-> ((ctr < 8'd4) until (ctr == 8'd6)));  // DIS @4
  p_utw_ok:   assert property (@(posedge clk) (ctr==2) |-> ((ctr <= 8'd6) until_with (ctr == 8'd6))); // PROVED
  p_utw_b:    assert property (@(posedge clk) (ctr==2) |-> ((ctr < 8'd4) until_with (ctr == 8'd6))); // DIS @4
  p_acc_ok:   assert property (@(posedge clk) (ctr==2) |-> accept_on (ctr == 8'd4) nexttime[3] (ctr == 8'd5));  // PROVED
  p_acc_vac:  assert property (@(posedge clk) (ctr==2) |-> accept_on (ctr == 8'd4) nexttime[3] (ctr == 8'd99)); // PROVED
  p_rej_b:    assert property (@(posedge clk) (ctr==2) |-> reject_on (ctr == 8'd4) nexttime[3] (ctr == 8'd99)); // DIS @4
  p_if_ok:    assert property (@(posedge clk) (ctr==3) |-> if (lvl) (ctr >= 8'd3) else (ctr >= 8'd99)); // PROVED
  p_if_b:     assert property (@(posedge clk) (ctr==3) |-> if (lvl) (ctr >= 8'd4) else (ctr >= 8'd99)); // DIS @3
  p_case_ok:  assert property (@(posedge clk) (ctr==5) |-> case (1'b1) 1: (ctr == 8'd5); default: (ctr == 8'd99); endcase); // PROVED
  p_sneg_ok:  assert property (@(posedge clk) (ctr==3) |-> strong (p3 ##[0:4] p3));       // PROVED
  p_sneg_b:   assert property (@(posedge clk) (ctr==3) |-> strong (p3 ##[0:2] p3));       // PROVED (##0 self-match at 3; ic3 pass hangs on this shape, BMC-verified)
endmodule
