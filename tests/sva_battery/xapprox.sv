// Positive controls for the sva_lowering approximation detectors
// (register v3 A3/B3): every kind in the README table is observed firing
// here, by name, and `sva2check -exact` refuses the file.  Not a verdict
// test (x* = stress, not gated on PROVED/DIS).  Run
//   yosys -p 'read_verilog -sv -formal xapprox.sv; hierarchy -check; sva2check'
// plain (10 kinds), with SVA_NOSPLIT=1 (adds locals-merged-window and
// locals-lowest-hot-age).
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
  // noncons-rep-merged-counter: a [->m:n] whose count set would exceed
  // 65 states keeps the merged counter.  (A local read after an unbounded
  // [->] -- this control's old shape -- is now refused: the ring read
  // the wrong attempt's local.)
  p_nc_big: assert property (@(posedge clk) (a ##1 b[->2:70]) |-> c);
  // empty-match-operand: an and/intersect/within operand that can match
  // empty (the empty match of a ## operand is exact, E1)
  p_empty_op: assert property (@(posedge clk) (a ##1 (b and c[*0:2])) |-> c);
  // prop-binop-unaligned / eventually-unaligned: an operand whose failure
  // tick varies per attempt (until) cannot be aligned onto the other
  p_or_unal: assert property (@(posedge clk) (a until b) or c);
  p_ev_unal: assert property (@(posedge clk) a |-> s_eventually [1:2] (b until c));
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
