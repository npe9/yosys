// cases.sv -- test cases for the yosys `cdc` pass (structural CDC analysis)
//
// Suggested flow (per module, or run `cdc` with no selection to analyze all
// five modules in one go):
//
//   read_verilog -sv cases.sv
//   hierarchy -check
//   proc
//   opt_clean
//   cdc
//   cdc -json cdc_findings.json        # optional
//
// Expected results (one table line per crossing bit, plus reset findings):
//
//   module           | findings
//   -----------------+-------------------------------------------------------
//   cdc_sync_2ff     | 1x OK       synchronizer-2ff    (clk_a -> clk_b)
//   cdc_bare         | 1x ERROR    unsynchronized-bit  (clk_a -> clk_b)
//   cdc_gray_bus     | 4x WARNING  synced-bus          (clk_a -> clk_b, 4 bits)
//   cdc_reset        | 2x WARNING  reset-cdc           (external port + foreign domain)
//   cdc_async_fifo   | 9x WARNING  synced-bus          (two 4-bit gray pointers + flag)
//
// Note: findings group by (dst domain, src domain set); the FIFO flag's
// 2FF chain shares its clk_a -> clk_b group with the write-gray pointer,
// so it reports as synced-bus, not synchronizer-2ff (per-structure group
// splitting is a planned refinement; tests/sva_battery/run_cdc.py pins
// the current table).
//
// Note: cdc_reset also contains an async-reset FF whose reset comes from a
// top-level input port; both reset-cdc sub-cases (external port, foreign
// domain FF) are exercised on purpose.

// ---------------------------------------------------------------------------
// (a) two-flop synchronizer: single-bit crossing with a proper sync chain.
//     Expect: 1 OK (synchronizer-2ff), 0 errors, 0 warnings.
// ---------------------------------------------------------------------------
module cdc_sync_2ff (
    input  logic clk_a,
    input  logic clk_b,
    input  logic din,
    output logic dout
);
    // source-domain register
    logic src;
    always_ff @(posedge clk_a) src <= din;

    // two-flop synchronizer in the destination domain
    logic meta;
    always_ff @(posedge clk_b) meta <= src;

    logic sync;
    always_ff @(posedge clk_b) sync <= meta;

    assign dout = sync;
endmodule

// ---------------------------------------------------------------------------
// (b) bare single-bit crossing with no synchronizer.
//     Expect: 1 ERROR (unsynchronized-bit).
// ---------------------------------------------------------------------------
module cdc_bare (
    input  logic clk_a,
    input  logic clk_b,
    input  logic din,
    output logic dout
);
    logic a;
    always_ff @(posedge clk_a) a <= din;

    // direct crossing into the clk_b domain, no synchronization
    always_ff @(posedge clk_b) dout <= a;
endmodule

// ---------------------------------------------------------------------------
// (c) 4-bit Gray counter crossing with per-bit two-flop sync and a Gray
//     decoder on the far side.
//     Expect: 4 WARNING (synced-bus) -- one line per crossing bit.
// ---------------------------------------------------------------------------
module cdc_gray_bus (
    input  logic clk_a,
    input  logic clk_b,
    input  logic inc,
    output logic [3:0] bin_b
);
    logic [3:0] bin_a, bin_next, gray_a;
    logic [3:0] gray_b1, gray_b2;

    // binary counter and Gray encoder in the clk_a domain
    assign bin_next = bin_a + 4'd1;
    assign gray_a   = bin_next ^ (bin_next >> 1);

    always_ff @(posedge clk_a)
        if (inc)
            bin_a <= bin_next;

    // per-bit two-flop synchronizer in the clk_b domain
    always_ff @(posedge clk_b) gray_b1 <= gray_a;
    always_ff @(posedge clk_b) gray_b2 <= gray_b1;

    // Gray-to-binary decode in the clk_b domain
    assign bin_b[3] = gray_b2[3];
    assign bin_b[2] = gray_b2[3] ^ gray_b2[2];
    assign bin_b[1] = gray_b2[3] ^ gray_b2[2] ^ gray_b2[1];
    assign bin_b[0] = gray_b2[3] ^ gray_b2[2] ^ gray_b2[1] ^ gray_b2[0];
