// Liveness (unbounded eventually) cases for the native ic3 pass.
// x-prefixed: outside the t*.sv battery glob; run_bmc.py does not support
// liveness.  DIS expectations carry no @N (v1 reports depth 0 for liveness
// disproofs; the lasso witness export is out of scope) -- check the ic3
// verdict lines manually.
module xlive1 (input logic clk);
  logic [2:0] c;
  always_ff @(posedge clk)
    c <= c + 3'd1;

  // the free-running counter visits 5 on every cycle
  l1: assert property (@(posedge clk) s_eventually c == 3'd5);   // PROVED
  // c is 3 bits wide: 8 is unreachable, so F p is false
  l2: assert property (@(posedge clk) s_eventually c == 4'd8);   // DIS
  // c > 1 holds forever once the counter has passed 0 and 1
  l3: assert property (@(posedge clk) s_eventually (c > 3'd1));  // PROVED
  // disable iff: attempts are vacuous at c == 0; from any other value
  // the counter still reaches 5
  l4: assert property (@(posedge clk) disable iff (c == 3'd0) s_eventually (c == 3'd5)); // PROVED
endmodule
