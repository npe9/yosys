// Unbounded sequences in property position, weak semantics (IEEE 1800-2017
// 16.12.2): an assert's sequence fails only when a finite prefix leaves the
// attempt no thread that could still match (2026-10-23, register v3 D2).
// They used to be refused ("no finite bound").  ctr = BMC step; each
// property starts once (ctr == 2) unless noted.  Expected depths follow
// from the LRM semantics by hand:
//   goto:   c throughout b[->1] fails at the first tick c is low while
//           waiting (the b tick included); without c it never fails.
//   goto+S: after the first b, the bounded tail S must match.
//   rep:    x[*m:$] ##k y fails once the run is over and every tail
//           spawned by it missed y.
//   gap:    P ##[m:$] Q fails only if P fails.
module tweak_unb (input logic clk, input logic xi, input logic yi, input logic zi);
  logic [3:0] ctr = 4'd0;
  always_ff @(posedge clk) if (ctr != 4'd15) ctr <= ctr + 4'd1;
  wire s  = (ctr == 4'd2);
  wire b6 = (ctr == 4'd6);
  wire never = 1'b0;

  g_hold:  assert property (@(posedge clk) s |-> (ctr <= 4'd6) throughout b6[->1]);   // PROVED
  g_drop:  assert property (@(posedge clk) s |-> (ctr <= 4'd4) throughout b6[->1]);   // DIS @5
  g_attb:  assert property (@(posedge clk) s |-> (ctr <= 4'd5) throughout b6[->1]);   // DIS @6
  g_never: assert property (@(posedge clk) s |-> never[->1]);                          // PROVED
  g_ovl:   assert property (@(posedge clk) (ctr >= 4'd2 && ctr <= 4'd4) |-> (ctr <= 4'd5) throughout b6[->1]); // DIS @6
  g_ovlok: assert property (@(posedge clk) (ctr >= 4'd2 && ctr <= 4'd4) |-> (ctr <= 4'd6) throughout b6[->1]); // PROVED

  gs_ok:   assert property (@(posedge clk) s |-> b6[->1] ##1 (ctr == 4'd7));          // PROVED
  gs_bad:  assert property (@(posedge clk) s |-> b6[->1] ##1 (ctr == 4'd8));          // DIS @7
  gs_seq:  assert property (@(posedge clk) s |-> b6[->1] ##2 (ctr == 4'd8) ##1 (ctr == 4'd10)); // DIS @9

  r_triv:  assert property (@(posedge clk) s |-> !(ctr == 4'd9) [*0:$] ##1 (ctr == 4'd9)); // PROVED
  r_miss:  assert property (@(posedge clk) s |-> (ctr >= 4'd2 && ctr <= 4'd5) [+] ##1 (ctr == 4'd9)); // DIS @6
  r_hit:   assert property (@(posedge clk) s |-> (ctr >= 4'd2 && ctr <= 4'd5) [+] ##1 (ctr == 4'd6)); // PROVED
  r0_late: assert property (@(posedge clk) s |-> (ctr == 4'd2 || ctr == 4'd3) [*0:$] ##2 (ctr == 4'd5)); // PROVED
  r0_zero: assert property (@(posedge clk) s |-> (ctr == 4'd2 || ctr == 4'd3) [*0:$] ##2 (ctr == 4'd3)); // PROVED
  r0_miss: assert property (@(posedge clk) s |-> (ctr == 4'd2 || ctr == 4'd3) [*0:$] ##2 never);        // DIS @5
  // two attempts, two runs: the run started at 2 dies at 4 with its tails
  // (at 5, 6) still pending when the attempt at 5 starts a run that never
  // dies; the first attempt fails at 6 (a monitor that let the new start
  // reset the pending tails missed it: sva_pbt seed 1 index 9)
  r_grp:   assert property (@(posedge clk) (ctr == 4'd2 || ctr == 4'd5) |-> ((ctr >= 4'd2 && ctr <= 4'd3) || ctr >= 4'd5) [+] ##3 (ctr == 4'd1)); // DIS @6

  d_ok:    assert property (@(posedge clk) s |-> (ctr == 4'd2) ##[1:$] (ctr == 4'd1)); // PROVED
  d_bad:   assert property (@(posedge clk) s |-> (ctr == 4'd3) ##[1:$] (ctr == 4'd9)); // DIS @2
  // the gap inside the left ## spine (sha3pad): ((P ##[1:$] Q) ##1 R)
  d_spok:  assert property (@(posedge clk) s |-> (ctr == 4'd2) ##[1:$] (ctr == 4'd9) ##1 (ctr == 4'd1)); // PROVED
  d_spbad: assert property (@(posedge clk) s |-> (ctr == 4'd2) ##1 (ctr == 4'd4) ##[1:$] (ctr == 4'd9) ##1 (ctr == 4'd10)); // DIS @3

  // free inputs: the weak goto alone can never fail; with a throughout
  // gate an input low at the attempt tick fails at once
  f_goto:  assert property (@(posedge clk) xi |-> zi[->1]);                          // PROVED
  f_thru:  assert property (@(posedge clk) xi |-> yi throughout zi[->1]);            // DIS @0
endmodule
