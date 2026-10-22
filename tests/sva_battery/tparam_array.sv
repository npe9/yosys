// Unpacked-array parameters (2026-10-21; ibex `.PMPRstCfg(PMPRstCfg)`,
// OpenTitan `.RaclPolicySelVec(RaclPolicySelVec)`):
//  (1) passed through BY NAME to an instance, the value did not fold and was
//      silently overridden with 0 (03cc3a9f4 made it a strict error).  It is
//      now carried as one flat constant (element 0 most significant; struct
//      elements materialized with the element type) and split back into a
//      pattern in the derived module.
//  (2) a default naming another array parameter (`= pk::Rst`) did not fold
//      element reads.
//  (2b) a replication pattern `'{N{v}}` (OpenTitan's RaclPolicySelVec
//      default `'{NumRegs{0}}`) did not fold at all, as default or override.
//  (3) a STRUCT-typed array (`parameter c_t P [4]`) never recorded its
//      unpacked dimension (the packed one was recorded first): `P[1]` read
//      a bit.
//  (4) a member select on an element (`P[1].m`) became an implicit wire
//      (the constant-prefix rule named it `P[1].m`); it now folds from the
//      parameter's value, honouring a descending range (`[3:0]`: index 0
//      is the LAST pattern element).  A non-constant index is still open.
//  (5) `'{default: v}` for an unpacked array lists every element (it was
//      folded as ONE struct-wide constant -- a 2026-10-21 regression of the
//      type-name resolution -- or left unfoldable); u_dd uses the defaults.
// TOP: tparam_array
package tpa_pkg;
  typedef struct packed { logic [1:0] m; logic l; } c_t;
  localparam c_t Rst [4] = '{'{m:2'd1, l:1'b0}, '{m:2'd2, l:1'b1}, '{m:2'd3, l:1'b0}, '{m:2'd0, l:1'b1}};
  localparam int R [3] = '{7, 8, 9};
endpackage

module tpa_reg #(parameter tpa_pkg::c_t RV = '0) (output logic [2:0] q);
  assign q = RV;
endmodule

module tpa_leaf #(parameter tpa_pkg::c_t P [4] = '{default:'0}, parameter int N [3] = '{0, 0, 0},
                  parameter int D [3] = tpa_pkg::R, parameter int NR = 3,
                  parameter logic [1:0] RP [NR] = '{NR{2'd1}}, parameter logic [1:0] RD [NR] = '{NR{2'd3}})
  (output logic [2:0] o1, output logic [2:0] o2, output logic [7:0] n1, output logic [7:0] d2,
   output logic [1:0] rp, output logic [1:0] rd,
   output logic [1:0] m1, output logic l2, output logic [1:0] dm0);
  localparam tpa_pkg::c_t DESC [3:0] = '{'{m:2'd3, l:1'b0}, '{m:2'd2, l:1'b1}, '{m:2'd1, l:1'b1}, '{m:2'd0, l:1'b0}};
  assign m1 = P[1].m;
  assign l2 = P[2].l;
  assign dm0 = DESC[0].m;
  tpa_reg #(.RV(P[1])) r1 (.q(o1));
  tpa_reg #(.RV(P[2])) r2 (.q(o2));
  assign n1 = N[1];
  assign d2 = D[2];
  assign rp = RP[1];
  assign rd = RD[2];
endmodule

module tparam_array #(parameter tpa_pkg::c_t Q [4] = tpa_pkg::Rst, parameter int M [3] = '{4, 5, 6},
                      parameter int NumRegs = 3, parameter logic [1:0] QR [NumRegs] = '{NumRegs{2'd2}})
  (input logic clk);
  logic [2:0] o1, o2;
  logic [7:0] n1, d2;
  logic [1:0] rp, rd, m1, dm0;
  logic l2;
  tpa_leaf #(.P(Q), .N(M), .NR(NumRegs), .RP(QR)) u (.o1(o1), .o2(o2), .n1(n1), .d2(d2), .rp(rp), .rd(rd),
      .m1(m1), .l2(l2), .dm0(dm0));
  a_struct: assert property (@(posedge clk) o1 == 3'b101 && o2 == 3'b110); // PROVED
  a_int:    assert property (@(posedge clk) n1 == 5 && d2 == 9);           // PROVED
  a_rep:    assert property (@(posedge clk) rp == 2'd2 && rd == 2'd3);    // PROVED
  logic [2:0] e1, e2;
  logic [7:0] dn1, dd2;
  logic [1:0] drp, drd, dm1, ddm0;
  logic dl2;
  tpa_leaf u_dd (.o1(e1), .o2(e2), .n1(dn1), .d2(dd2), .rp(drp), .rd(drd), .m1(dm1), .l2(dl2), .dm0(ddm0));
  a_dflt:   assert property (@(posedge clk) e1 == 3'b000 && dm1 == 2'd0 && dl2 == 1'b0 && drp == 2'd1); // PROVED
  a_mem:    assert property (@(posedge clk) m1 == 2'd2 && l2 == 1'b0 && dm0 == 2'd0); // PROVED
  a_neg:    assert property (@(posedge clk) o1 == 3'b000);                 // DIS @0
endmodule
