// Per-attempt slot isolation: two overlapping attempts carry distinct
// values; a shared shadow register would clobber attempt@10 with
// attempt@11's capture and falsely report a violation at 11.
module tslot2(input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic req; assign req = (ctr == 8'd10) || (ctr == 8'd11);
  logic [7:0] dat; assign dat = ctr;
  property p;
    logic [7:0] v;
    @(posedge clk) (req, v = dat) |=> (dat == v + 8'd1);
  endproperty
  c: assert property (@(posedge clk) p);          // PROVED (slots); DIS with shared shadow
  property p2;
    logic [7:0] v;
    @(posedge clk) (req, v = dat) |=> (dat == v); // DIS @11: attempt@10 sees 11 != 10
  endproperty
  c2: assert property (@(posedge clk) p2);
endmodule
