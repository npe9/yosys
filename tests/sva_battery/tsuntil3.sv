// Temporal-p strong until: (ptemp) s_until q == (ptemp until q) && F q with
// p a bounded temporal property, not just a boolean.  p compiles to its
// per-tick failure pulse (prop_dispatch at start=1 every tick, the same
// scheme as the bounded eventually operand): "p holds at tick t" is "the
// p attempt starting at t completes", and its violation pulse fires at
// t's own deadline -- exactly the sampling the weak-until monitor
// performs.  s_until_with over a temporal p stays rejected (the
// with-variant's q&p completion conjunct is not defined for temporal p).
//
// The ctr shape makes every verdict structurally forced (b = t1,t2;
// c = t3; a = t5; d = t1; e = 0; ctr wraps at 8):
//   p1 fail_p = !(b(t-1)&&c(t)) && !(b(t-2)&&c(t-1)) is 1 at t1 -> DIS @1
//   p2 q covers every fail_p tick (ctr<=2 | ctr>=5) -> PROVED
//   p3 q = (ctr<=2) | a | (ctr==6): fail_p fires at t in {1,5,6,7} mod 8;
//     t1 is masked by qsen(1)=q(0) and t7 by qsen(7)=q(6) -- the monitor's
//     q-seen register covers q through t-1 ("no q since the attempt
//     start") -- and t5/t6 by q directly -> PROVED
//   p4 d at t1 with e never -> implication instance fails at t2 -> DIS @2
//   p5 strong-completion b ##2 c fails from t2 on (no c) -> DIS @2
//   (strong(seq) as the direct lhs of s_until is a parser limitation,
//    not a lowering one: strong/weak property primaries sit above the
//    sva_until precedence chain.)
// 2026-10-16 expectation corrected (sva_pbt F1): at ctr=3 neither q nor
// p = (b ##1 c) holds (b(3)=0), so the attempt starting there fails; the
// LRM step is 3 (4 with the deadline-based sequence lowering), the
// framework reports frame 7 -- step kept unpinned until per-attempt
// tracking (register v3 D5) lands.
// 2026-10-19 expectation corrected (register v3 C1): the fail_p formula
// above ORs in b(t-2)&&c(t-1), the PREVIOUS attempt's completion -- the
// strong_seq_fail end window reached back into earlier attempts.  p =
// (b ##1 c) holds at t only if b(t) && c(t+1), i.e. at ctr 2 alone, so
// the attempt at ctr 3 (q low, p false) fails: p2 is DIS (LRM step 3,
// engine 4).  p3 stays DIS for the same reason.  (oracle.py cannot check
// this file: s_until is outside its grammar.)
module tsuntil3 (input logic clk);
  logic [2:0] ctr = 0;
  logic a, b, c, d, e;
  always_ff @(posedge clk) ctr <= ctr + 1;
  assign b = (ctr == 3'd1) || (ctr == 3'd2);
  assign c = (ctr == 3'd3);
  assign a = (ctr == 3'd5);
  assign d = (ctr == 3'd1);
  assign e = 1'b0;
  p1: assert property (@(posedge clk) (b ##1 c) s_until a); // DIS @1
  p2: assert property (@(posedge clk) (b ##1 c) s_until ((ctr <= 3'd2) || (ctr >= 3'd5))); // DIS @4 (LRM @3: q and b are low at ctr 3; the engine reports at p's deadline, known class F5 / register D5)
  p3: assert property (@(posedge clk) (b ##1 c) s_until ((ctr <= 3'd2) || a || (ctr == 3'd6))); // DIS
  p4: assert property (@(posedge clk) (d |-> ##[0:1] e) s_until a); // DIS @2
  p5: assert property (@(posedge clk) (b ##2 c) s_until a); // DIS @2
endmodule
