// Resolvable const-selector case: apply_force_override should still fold arms.
module tfix_a(input logic [1:0] sel, output logic [3:0] y);
  always_comb begin
    y = 4'h0;
    case (2'b01)
      2'b01: y = 4'h5;
      2'b10: y = 4'ha;
      default: y = 4'hf;
    endcase
  end
endmodule
