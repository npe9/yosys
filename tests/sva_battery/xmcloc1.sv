// Multiclock local-variable scoreboard (v1): a value captured on the write
// clock is checked against data observed on the read clock at the matching
// pop tick.  8-bit data, 2-stage read synchronizer, strong ##[2:3] window.
// Ic3-stress: the 479-bit slot/island state space grinds generalization
// (same class as t7); the 1-bit fixed-delay sibling tmcloc2.sv is the green
// battery case.  Test conventions learned here:
//   * the local captures a REGISTER (mem_v), not a raw input -- registers
//     and SVA captures share the post-edge sample convention;
//   * the antecedent keys on $rose(wr_seen), not the raw input -- a raw
//     input match can fire at a tick whose clk2fflogic edge already
//     sampled pre-input data;
//   * the DUT pushes once (gated by !wr_seen) -- a second push overwrites
//     the checked word before the pop.
//
// PREP-EXTRA-AFTER-MEMORY: clk2fflogic
// x-prefix: ic3-stress (t7-class generalization grind, no proof at 500 s)
// and BMC-heavy (z3 >120 s to bound).  Only the DIS expectations are
// machine-checked here; ok is documented-open (holds by construction of
// the one-shot DUT, cf. the green sibling tmcloc2.sv which proves the
// identical shape at 1-bit ##1 in 51 s).
module xmcloc1(input logic wr_clk, input logic rd_clk,
               input logic push_pulse, input logic [7:0] din);
  logic       wr_seen;
  logic [7:0] mem_v;
  logic       v0, v1, pop;
  logic [7:0] rdata;
  always @(posedge wr_clk)
    if (push_pulse && !wr_seen) begin
      mem_v   <= din;
      wr_seen <= 1'b1;
    end
  always @(posedge rd_clk) begin
    v0  <= wr_seen;
    v1  <= v0;
    if (v1)
      rdata <= mem_v;
  end
  assign pop = v1;
  property p_ok;
    logic [7:0] v;
    @(posedge wr_clk) ($rose(wr_seen), v = mem_v)
      |-> @(posedge rd_clk) ##2 (pop && rdata == v);
  endproperty
  ok:    assert property (@(posedge wr_clk) p_ok);     // open (ic3/BMC stress; see header)
  property p_baddata;
    logic [7:0] v;
    @(posedge wr_clk) ($rose(wr_seen), v = mem_v)
      |-> @(posedge rd_clk) ##2 (pop && rdata == (v ^ 8'hFF));
  endproperty
  baddata: assert property (@(posedge wr_clk) p_baddata);  // DIS
endmodule
