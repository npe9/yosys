module tslot(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic req;
  assign req = (ctr == 10) || (ctr == 11);   // back-to-back: overlapping attempts
  logic [7:0] data;
  assign data = (ctr == 10) ? 8'd42 : 8'd77;

  // attempt@10 captures 42, checks at 11; attempt@11 captures 77, checks at
  // 12.  A shared slot is clobbered by the second capture before the first
  // check (false violation); per-attempt slots prove.
  property p;
    logic [7:0] v;
    @(posedge clk) (req, v = data) |=> (v == ((ctr == 8'd11) ? 8'd42 : 8'd77));
  endproperty
  ok:  assert property (@(posedge clk) p);   // PROVED (slots)

  property pb;
    logic [7:0] v;
    @(posedge clk) (req, v = data) |=> (v == ((ctr == 8'd11) ? 8'd77 : 8'd42));
  endproperty
  bad: assert property (@(posedge clk) pb);  // DIS @11
endmodule
