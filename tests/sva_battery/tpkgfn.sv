// TOP: tpkgfn
// Imported constant functions (register v3 D7c): a wildcard-imported
// package function called by its bare name in a localparam (module or
// package scope) used to be "Unresolved function" (package scope: the
// package's own `import` was never applied) or "Constant function did not
// evaluate" (the result variable's reference kept the package-qualified
// spelling while its declaration became $result).  Widths below are the
// evaluated results; the DIS line is the control.
package tpf_u;
  function automatic integer vbits(integer value);
    return (value == 1) ? 1 : $clog2(value);
  endfunction
  function automatic integer plus1(integer value);
    return value + 1;
  endfunction
endpackage
package tpf_b;
  import tpf_u::*;
  localparam int unsigned N = 16;
  localparam int unsigned NW = vbits(N);          // 4
  typedef logic [NW-1:0] idx_t;
endpackage
module tpkgfn(input logic clk, input tpf_b::idx_t i, output logic [7:0] w, output logic [7:0] x);
  import tpf_u::*;
  localparam int W = vbits(16);                   // 4
  localparam int P = plus1(16);                   // 17
  localparam int Q = tpf_u::vbits(1);             // 1 (qualified form, the earlier fix)
  assign w = W;
  assign x = P;
  c_w:  assert property (@(posedge clk) w == 8'd4 && x == 8'd17 && $bits(i) == 4 && Q == 1); // PROVED
  c_bad: assert property (@(posedge clk) w == 8'd0);                                          // DIS @0
endmodule
