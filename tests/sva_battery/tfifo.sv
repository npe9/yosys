// Two-clock async FIFO (Gray pointers, 2FF synchronizers, depth 8).
// Layer A multiclock test: multiclock DESIGN, single-clock properties.
// clk2fflogic goes AFTER memory so memory_map still sees a clocked
// (sync-write) memory and emits $dff; clk2fflogic then removes the clocks.
// PREP-EXTRA-AFTER-MEMORY: clk2fflogic
module tfifo (
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

    // canary: one write does not fill a depth-8 FIFO (would mean full is
    // stuck or the pointer math is wrong) — must DIS
    p_full_canary: assert property (@(posedge clk_wr) wr_en |=> full);          // DIS @1

    // assume/assert self-consistency pairs: identical property as assume +
    // as assert. If ic3 dropped the $assume from its transition relation,
    // the assert DISes (wr_en && full is reachable at ~step 15 unconstrained,
    // as the first run of this file showed) — so PROVED here proves assumes
    // are honored across clock domains.
    a_prot_wr:     assume property (@(posedge clk_wr) !(wr_en && full));
    p_prot_wr:     assert property (@(posedge clk_wr) !(wr_en && full));       // PROVED
    a_prot_rd:     assume property (@(posedge clk_rd) !(rd_en && empty));
    p_prot_rd:     assert property (@(posedge clk_rd) !(rd_en && empty));      // PROVED
endmodule
