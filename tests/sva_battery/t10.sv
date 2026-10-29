// IC3-TIMEOUT: 300
module t10 (input logic clk);
  logic [7:0] ctr;
  always_ff @(posedge clk) ctr <= ctr + 8'd1;
  logic [127:0] block_msg;
  logic a;
  assign a = (ctr == 8'd10);
  assign block_msg = {120'd0, ctr};

  function automatic logic [127:0] add_round(logic [127:0] x, logic [127:0] b);
    return x + b;
  endfunction

  // caliptra fv_doe_encrypt shape: request-gated antecedent, tracked local,
  // computed result, checked one cycle later
  property p_crypt;
    logic [127:0] tracked, result;
    @(posedge clk) (a, tracked = block_msg)
      ##0 (1'b1, result = tracked + 128'd5)
      |->
      ##1 (block_msg == result - 128'd4);
  endproperty

  property p_cryptb;
    logic [127:0] tracked, result;
    @(posedge clk) (a, tracked = block_msg)
      ##0 (1'b1, result = tracked + 128'd5)
      |->
      ##1 (block_msg == result);
  endproperty

  // (PROVED: result-4 = tracked+1 = next block_msg; a DIS @11 here was
  // the same-tick read of tracked before its write landed, fixed in fe52)
  p_crypt_a: assert property (p_crypt);   // PROVED
  p_crypt_b: assert property (p_cryptb);  // DIS @11
endmodule
