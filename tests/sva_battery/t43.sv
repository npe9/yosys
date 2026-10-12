module t43(input logic [3:0] tag, output logic [7:0] y);
  always_comb begin
    y = 8'h00;
    case (1'b1)
      tag[0]: y = 8'h11;
      tag[1]: y = 8'h22;
      tag[2]: y = 8'h33;
      default: y = 8'h55;
    endcase
  end
endmodule
