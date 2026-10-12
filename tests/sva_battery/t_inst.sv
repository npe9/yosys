// Named sequence/property instance stress tests.
// ctr free-runs 0,1,2,...; a/b/c/d pulse at ctr==10/11/12/13; lvl spans 20..21.
module t_inst (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b, c, d, lvl;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd11);
  assign c = (ctr == 8'd12);
  assign d = (ctr == 8'd13);
  assign lvl = (ctr >= 8'd20) && (ctr < 8'd22);

  // 1. nested instance: s2 calls s1.  s2(a) = a@t, b@t+1, c@t+2 -> 10,11,12.
  sequence s1(x); x ##1 b; endsequence
  sequence s2(y); s1(y) ##1 c; endsequence
  p_s2_ok: assert property (@(posedge clk) s2(a) |-> (ctr == 8'd12));  // PROVED
  p_s2_b:  assert property (@(posedge clk) s2(a) |-> (ctr == 8'd13));  // DIS @12

  // 2. instance with expression actual.  s3(x) = ##2 x.
  sequence s3(x); ##2 x; endsequence
  p_s3_ok: assert property (@(posedge clk) a |-> s3(ctr == 8'd12));    // PROVED
  p_s3_b:  assert property (@(posedge clk) a |-> s3(ctr == 8'd13));    // DIS @12

  // 3. property instance with local + constant arg k.
  property p4(sig, k);
    logic [7:0] got;
    @(posedge clk) (sig, got = ctr) |=> (ctr == got + k);
  endproperty
  p_p4_ok: assert property (@(posedge clk) p4(a, 8'd1));               // PROVED
  p_p4_b:  assert property (@(posedge clk) p4(a, 8'd2));               // DIS @11

  // 4. caller local passed into an instance.  p8(g) = ##1 (ctr == g+2).
  sequence s8(g); ##1 (ctr == g + 8'd2); endsequence
  property p7;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> s8(v);
  endproperty
  p7_ok:   assert property (@(posedge clk) p7);                        // PROVED
  property p7b;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> s8b(v);
  endproperty
  sequence s8b(g); ##1 (ctr == g + 8'd3); endsequence
  p7_b:    assert property (@(posedge clk) p7b);                       // DIS @12

  // 5. name-only property instance, positive and negative.
  property p6;  @(posedge clk) a |=> b; endproperty
  p_name:  assert property (@(posedge clk) p6);                        // PROVED
  property p6n; @(posedge clk) a |=> c; endproperty
  p_name_b: assert property (@(posedge clk) p6n);                     // DIS @11

  // 6. chained calls: sb uses sc twice.  sb(x,y) = x@t, y@t+2.
  sequence sc(x); ##1 x; endsequence
  sequence sb(x, y); sc(x) ##0 sc(y); endsequence
  p_sb_ok: assert property (@(posedge clk) sb(a, c) |-> (ctr == 8'd12)); // PROVED
  p_sb_b:  assert property (@(posedge clk) sb(a, d) |-> (ctr == 8'd12)); // vacuous: 13!=12 -> PROVED
  p_sb2_b: assert property (@(posedge clk) sb(a, c) |-> (ctr == 8'd13)); // PROVED

  // 7. let used inside a sequence body.
  let eqctr(x) = (ctr == x);
  sequence slet; ##1 eqctr(8'd11); endsequence
  p_let:   assert property (@(posedge clk) a |-> slet);                // PROVED
endmodule
