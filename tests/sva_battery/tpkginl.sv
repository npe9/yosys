// TOP: tpkginl
// Inlined imported package functions (register v3 D7d): a wildcard-imported
// package function used in a continuous assign used to leave its inlined
// body assigning `r::mix` while the call read `$result`, so the value was
// x (the stress `func` leg read x where Verilator had the value; it was a
// tautology `mix(a,b) ^ mix(b,a)` for weeks, which is why only the mutation
// leg's lesson caught it).  The property pins a concrete value.
package tpi_r;
  function automatic logic [31:0] mix(input logic [15:0] u, input logic [15:0] v);
    return {u ^ v, u + v} >> 3;
  endfunction
endpackage
module tpkginl import tpi_r::*; (input logic clk, input logic [15:0] a, input logic [15:0] b, output logic [31:0] y, output logic [31:0] z);
  assign y = mix(a, b) ^ mix(b, b);
  assign z = mix(a, b);
  // mix(16'h1234, 16'h0001) = {16'h1235, 16'h1235} >> 3 = 32'h0246_A246
  p_val:  assert property (@(posedge clk) (a == 16'h1234 && b == 16'h0001) |-> z == 32'h0246_A246); // PROVED
  p_mut:  assert property (@(posedge clk) (a == 16'h1234 && b == 16'h0001) |-> z == 32'h0246_A247); // DIS @0
  p_xor:  assert property (@(posedge clk) (a == b) |-> y == 32'd0);                                 // PROVED
endmodule
