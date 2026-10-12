// Two-clock async FIFO (Gray pointers, 2FF synchronizers, depth 8).
// DISTANCE-invariant stress case: (wbin - rbin) within [0, 8] needs the
// full-flag/empty-flag sync-skew lemmas (true_dist vs synced_dist) and is
// ic3-hard — plain k-induction cannot prove it either (a poisoned
// dist=9 state persists under stuttering without violating the k-window,
// and symmetrically for dist<0 on the read side). BMC holds both to
// bound. x-prefix keeps it out of the t*.sv battery glob; same class as
// the t7/t8 strong-window stress files. tfifo.sv carries the
// ic3-decidable multiclock properties (canary + assume self-consistency).
// PREP-EXTRA-AFTER-MEMORY: clk2fflogic
module xfifo_overrun (
    input  logic       clk_wr,
    input  logic       clk_rd,
    input  logic       wr_en,
    input  logic [7:0] wdata,
    input  logic       rd_en,
    output logic [7:0] rdata,
    output logic       full,
    output logic       empty
);
    logic [3:0] wbin, wgray, rbin, rgray;
    logic [3:0] wgray_r1, wgray_r2; // wgray synced into rd domain
    logic [3:0] rgray_w1, rgray_w2; // rgray synced into wr domain
    logic [7:0] mem [0:7];

    assign full  = (wgray == {~rgray_w2[3:2], rgray_w2[1:0]});
    assign empty = (rgray == wgray_r2);
    assign rdata = mem[rbin[2:0]];

    wire [3:0] wbin_next  = wbin + (wr_en && !full);
    wire [3:0] wgray_next = (wbin_next >> 1) ^ wbin_next;
    wire [3:0] rbin_next  = rbin + (rd_en && !empty);
    wire [3:0] rgray_next = (rbin_next >> 1) ^ rbin_next;

    always_ff @(posedge clk_wr) begin
        if (wr_en && !full)
            mem[wbin[2:0]] <= wdata;
        wbin     <= wbin_next;
        wgray    <= wgray_next;
        rgray_w1 <= rgray;
        rgray_w2 <= rgray_w1;
    end

    always_ff @(posedge clk_rd) begin
        rbin     <= rbin_next;
        rgray    <= rgray_next;
        wgray_r1 <= wgray;
        wgray_r2 <= wgray_r1;
    end

    p_no_overrun:  assert property (@(posedge clk_wr) ((wbin - rbin) <= 4'd8)); // PROVED (BMC-holds; ic3-hard)
    p_no_underrun: assert property (@(posedge clk_rd) (wbin >= rbin));          // PROVED (BMC-holds; ic3-hard)
endmodule
