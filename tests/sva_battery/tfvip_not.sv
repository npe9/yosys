// The AXI4 FVIP's exclusive-access check (2026-10-21):
//   property exclusive_wr_rd_simultaneously;
//     (not (exclusive_write_read(AWVALID, AWID, ...)));   // ##0 fusions
//   endproperty
//   if (...) begin  ap_...: assert property (exclusive_wr_rd_simultaneously);
// Two bugs: (1) the bare name inside a generate scope became an undriven
// wire `genblk1.exclusive_wr_rd_simultaneously` and was "checked" (tagged
// exact) as a FREE input -- the previous build disproves n_ok below at
// step 0 for that reason; (2) once the name resolves, `not` of a
// sequence INSTANCE was "not a safety property" (only sequence operators
// counted as sequence-shaped).  `not` of a bounded sequence fails exactly
// when the sequence matches.  (3) found pinning this: a property named
// by TWO assertions (g.n_ok and n_ok2; n_bad and n_bad2) aborted with
// dict::at or "sva_op on non-$sva cell" (any build): the first check
// dropped the declaration cell with its consumed tree.  Declaration cells
// are protected like declaration bodies now.
// Trace: a @ 2, b @ 3 (never together); c @ 2.
module tfvip_not (input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic a, b, c, rst_n;
  assign a = t == 2;
  assign b = t == 3;
  assign c = t == 2;
  assign rst_n = 1'b1;
  default clocking fpv_clk @(posedge clk); endclocking
  default disable iff (!rst_n);

  sequence both(x, y); x ##0 y; endsequence
  sequence then1(x, y); x ##1 y; endsequence
  property no_ab; (not (both(a, b))); endproperty
  property no_ac; (not (both(a, c))); endproperty
  property no_ab1; (not (then1(a, b))); endproperty

  if (1) begin: g
    n_ok:  assert property (no_ab);    // holds (g.n_ok, read by hand)
  end
  n_ok2:  assert property (no_ab);     // PROVED
  n_bad:  assert property (no_ac);     // DIS @2
  n_bad1: assert property (no_ab1);    // DIS @3
  n_bad2: assert property (no_ac);     // DIS @2
endmodule
