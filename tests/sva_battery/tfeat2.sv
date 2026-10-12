module tfeat2(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b, fb, hold;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd11);
  assign fb = (ctr == 8'd10);   // rises at 10, falls at 11
  assign hold = (ctr < 8'd20);  // constant 1 through the region of interest

  sequence s5;
    @(posedge clk) a ##1 b;
  endsequence

  // $rose / $fell
  p_rose_ok: assert property (@(posedge clk) a |-> $rose(a));            // PROVED (0->1 at 10)
  p_rose_b:  assert property (@(posedge clk) b |-> $rose(a));            // DIS @11 (a was 1, now 0)
  p_fell_ok: assert property (@(posedge clk) (ctr==11) |-> $fell(fb));   // PROVED (1->0 at 11)
  p_fell_b:  assert property (@(posedge clk) (ctr==10) |-> $fell(fb));   // DIS @10

  // $stable / $changed
  p_stab_ok: assert property (@(posedge clk) (ctr==5) |-> $stable(hold)); // PROVED
  p_stab_b:  assert property (@(posedge clk) (ctr==5) |-> $stable(ctr));  // DIS @5
  p_chg_ok:  assert property (@(posedge clk) (ctr==11) |-> $changed(fb)); // PROVED
  p_chg_b:   assert property (@(posedge clk) (ctr==12) |-> $changed(fb)); // DIS @12

  // $past / $sampled
  p_past_ok: assert property (@(posedge clk) (ctr==10) |-> ($past(ctr, 2) == 8'd8));  // PROVED
  p_past_b:  assert property (@(posedge clk) (ctr==10) |-> ($past(ctr, 1) == 8'd8));  // DIS @10 (past=9)
  p_smp_ok:  assert property (@(posedge clk) (ctr==10) |-> ($sampled(ctr) == 8'd10)); // PROVED

  // .triggered / .matched
  p_trg_ok:  assert property (@(posedge clk) (ctr==11) |-> s5.triggered);              // PROVED (match a@10,b@11 ends at 11)
  p_trg_b:   assert property (@(posedge clk) (ctr==12) |-> s5.triggered);              // DIS @12
  p_mtc_ok:  assert property (@(posedge clk) (ctr==11) |-> (s5.matched or (ctr == 8'd99))); // PROVED
  p_mtc_b:   assert property (@(posedge clk) (ctr==12) |-> (s5.matched or (ctr == 8'd99))); // DIS @12
endmodule
