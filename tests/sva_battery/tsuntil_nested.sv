// A STRONG until as an implication's consequent (LRM 16.13.12 p2, p4),
// `ant |-> a s_until b`, was "unsupported property operator 's_until'".
// a s_until b == (a until b) && s_eventually b: the main check is the weak
// until (safety), the "$live" twin the eventuality (the temporal-response
// lowering); run_battery folds the legs.  Liveness disproofs carry no step.
// Trace: a @ 2..5, b @ 6 only, c from 2 on; t saturates at 15.
module tsuntil_nested (input logic clk);
  logic [3:0] t = 0;
  always_ff @(posedge clk) if (t != 4'd15) t <= t + 4'd1;
  logic a, b, c;
  assign a = t >= 2 && t <= 5;
  assign b = t == 6;
  assign c = t >= 2;
  default clocking @(posedge clk); endclocking
  su_ok:   assert property (t == 2 |-> a s_until b);          // PROVED
  su_n:    assert property (t == 1 |-> a s_until b);          // DIS @1
  su_nob:  assert property (t == 7 |-> c s_until b);          // DIS
  sw_n:    assert property (t == 2 |-> a s_until_with b);     // DIS @6
  sw_ok:   assert property (t == 2 |-> c s_until_with b);     // PROVED
  sw_nob:  assert property (t == 7 |-> c s_until_with b);     // DIS
  // the weak forms over the same trace: no eventuality
  wu_nob:  assert property (t == 7 |-> c until b);            // PROVED
  su_imp:  assert property (t == 1 |=> a s_until b);          // PROVED
endmodule
