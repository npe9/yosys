// Same livelock design as xlive2 without the environment bound: stall can
// stick forever, holding done at 0 -- F done is false.  DIS case (check
// the ic3 verdict line manually; the runner exits nonzero as expected).
module xlive3 (input logic clk, input logic stall);
  logic mode;      // 0 = run, 1 = stall
  logic [1:0] cnt;
  logic done;

  always_ff @(posedge clk) begin
    mode <= stall;
    if (mode == 1'b0) begin
      if (cnt == 2'd3)
        done <= 1'b1;
      else
        cnt <= cnt + 2'd1;
    end
  end

  l1: assert property (@(posedge clk) s_eventually done);  // DIS
endmodule
