// cover of implications and of unbounded sequences (register v3 D4,
// 2026-10-23).  IEEE 1800-2017 16.14.3 counts NON-VACUOUS successes;
// 16.14.8: `R |-> P` is non-vacuous when R matches and P succeeds
// non-vacuously.  So a vacuous-only implication is UNREACHABLE, and the hit
// of R |-> P is P's match started at R's end (|=>: one tick later).  These
// used to be skipped ("cover of a property operator", "cover of an
// unbounded sequence").  ctr = BMC step; reach depths by hand.
module tcover_impl (input logic clk, input logic x, input logic y, input logic z);
  logic [3:0] ctr = 4'd0;
  always_ff @(posedge clk) if (ctr != 4'd15) ctr <= ctr + 4'd1;

  c_same:  cover property (@(posedge clk) (ctr == 4'd2) |-> (ctr == 4'd2));               // REACHED @2
  c_vac:   cover property (@(posedge clk) (ctr == 4'd2) |=> (ctr == 4'd4));               // UNREACHABLE
  c_vac0:  cover property (@(posedge clk) (ctr == 4'd2) |-> (ctr == 4'd3));               // UNREACHABLE
  c_seq:   cover property (@(posedge clk) (ctr == 4'd2) |=> (ctr == 4'd3) ##1 (ctr == 4'd4)); // REACHED @4
  c_goto:  cover property (@(posedge clk) (ctr == 4'd2) |-> (ctr <= 4'd6) throughout (ctr == 4'd6)[->1]); // REACHED @6
  c_gbad:  cover property (@(posedge clk) (ctr == 4'd2) |-> (ctr <= 4'd4) throughout (ctr == 4'd6)[->1]); // UNREACHABLE
  c_nest:  cover property (@(posedge clk) (ctr == 4'd1) |=> ((ctr == 4'd2) |=> (ctr == 4'd3))); // REACHED @3
  c_nvac:  cover property (@(posedge clk) (ctr == 4'd1) |=> ((ctr == 4'd5) |=> (ctr == 4'd6))); // UNREACHABLE
  c_unb:   cover property (@(posedge clk) (ctr == 4'd2) ##[1:$] (ctr == 4'd9));             // REACHED @9
  c_free:  cover property (@(posedge clk) x |-> y[->1] ##1 z);                              // REACHED @1
endmodule
