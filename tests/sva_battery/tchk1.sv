// Checker blocks (IEEE 1800-2009 17): basic instantiation. Named port
// connections, one instance, two assertions in the checker body.
// ctr free-runs 0,1,2,...; a/b pulse at ctr==10/11.
module tchk1 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd11);

  // c1(sig, lim): sig implies b next cycle; ctr stays under lim.
  checker c1(sig, lim);
    c1_ok: assert property (@(posedge clk) sig |=> b);   // PROVED
    c1_b:  assert property (@(posedge clk) ctr <= lim);  // DIS @13 (lim=12)
  endchecker

  c1 u_chk (.sig(a), .lim(8'd12));
endmodule
