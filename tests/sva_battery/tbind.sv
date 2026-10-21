// `bind` (IEEE 1800-2017 23.11; 2026-10-21, the AXI4 FVIP's testbench
// binds amba_axi4_protocol_checker into axi_crossbar).  The bound instance
// used to stay where the bind was written: in the same file it was
// SILENTLY DROPPED (stock yosys 0.51 too), from another file it was
// appended to every later module ("Cannot add cell ... already created"),
// and its port expressions resolved in the bind's file scope (implicit
// nets).  Now it is moved into the target module before elaboration (a
// target from an earlier read_verilog is re-elaborated) and its ports
// resolve there; an unknown target is a loud hierarchy warning.
// The bound checker's output drives `seen` in the target: if the bind is
// dropped, `seen` is undriven (a free input) and b_seen disproves.
// Trace: x @ 2..4.
// TOP: tbind
module tbind_chk #(parameter int W = 1) (input logic clk, input logic [W-1:0] v, output logic ok);
  logic [W-1:0] prev = '0;
  always_ff @(posedge clk) prev <= v;
  assign ok = (v == prev) || (v == prev + 1'b1) || (v == '0);
endmodule

module tbind (input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic x, seen, seen2;
  assign x = t >= 2 && t <= 4;

  b_seen:  assert property (@(posedge clk) seen);               // PROVED
  b_seen2: assert property (@(posedge clk) t != 4'd15 |-> seen2); // PROVED
  b_nseen: assert property (@(posedge clk) !seen2);              // DIS @0
endmodule

bind tbind tbind_chk #(.W(4)) u_chk (.clk(clk), .v(t), .ok(seen));
bind tbind tbind_chk #(.W(1)) u_chk2 (.clk(clk), .v(x), .ok(seen2));
