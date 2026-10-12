// Checker blocks (IEEE 1800-2009 17): a checker's own `default clocking`
// applies to assertions in the body that carry no explicit clocking
// event, including an assertion parsed before the default clocking
// declaration. An explicit clock still wins over the default.
// ctr free-runs 0,1,2,...; a/b/c pulse at ctr==10/11/12.
module tchk5 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b, c;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd11);
  assign c = (ctr == 8'd12);

  // 1. default clocking declared before the assertions.
  checker dc(sig);
    default clocking @(posedge clk); endclocking
    d1_ok: assert property (sig |=> b);            // PROVED
    d1_b:  assert property (sig |=> c);            // DIS @11 (c at ctr==11 is false)
    d2_ok: assert property (@(posedge clk) sig |=> b); // PROVED (explicit clock)
  endchecker
  dc u_dc (.sig(a));

  // 2. default clocking declared after the assertion (17.2 applies it
  // to the whole checker scope regardless of position).
  checker dc2(sig);
    e1_ok: assert property (sig |=> b);            // PROVED
    default clocking @(posedge clk); endclocking
  endchecker
  dc2 u_dc2 (.sig(a));
endmodule
