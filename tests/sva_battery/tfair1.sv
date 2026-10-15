// Fairness: assume property (s_eventually ...) lowers to $fair cells
// (sva2check) and the ic3 liveness gfp prunes its survivor set to states
// that can visit every $fair predicate within the set (Emerson-Lei fair
// gfp, attractor per predicate), so only FAIR !p-cycles disprove.
// BMC-SKIP: all three properties are liveness-class (s_eventually), so
// the safety-only BMC leg has nothing to check (write_smt2 skips both the
// liveness $check cells and the $fair cell with warnings); expectations
// are checked by the ic3 leg only.  See tfair2 for $fair + safety legs.
module tfair1 (input logic clk, input logic tick);
  logic b;
  logic [2:0] c;
  always_ff @(posedge clk) begin
    if (tick) b <= ~b;
    if (tick) c <= (c == 3) ? 3 : c + 1;
  end

  // 1: without the fairness constraint tick=0 forever keeps b low and
  //    disproves F(b); the assumption forces b to toggle infinitely.
  assume property (@(posedge clk) s_eventually tick);
  f_toggle: assert property (@(posedge clk) s_eventually b);          // PROVED

  // 2: c saturates at 3, so c==5 never happens; the fair cycle
  //    (c==3, tick=1) visits the fairness predicate infinitely, so the
  //    disproof survives the fair pruning (no false proof).
  f_sat: assert property (@(posedge clk) s_eventually (c == 3'd5));   // DIS

  // 3: under the fairness assumption c reaches 3 (tick fires
  //    infinitely); the unfair stalling paths are excluded.
  f_hit3: assert property (@(posedge clk) s_eventually (c == 3'd3));  // PROVED
endmodule
