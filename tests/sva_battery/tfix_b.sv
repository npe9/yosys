// Reverse case inside named block with disable in the default arm.
// When tag[0]=1 the disable must NOT fire, so the trailing y<=8'h66 wins.
// READ-FLAGS: -permissive
// (a conditional `disable blk` is a documented frontend degradation --
// "disable that is not an unconditional exit of its named block is
// ignored" -- so the strict read refuses this file; the expectation
// below only exercises the arm where the disable does not fire)
module tfix_b(input logic clk, input logic [3:0] tag, output logic [7:0] y);
  always @(posedge clk) begin : blk
    y <= 8'h00;
    case (1'b1)
      tag[0]: y <= 8'h11;
      default: begin
        y <= 8'h55;
        disable blk;
      end
    endcase
    y <= 8'h66;
  end
endmodule
