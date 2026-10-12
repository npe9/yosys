module tfeat5(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b, bb, c2;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd11);
  assign bb = (ctr == 8'd11) || (ctr == 8'd12);   // two consecutive ticks
  assign c2 = (ctr == 8'd12) || (ctr == 8'd13);

  // Multi-hot age: local read where the satisfying parse is the OLDER-age
  // tap.  (a,v=ctr) at 10 (v=10), |=> at 11.  bb[*1:2] ##0 (ctr==v+2):
  // 1-rep end 11 (age 1) fails 11==12; 2-rep end 12 (age 2) satisfies
  // 12==12.  "lowest hot age wins" reads the age-1 slot at 12 (stale) and
  // misses the only good parse.
  property p_rep2;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> bb[*1:2] ##0 (ctr == v + 8'd2);
  endproperty
  p_rep2_ok: assert property (@(posedge clk) p_rep2);                                    // PROVED
  property p_rep2b;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> bb[*1:2] ##0 (ctr == v + 8'd3);
  endproperty
  p_rep2_bad: assert property (@(posedge clk) p_rep2b);                                  // DIS @12

  // Same shape with ##[1:2]: end 12 (offset 1, age 2) fails 12==13; end 13
  // (offset 2, age 3) satisfies 13==10+3.
  property p_rng;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> (b ##[1:2] c2) ##0 (ctr == v + 8'd3);
  endproperty
  p_rng_ok: assert property (@(posedge clk) p_rng);                                     // PROVED
  property p_rngb;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> (b ##[1:2] c2) ##0 (ctr == v + 8'd4);
  endproperty
  p_rng_bad: assert property (@(posedge clk) p_rngb);                                   // DIS @13

  // Younger-tap read must keep working.
  property p_young;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> bb[*1:2] ##0 (ctr == v + 8'd1);
  endproperty
  p_young_ok: assert property (@(posedge clk) p_young);                                 // PROVED
endmodule
