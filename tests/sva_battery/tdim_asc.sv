// $left/$right/$low/$high on ascending packed vectors (register item;
// recorded at dd6a43480 as `logic [1:6]` -> $high/$left 5/5, LRM 6/1).
// Holds at e318a3f48 in plain RTL and in concurrent assertions; the w_*
// controls keep the c_* proofs from being vacuous.
module tdim_asc (input logic clk);
  logic [1:6]  v;
  logic [3:10] o;
  logic [7:0]  d;
  c_left:  assert property (@(posedge clk) $left(v) == 1);    // PROVED
  c_high:  assert property (@(posedge clk) $high(v) == 6);    // PROVED
  c_low:   assert property (@(posedge clk) $low(v) == 1);     // PROVED
  c_right: assert property (@(posedge clk) $right(v) == 6);   // PROVED
  c_off:   assert property (@(posedge clk) $left(o) == 3 && $high(o) == 10 && $size(o) == 8);  // PROVED
  c_inc:   assert property (@(posedge clk) $increment(v) == -1 && $increment(d) == 1);        // PROVED
  w_high:  assert property (@(posedge clk) $high(v) == 5);    // DIS @0
  w_left:  assert property (@(posedge clk) $left(v) == 6);    // DIS @0
endmodule