endmodule

// ---------------------------------------------------------------------------
// (d) asynchronous reset from another domain.
//     Expect: 2 WARNING (reset-cdc): rst_a_n reset by external port ext_rst_n,
//     and cnt_b reset by the clk_a-domain signal rst_a_n.
// ---------------------------------------------------------------------------
module cdc_reset (
    input  logic clk_a,
    input  logic clk_b,
    input  logic ext_rst_n,
    output logic [3:0] cnt_b
);
    // reset generator in the clk_a domain, reset by an external port
    logic rst_a_n;
    always_ff @(posedge clk_a or negedge ext_rst_n)
        if (!ext_rst_n)
            rst_a_n <= 1'b0;
        else
            rst_a_n <= 1'b1;

    // clk_b-domain counter with an async reset from the clk_a domain
    always_ff @(posedge clk_b or negedge rst_a_n)
        if (!rst_a_n)
            cnt_b <= 4'd0;
        else
            cnt_b <= cnt_b + 4'd1;
endmodule

// ---------------------------------------------------------------------------
// (e) async FIFO: two clock domains, Gray pointers crossing in both
//     directions (each synchronized with a 2x4-bit two-flop chain), plus one
//     properly synchronized single-bit flag crossing.
//     Expect: 8 WARNING (synced-bus: wgray->clk_b 4 bits, rgray->clk_a 4
//     bits) + 1 OK (synchronizer-2ff for the flag).
// ---------------------------------------------------------------------------
module cdc_async_fifo (
    input  logic clk_a,
    input  logic clk_b,
    input  logic wr_en,
    input  logic rd_en,
    input  logic [7:0] wdata,
    input  logic flush_req_i,
    output logic [7:0] rdata,
    output logic full,
    output logic empty,
    output logic flush_done
);
    logic [3:0] wbin, wgray;    // write pointer, clk_a domain
    logic [3:0] rbin, rgray;    // read pointer, clk_b domain
    logic [3:0] wbin_n, rbin_n;
    logic [3:0] wsync1, wsync2; // wgray synchronized into clk_b
    logic [3:0] rsync1, rsync2; // rgray synchronized into clk_a
    logic       flush_a, fsync1;

    logic [7:0] mem [0:7];

    // ---- write clock domain (clk_a) ----
    assign wbin_n = wbin + (wr_en & ~full);
    assign wgray  = wbin_n ^ (wbin_n >> 1);

    always_ff @(posedge clk_a) wbin <= wbin_n;

    always_ff @(posedge clk_a)
        if (wr_en & ~full)
            mem[wbin[2:0]] <= wdata;

    // ---- read clock domain (clk_b) ----
    assign rbin_n = rbin + (rd_en & ~empty);
    assign rgray  = rbin_n ^ (rbin_n >> 1);

    always_ff @(posedge clk_b) rbin <= rbin_n;

    assign rdata = mem[rbin[2:0]];

    // ---- pointer crossings, both directions ----
    always_ff @(posedge clk_b) wsync1 <= wgray;
    always_ff @(posedge clk_b) wsync2 <= wsync1;

    always_ff @(posedge clk_a) rsync1 <= rgray;
    always_ff @(posedge clk_a) rsync2 <= rsync1;

    // ---- status flags ----
    assign full  = (wgray == {~rsync2[3:2], rsync2[1:0]});
    assign empty = (rgray == wsync2);

    // ---- single-bit flag crossing, properly synchronized ----
    always_ff @(posedge clk_a) flush_a <= flush_req_i;
    always_ff @(posedge clk_b) fsync1 <= flush_a;
    always_ff @(posedge clk_b) flush_done <= fsync1;
endmodule
