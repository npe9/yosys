// Checker blocks (IEEE 1800-2009 17): port connection styles. Positional
// connections with and without a defaulted port, and `.*` implicit
// connections binding formals to same-named module signals.
// ctr free-runs 0,1,2,...; a/b pulse at ctr==10/11.
module tchk2 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b, sig;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd11);
  assign sig = a;
  logic [7:0] expct;
  assign expct = 8'd11;

  // 1. positional connection, defaulted port y = 11 kicks in.
  checker c_pos(x, y = 8'd11);
    p_ok: assert property (@(posedge clk) x |=> (ctr == y));  // PROVED
  endchecker
  c_pos u_pos (sig);

  // 2. positional connection overriding the default.
  checker c_ovr(x, y = 8'd11);
    p_b: assert property (@(posedge clk) x |=> (ctr == y));   // DIS @11 (y=12)
  endchecker
  c_ovr u_ovr (sig, 8'd12);

  // 3. `.*`: sig/expct bind to the same-named module signals.
  checker c_wc(sig, expct);
    w_ok: assert property (@(posedge clk) sig |=> (ctr == expct)); // PROVED
  endchecker
  c_wc u_wc (.*);

  // 4. `.*` mixed with an explicit named override.
  checker c_wb(sig, expct);
    w_b: assert property (@(posedge clk) sig |=> (ctr == expct));  // DIS @11 (expct=13)
  endchecker
  c_wb u_wb (.*, .expct(8'd13));
endmodule
