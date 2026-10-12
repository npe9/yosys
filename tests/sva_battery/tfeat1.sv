module tfeat1(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic [3:0] sat;
  always_ff @(posedge clk) if (sat < 4'd9) sat <= sat + 4'd1;
  logic a, b, e;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd11);
  assign e = (ctr == 8'd11) || (ctr == 8'd13);

  // first_match: plain antecedent `a ##[1:3] e` matches ending 11 and 13;
  // first_match keeps only the earliest (11), so ctr<=11 holds PROVED
  // (plain would DIS@13), and ctr<=10 must DIS@11.
  p_fm_ok:  assert property (@(posedge clk) first_match(a ##[1:3] e) |-> (ctr <= 8'd11)); // PROVED
  p_fm_bad: assert property (@(posedge clk) first_match(a ##[1:3] e) |-> (ctr <= 8'd10)); // DIS @11

  // iff: equivalence both ways.
  p_iff_ok:  assert property (@(posedge clk) (ctr == 8'd10) iff a);       // PROVED
  p_iff_bad: assert property (@(posedge clk) (ctr == 8'd11) iff a);       // DIS @10

  // implies: boolean implication at property level.
  p_imp_ok:  assert property (@(posedge clk) a implies (ctr == 8'd10));   // PROVED
  p_imp_bad: assert property (@(posedge clk) a implies (ctr == 8'd11));   // DIS @10

  // sync_accept_on / sync_reject_on: synchronous abort.
  p_sacc_ok: assert property (@(posedge clk) (ctr==2) |-> sync_accept_on(ctr == 8'd4) nexttime[3] (ctr == 8'd99)); // PROVED (abort at 4 before check at 5)
  p_srej_b:  assert property (@(posedge clk) (ctr==2) |-> sync_reject_on(ctr == 8'd4) (ctr != 8'd99));             // DIS @4

  // weak/strong on bounded sequences in consequent position.  weak and
  // strong coincide on the infinite traces model checking explores (LRM
  // 16.12.1), so a non-matching weak sequence is disproved just like strong.
  p_weak_ok: assert property (@(posedge clk) a |-> weak ((ctr==10) ##1 (ctr==11)));             // PROVED (match)
  p_str_ok:  assert property (@(posedge clk) a |-> strong ((ctr==10) ##1 (ctr==11)));          // PROVED (match)
  p_weak_b:  assert property (@(posedge clk) a |=> weak ((ctr==11) ##1 (ctr == 8'd99)));       // DIS @12 (no match by expiry)
  p_str_b:   assert property (@(posedge clk) a |=> strong ((ctr==11) ##1 (ctr == 8'd99)));     // DIS @12

  // s_nexttime / s_eventually (strong variants, bounded).
  p_snxt_ok: assert property (@(posedge clk) (ctr==3) |-> s_nexttime[2] (ctr == 8'd5));       // PROVED
  p_sev_ok:  assert property (@(posedge clk) (ctr==3) |-> s_eventually[1:4] (sat >= 4'd7));   // PROVED
  p_sev_b:   assert property (@(posedge clk) (ctr==3) |-> s_eventually[1:3] (sat >= 4'd7));   // DIS @6
endmodule
