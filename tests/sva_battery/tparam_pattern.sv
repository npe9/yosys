// Assignment patterns as parameter values (2026-10-21, the AXI4 FVIP
// testbench's `amba_axi4_protocol_checker #('{ID_WIDTH:4, ..., MAXWAIT:20})`).
// Silent zeros, all fixed:
//  (1) a pattern as an instance OVERRIDE (positional, named, `.cfg(..)`,
//      `default:`) has no type in the instantiating module: the derived
//      module got cfg=32'0 (wrong value AND width) -- every FVIP checker ran
//      with an all-zero cfg (ID_WIDTH 0, MAXWAIT 0, ...).  The override now
//      takes the overridden parameter's struct type; one that cannot be
//      typed is an error, and so is any parameter value that does not fold
//      (genrtlil used to write Const(0)).
//  (2) a NAMED pattern for a parameter of a compilation-unit (file-scope)
//      typedef folded to 0 (the struct was cloned before it was sized).
// An untyped named pattern (`localparam [15:0] X = '{A:5, B:9}`) is an
// error.  The generate-loop runaway guard (16384 iterations) is the FVIP's
// MAX_PENDING-1 wrap; not pinned here (it takes seconds).
// TOP: tparam_pattern
typedef struct packed { logic [7:0] A; logic [7:0] B; } tpp_unit_t;
package tpp_pkg;
  typedef enum logic [1:0] {SRC, DST} agent_t;
  typedef struct packed { int unsigned ID_WIDTH; int unsigned MAXW; agent_t AGENT; logic EN; } cfg_t;
endpackage

module tpp_leaf import tpp_pkg::*;
  #(parameter cfg_t cfg = '{ID_WIDTH:4, MAXW:4, AGENT:SRC, default:'0})
  (input logic [cfg.ID_WIDTH-1:0] id, output logic [31:0] w, output logic [31:0] mw,
   output logic [1:0] ag, output logic en);
  assign w = $bits(id);
  assign mw = cfg.MAXW;
  assign ag = cfg.AGENT;
  assign en = cfg.EN;
endmodule

module tpp_unit #(parameter tpp_unit_t cfg = '{A:1, B:2}) (output logic [7:0] b);
  assign b = cfg.B;
endmodule

module tparam_pattern import tpp_pkg::*; (input logic clk);
  logic [31:0] w0, w1, w2, w3, m0, m1, m2, m3;
  logic [1:0] a0, a1, a2, a3;
  logic e0, e1, e2, e3;
  logic [7:0] ub, uc, ud;
  localparam tpp_unit_t P = '{A:5, B:9};
  tpp_leaf u_def (.id('0), .w(w0), .mw(m0), .ag(a0), .en(e0));
  tpp_leaf #('{ID_WIDTH:7, MAXW:20, AGENT:DST, EN:1}) u_named (.id('0), .w(w1), .mw(m1), .ag(a1), .en(e1));
  tpp_leaf #(.cfg('{6, 12, SRC, 1'b1})) u_pos (.id('0), .w(w2), .mw(m2), .ag(a2), .en(e2));
  tpp_leaf #('{default:3}) u_dflt (.id('0), .w(w3), .mw(m3), .ag(a3), .en(e3));
  tpp_unit #(P) u_p (.b(ub));
  tpp_unit #('{A:5, B:9}) u_q (.b(uc));
  tpp_unit u_d (.b(ud));

  p_def:   assert property (@(posedge clk) w0 == 4 && m0 == 4 && a0 == SRC && !e0);   // PROVED
  p_named: assert property (@(posedge clk) w1 == 7 && m1 == 20 && a1 == DST && e1);   // PROVED
  p_pos:   assert property (@(posedge clk) w2 == 6 && m2 == 12 && a2 == SRC && e2);   // PROVED
  p_dflt:  assert property (@(posedge clk) w3 == 3 && m3 == 3 && a3 == 2'd3 && e3);   // PROVED
  p_unit:  assert property (@(posedge clk) ub == 9 && uc == 9 && ud == 2 && P == 16'h0509); // PROVED
  p_neg:   assert property (@(posedge clk) m1 == 0);                                    // DIS @0
endmodule
