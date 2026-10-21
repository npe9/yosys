// The AXI4 FVIP low-power channel (2026-10-21, see tfvip_bound.sv):
// `assert property (disable iff (!ARESETn) csysreq_fall(CSYSREQ, CSYSACK))`
// whose package declaration body is `(@(negedge csysreq) csysack)` was
// "no usable sampling clock": the assertion's own clock slot is empty and
// the instance body's leading clock (LRM 16.16) was never looked up.  Now
// the chain walks into the instance, takes its clock, and keeps the outer
// disable iff.  The k_dir_* twins write the same clock at the assertion;
// the verdicts agree on both legs.  (Under `default clocking` the
// assertion gets the default clock and the instance's own clock is a
// nested clock change: a loud "unsupported property operator 'clocked'",
// not exercised here.)
// Trace: rq @ 2..4 (falls at 5), ak_bad low @ 5, rst_on low @ 4..6.
package tfvip_clk_pkg;
  property fall(r, a); (@(negedge r) a); endproperty
endpackage

module tfvip_clk import tfvip_clk_pkg::*; (input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic rq, ak_bad, rst, rst_on;
  assign rq = t >= 2 && t <= 4;
  assign ak_bad = !(t == 5);
  assign rst = 1'b0;
  assign rst_on = !(t >= 4 && t <= 6);

  k_ins_ok:  assert property (disable iff (rst) fall(rq, 1'b1));           // PROVED
  k_ins_bad: assert property (disable iff (rst) fall(rq, ak_bad));         // DIS @5
  k_dir_bad: assert property (@(negedge rq) disable iff (rst) ak_bad);     // DIS @5
  k_ins_dis: assert property (disable iff (!rst_on) fall(rq, ak_bad));     // PROVED
  k_dir_dis: assert property (@(negedge rq) disable iff (!rst_on) ak_bad); // PROVED
endmodule
