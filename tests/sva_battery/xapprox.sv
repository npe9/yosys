// Positive controls for the sva_lowering approximation detectors
// (register v3 A3/B3): every kind in the README table is observed firing
// here, by name, and `sva2check -exact` refuses the file.  Not a verdict
// test (x* = stress, not gated on PROVED/DIS).  Run
//   yosys -p 'read_verilog -sv -formal xapprox.sv; hierarchy -check; sva2check'
// plain (10 kinds), with SVA_NOSPLIT=1 (adds locals-merged-window and
// locals-lowest-hot-age) and with SVA_NOANC=1 (adds
// noncons-rep-merged-counter in place of the anchored top-age kind).
// until-merged-attempts is covered by t8.sv (p_utw_b).
module xapprox(input logic clk, a, b, c, input logic [3:0] d);
  // first-match-any (`and` operand) + first-match-overlap-window (ranged delays)
  p_fm_and: assert property (@(posedge clk) a |-> first_match((b ##[1:2] c) and (c ##[1:3] b)));
  // first-match-overlap-window alone
  p_fm_win: assert property (@(posedge clk) a |-> first_match(b ##[1:3] c));
  // first-match-locals-shared-slots: first_match of a sequence reading a local
  property p_fml;
    logic [3:0] x;
    (a, x = d) |-> first_match(##[1:3] (b && d == x));
  endproperty
  p_fm_loc: assert property (@(posedge clk) p_fml);
  // noncons-rep-unbounded-top-age (anchored, default) /
  // noncons-rep-merged-counter (SVA_NOANC=1): [->m] with a local
  property p_ncl;
    logic [3:0] x;
    (a, x = d) |-> ##1 b[->2] ##1 (c && d == x);
  endproperty
  p_nc_loc: assert property (@(posedge clk) p_ncl);
  // locals-merged-window + locals-lowest-hot-age under SVA_NOSPLIT=1:
  // ranged delay whose tail reads the local
  property p_lw;
    logic [3:0] x;
    (a, x = d) ##[1:3] (b && d == x) |-> c;
  endproperty
  p_loc_win: assert property (@(posedge clk) p_lw);
  // weak-as-strong
  p_weak: assert property (@(posedge clk) a |-> weak(b ##[1:2] c));
  // noncons-rep-seq-body-merged: [->] over a sequence body
  p_ncseq: assert property (@(posedge clk) ((b ##1 c)[->1]) |-> a);
  // strong-seq-merged-attempts: a property-position sequence whose
  // window exceeds 16 rotating copies
  p_ssm: assert property (@(posedge clk) a |-> (b ##[1:20] c));
  // seq-binop-merged-attempts: intersect with an unbounded operand
  p_binop: assert property (@(posedge clk) (b[=1] intersect (c ##[1:2] b)) |-> a);
  // late-failure-tick: an assumption whose early failure cannot be built
  // (slot mode: local variables) keeps its deadline tick
  property q_lt;
    logic [3:0] v;
    (a, v = d) |-> ##1 (d == v) ##1 b;
  endproperty
  q_late: assume property (@(posedge clk) q_lt);
endmodule
