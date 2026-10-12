// Checker blocks (IEEE 1800-2009 17): nested checker instantiation -- a
// checker instantiated inside another checker body. Instance names and
// local declarations compose (u_out$u_in$s1).
// ctr free-runs 0,1,2,...; a/b/c pulse at ctr==10/11/12.
module tchk3 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b, c;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd11);
  assign c = (ctr == 8'd12);

  checker inner(x);
    i_ok: assert property (@(posedge clk) x |=> b);          // PROVED
  endchecker

  checker outer(y);
    // nested instantiation: the inner checker's formal binds to the
    // outer checker's formal
    inner u_in (.x(y));
    // y |=> ##2 c: y at t, c at t+3 -> a@10 requires c@13, never true
    o_b: assert property (@(posedge clk) y |=> ##2 c);       // DIS @13
  endchecker

  outer u_out (.y(a));
endmodule
