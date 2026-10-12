module tfeat3(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic a, b, c;
  assign a = (ctr == 8'd10);
  assign b = (ctr == 8'd11);
  assign c = (ctr == 8'd12);

  // local flows through `or` branches (each branch reads the incoming v)
  property p_or;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> ((b ##0 (ctr == v + 8'd1)) or (c ##0 (ctr == v + 8'd2)));
  endproperty
  p_or_ok:  assert property (@(posedge clk) p_or);                                   // PROVED (branch 1 at 11)
  property p_or_b;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> ((b ##0 (ctr == v + 8'd2)) or (c ##0 (ctr == v + 8'd3)));
  endproperty
  p_or_bad: assert property (@(posedge clk) p_or_b);                                 // DIS @11

  // local with range repetition [*1:2]; the consequent starts at 11 (|=>)
  property p_rep;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> (b[*1:2] ##0 (ctr == v + 8'd1 || ctr == v + 8'd2));
  endproperty
  p_rep_ok: assert property (@(posedge clk) p_rep);                                  // PROVED
  property p_rep_b;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> (b[*1:2] ##0 (ctr == v + 8'd3));
  endproperty
  p_rep_bad: assert property (@(posedge clk) p_rep_b);                               // DIS @12

  // local under throughout; the gate must hold at every span tick with the
  // local read at that tick's own age
  property p_thr;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> ((ctr > v) throughout (b ##1 c));
  endproperty
  p_thr_ok: assert property (@(posedge clk) p_thr);                                  // PROVED
  property p_thr_b;
    logic [7:0] v;
    @(posedge clk) (a, v = ctr) |=> ((ctr > v + 8'd1) throughout (b ##1 c));
  endproperty
  p_thr_bad: assert property (@(posedge clk) p_thr_b);                               // DIS @12 (gate fails at 11; failure known at span end 12)
endmodule
