// Livelock liveness case for the native ic3 pass, with the environment
// bounded by an assume: the stall input may not stick, so done eventually
// rises.  PROVED case (runner exits 0).
module xlive2 (input logic clk, input logic stall);
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

  // environment: stall never sticks (mode is the registered stall)
  a_run: assume property (@(posedge clk) mode == 1'b0);
  l1: assert property (@(posedge clk) s_eventually done);  // PROVED
endmodule
