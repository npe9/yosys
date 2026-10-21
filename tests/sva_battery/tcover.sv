// Register v3 D4: `cover property` / `cover sequence` checked as
// reachability (2026-10-21).  A cover of a bounded sequence or a boolean
// is REACHED @N when its match can first occur at step N (a witness), or
// UNREACHABLE when the engine proves it can never occur.  Before D4 every
// cover was "not checked" (A tied to 0).  A property-operator body stays a
// loud skip.  Motivating case (AXI4 FVIP on a crossbar, peer session): a
// cover on a DUT output that an over-strong assumption made unreachable --
// c_vac below, the vacuity a passing assertion cannot show.
// Trace: x @ 2..4, y @ 3; g a free input.
module tcover (input logic clk, input logic g);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic x, y, rst;
  assign x = t >= 2 && t <= 4;
  assign y = t == 3;
  assign rst = t == 3;
  sequence s_xy; x ##1 y; endsequence
  // property declarations with a boolean / sequence body (the FVIP's
  // valid_with_ready, axi4_back_to_back) cover like sequences
  property pr(v, r); v && r; endproperty
  property pb(v, r); v ##1 r; endproperty

  c_bool:  cover property (@(posedge clk) x);                    // REACHED @2
  c_seq:   cover property (@(posedge clk) x ##1 y);              // REACHED @3
  c_never: cover property (@(posedge clk) x ##1 !x ##1 x);       // UNREACHABLE
  c_decl:  cover property (@(posedge clk) s_xy);                 // REACHED @3
  c_sseq:  cover sequence (@(posedge clk) y ##1 x);              // REACHED @4
  c_dis:   cover property (@(posedge clk) disable iff (rst) y);  // UNREACHABLE
  c_pd:    cover property (@(posedge clk) pr(x, y));              // REACHED @3
  c_pd2:   cover property (@(posedge clk) pr(y, !x));             // UNREACHABLE
  c_pb:    cover property (@(posedge clk) pb(x, !x));             // REACHED @5
  // a property-operator body has no cover lowering yet: loud skip
  c_imp:   cover property (@(posedge clk) x |-> y);               // NOT-CHECKED
  // an assumption that forbids g makes its cover unreachable (vacuity)
  a_g:     assume property (@(posedge clk) !g);
  c_vac:   cover property (@(posedge clk) g);                    // UNREACHABLE
  c_free:  cover property (@(posedge clk) x && !g);              // REACHED @2
  // a passing assertion over the same g: it cannot tell that g never rises
  p_vac:   assert property (@(posedge clk) g |=> x);             // PROVED
  for (genvar i = 1; i < 3; i++) begin: sym
    wp: cover property (@(posedge clk) t == i);
  end
endmodule
