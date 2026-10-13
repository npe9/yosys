// Crash regression (sva_fuzz D-1-*): a sampled-value function as the
// throughout/within gate crashed sva2check (dangling expr_drv entry after
// sampled-value lowering).  Also exercises bare bounded sequences in
// property position (within/throughout) enabled by the exact-repetition
// [*N] bound fix.
module tthrough1 (input logic clk, input logic a, input logic b);
  p0: assert property (@(posedge clk) always ($fell(b) throughout (a[*1]))); // DIS @0
  p1: assert property (@(posedge clk) always (a && $fell(b) |-> ($fell(b) throughout (a[*1])))); // PROVED
  p2: assert property (@(posedge clk) always ($rose(a) within (b[*2]))); // DIS @1
endmodule
