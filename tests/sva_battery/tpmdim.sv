// Packed multi-dimensional parameters (C3 corpus finding, 2026-10-20):
// `parameter logic [3:0][1:0] P` kept its declared width but not its
// dimensions, so `P[k]` read bit k of the flattened value, zero-extended,
// instead of element k.  OpenTitan aes_prng_masking's PermutationCheck_A
// (an initial-block check that RndCnstLfsrPerm is a permutation) went
// DIS @0 on the default, valid permutation.  Constant and dynamic selects,
// explicit and typedef'd types, localparams and module parameters, [hi:lo]
// and [lo:hi] ranges, three dimensions and element slices are covered;
// verified against Verilator 5.052 on the same expressions.
// Unpacked parameter arrays (`localparam logic [7:0] M [4:6] = '{..}`) had
// their unpacked range dropped by the parser: $low/$high/$size folded to
// 0/0/1, a [hi:lo] array read its pattern backwards and an index outside
// 0..n-1 read an undriven wire (pulp lfsr's `Masks[LfsrWidth]` checks, DIS
// @0).  Now the declared bounds and direction are kept, and a read that
// still does not fold is a loud frontend degradation.
typedef logic [3:0][1:0] tpmdim_perm_t;
module tpmdim(input logic clk, input logic [1:0] v);
  localparam logic [3:0][1:0] P  = {2'd2, 2'd0, 2'd3, 2'd1};   // a permutation
  localparam tpmdim_perm_t    T  = {2'd2, 2'd0, 2'd3, 2'd1};
  localparam logic [3:0][1:0] B  = {2'd2, 2'd0, 2'd2, 2'd1};   // not one
  localparam logic [0:3][2:0] U  = {3'd5, 3'd1, 3'd7, 3'd2};
  localparam logic [1:0][1:0][1:0] C = 8'b10_01_11_00;
  localparam logic [7:0] M [4:6] = '{8'h0C, 8'h1E, 8'hA0};
  localparam logic [7:0] N [3]   = '{8'h0C, 8'h1E, 8'hA0};
  localparam logic [7:0] R [2:0] = '{8'h0C, 8'h1E, 8'hA0};
  logic [1:0] q = 0;
  always_ff @(posedge clk) q <= v;
  logic [3:0] perm_ok, perm_bad;
  initial begin
    perm_ok = '0;
    perm_bad = '0;
    for (int k = 0; k < 4; k++) begin
      perm_ok[P[k]] = 1'b1;
      perm_bad[B[k]] = 1'b1;
    end
    i_perm:     assert (&perm_ok);                            // PROVED
    i_perm_bad: assert (&perm_bad);                           // DIS @0
    // array queries on parameter arrays (immediate checks: inside a
    // concurrent assertion $low & co. take declared vectors only)
    i_ubnd:  assert ($low(M) == 4 && $high(M) == 6 && $size(M) == 3 && $bits(M) == 24);  // PROVED
    i_uleft: assert ($left(N) == 0 && $high(N) == 2 && $left(R) == 2);                  // PROVED
    i_ubad:  assert ($low(M) == 0);                                                     // DIS @0
  end
  p_const:  assert property (@(posedge clk) P[1] == 2'd3 && P[3] == 2'd2 && T[0] == 2'd1);  // PROVED
  p_dyn:    assert property (@(posedge clk) P[q] == T[q]);                         // PROVED
  p_tab:    assert property (@(posedge clk) q == 2'd1 |-> P[q] == 2'd3);           // PROVED
  p_tabbad: assert property (@(posedge clk) q == 2'd1 |-> P[q] == 2'd1);           // DIS @1
  // the old reading (P[1] = bit 1 = 0) proved this one: a false proof
  p_wasfp:  assert property (@(posedge clk) q == 2'd1 |-> P[q] != 2'd3);           // DIS @1
  p_upto:   assert property (@(posedge clk) U[0] == 3'd5 && U[3] == 3'd2 && U[1:2] == 6'o17);  // PROVED
  p_3d:     assert property (@(posedge clk) C[1][0] == 2'b01 && C[0][1] == 2'b11 && C[1] == 4'b1001);  // PROVED
  p_slice:  assert property (@(posedge clk) P[2:1] == 4'b00_11);                   // PROVED
  p_bits:   assert property (@(posedge clk) $bits(P[0]) == 2);                     // PROVED
  p_uel:    assert property (@(posedge clk) M[4] == 8'h0C && M[6] == 8'hA0 && N[0] == 8'h0C && R[2] == 8'h0C && R[0] == 8'hA0);  // PROVED
  p_udyn:   assert property (@(posedge clk) q == 2'd0 |-> R[q] == 8'hA0 && N[q] == 8'h0C);  // PROVED
  p_udbad:  assert property (@(posedge clk) q == 2'd2 |-> R[q] == 8'hA0);           // DIS @1
  p_umsb:   assert property (@(posedge clk) M[6][7] && !M[5][7]);                  // PROVED
endmodule
