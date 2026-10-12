module t42(input logic clk, output logic [7:0] y);
  logic [3:0] tag = 4'b0100;
  logic [1:0] fsm_q = 0;
  logic [7:0] cnt = 0;
  always_ff @(posedge clk) begin
    cnt <= cnt + 1;
    if (cnt == 3) fsm_q <= 1;
  end
  always_comb begin
    y = 8'h00;
    if (cnt >= 2) begin
      unique case (fsm_q)
        2'd0: begin
          unique case (1'b1)
            tag[0]: y = 8'h11;
            tag[1]: y = 8'h22;
            tag[2]: y = 8'h33;
            tag[3]: y = 8'h44;
            default: y = 8'h55;
          endcase
        end
        default: y = 8'h66;
      endcase
    end
  end
endmodule
