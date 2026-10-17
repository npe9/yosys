// TOP: tdtype
// Type parameters (register v3 D0): `parameter type T = <default>` is a
// typedef that is also a parameter (advertised to hierarchy); an instance's
// `.T(<typedef name>)` -- module-local, file-scope, package-qualified or an
// enum -- is spliced into the derived module, so ports declared `T` take the
// override's width.  The properties compare widths the override decides.
package tdt_pkg;
  typedef logic [4:0] five_t;
endpackage
typedef logic [6:0] seven_t;
typedef enum logic [1:0] {TA = 0, TB = 1, TC = 2} st_e;
module tdtype_leaf #(parameter int W = 2, parameter type T = logic [W-1:0]) (input logic clk, input T d, output T q);
  always_ff @(posedge clk) q <= d;
  // (per-instance copies report as u7.p_w etc.; the top-level w7 carries the expectation)
  p_w: assert property (@(posedge clk) $bits(q) == $bits(d));
endmodule
module tdtype(input logic clk, input logic [6:0] d7, input logic [4:0] d5, input logic [2:0] d3, input st_e de,
              input logic [8:0] d9, input logic [5:0] d6,
              output logic [6:0] q7, output logic [4:0] q5, output logic [2:0] q3, output st_e qe,
              output logic [8:0] q9, output logic [5:0] q6, output logic [13:0] qc);
  typedef logic [5:0] six_t;   // a module-local typedef as the override (qualified `tdtype::six_t`)
  // a struct override whose member type and range come from the parent
  // (pulp axi's ar_chan_t shape): addr_t and AddrW travel with it
  localparam int AddrW = 12;
  typedef logic [AddrW-1:0] addr_t;
  typedef struct packed { addr_t addr; logic [1:0] size; } chan_t;
  chan_t ch;
  assign ch = '{addr: d9[11:0], size: d6[1:0]};
  tdtype_leaf #(.T(chan_t)) uc (.clk(clk), .d(ch), .q(qc));
  tdtype_leaf #(.T(logic [8:0])) u9 (.clk(clk), .d(d9), .q(q9));   // an anonymous type (D0b)
  tdtype_leaf #(.T(six_t)) u6 (.clk(clk), .d(d6), .q(q6));
  tdtype_leaf #(.T(seven_t)) u7 (.clk(clk), .d(d7), .q(q7));
  tdtype_leaf #(.T(tdt_pkg::five_t)) u5 (.clk(clk), .d(d5), .q(q5));
  tdtype_leaf #(.W(3)) u3 (.clk(clk), .d(d3), .q(q3));
  tdtype_leaf #(.T(st_e)) ue (.clk(clk), .d(de), .q(qe));
  w7: assert property (@(posedge clk) $bits(q7) == 7 && $bits(q5) == 5 && $bits(q3) == 3 && $bits(qe) == 2); // PROVED
  w96: assert property (@(posedge clk) $bits(q9) == 9 && $bits(q6) == 6);  // PROVED
  wc1: assert property (@(posedge clk) $bits(qc) == 14);                               // PROVED
  wc2: assert property (@(posedge clk) d9[11:0] == 12'h123 |=> qc[13:2] == 12'h123);   // PROVED
  w7bad: assert property (@(posedge clk) $bits(q7) == 2);               // DIS @0
  e_hold: assert property (@(posedge clk) de == TB |=> qe == TB);       // PROVED
endmodule
