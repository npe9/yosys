// PREP-EXTRA-AFTER-MEMORY: clk2fflogic
// Minimal multiclock-local scoreboard: 1-bit data, 1-stage read sync, ##1.
// IC3-TIMEOUT: 600
module tmcloc2(input logic wr_clk, input logic rd_clk,
               input logic push_pulse, input logic din);
  logic wr_seen, mem_v, v0, pop, rdata;
  always @(posedge wr_clk)
    if (push_pulse && !wr_seen) begin
      mem_v   <= din;
      wr_seen <= 1'b1;
    end
  always @(posedge rd_clk) begin
    v0    <= wr_seen;
    rdata <= mem_v;
  end
  assign pop = v0;
  property p_ok;
    logic v;
    @(posedge wr_clk) ($rose(wr_seen), v = mem_v)
      |-> @(posedge rd_clk) ##1 (pop && rdata == v);
  endproperty
  ok:  assert property (@(posedge wr_clk) p_ok);  // PROVED
  property p_bad;
    logic v;
    @(posedge wr_clk) ($rose(wr_seen), v = mem_v)
      |-> @(posedge rd_clk) ##1 (pop && rdata == !v);
  endproperty
  bad: assert property (@(posedge wr_clk) p_bad);  // DIS
endmodule
