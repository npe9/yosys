// IC3-BMC: 12
// Same as tinit_one.sv through the batched -bmc pre-pass (one shared
// unrolling for every assert and cover; misses fall through to IC3).
// ic3 -init-zero keeps explicit \init values (2026-10-22).  It used to
// force EVERY register to 0, overriding `reg init = 1'b1`: the reset
// generator below never fired, a_noinit was PROVED (false) and c_init
// UNREACHABLE, while smtbmc and the sim replay -- prepped with `setundef
// -init -zero`, which keeps the init -- saw init = 1 at step 0.  Found by
// the peer's smtbmc -c cross-check on the Mem_Controller / Deburster FVIP
// harnesses (covers reached a step early, some at depth 0 during reset).
// q is a one-hot ring seeded 2'b10: the old all-zero start made a_ring
// fail; its proof exercises the fixed-init-vector generalization.
// Trace: init 1,0,0..; c 0,5,6,7,0..; q 10,01,10,..
module tinit_one_bmc (input logic clk, input logic a);
  logic init = 1'b1;
  always_ff @(posedge clk) init <= 1'b0;
  wire rst_n = !init;
  logic [2:0] c = 3'd0;
  always_ff @(posedge clk) if (!rst_n) c <= 3'd5; else c <= c + 3'd1;
  logic [1:0] q = 2'b10;
  always_ff @(posedge clk) q <= {q[0], q[1]};

  a_noinit: assert property (@(posedge clk) !init);              // DIS @0
  a_c:      assert property (@(posedge clk) c != 3'd6);          // DIS @2
  a_ring:   assert property (@(posedge clk) q != 2'b00);         // PROVED
  a_ring2:  assert property (@(posedge clk) q != 2'b11);         // PROVED
  c_init:   cover property (@(posedge clk) init);                // REACHED @0
  c_c7:     cover property (@(posedge clk) c == 3'd7);           // REACHED @3
  c_q01:    cover property (@(posedge clk) q == 2'b01 && a);     // REACHED @1
endmodule
