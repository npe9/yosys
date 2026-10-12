module tfix_c(input logic [3:0] tag, output logic [7:0] y, output logic [7:0] z, output logic [7:0] w);
  always_comb begin
    y = 8'h00; z = 8'h00; w = 8'h00;
    // reverse casez: wildcard arm types must not be resolved/erased
    casez (1'b1)
      tag[0]: y = 8'h11;
      tag[1]: y = 8'h22;
      default: y = 8'h55;
    endcase
    // reverse case without default: no arms may be dropped
    case (1'b1)
      tag[2]: z = 8'h33;
      tag[3]: z = 8'h44;
    endcase
    // constant selector + all-constant compares: first match wins, foldable
    case (2'b10)
      2'b10: w = 8'haa;
      2'b1x: w = 8'hbb;
      default: w = 8'hcc;
    endcase
  end
endmodule
