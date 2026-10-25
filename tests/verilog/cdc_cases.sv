// cases.sv -- test cases for the yosys `cdc` pass (structural CDC analysis)
//
// Suggested flow (per module, or run `cdc` with no selection to analyze all
// six modules in one go):
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
//   cdc_gray_bus     | 4x WARNING  comb-before-sync    (clk_a -> clk_b, 4 bits: gray_a
//                    |                                is combinational, not a launch flop)
//   cdc_reset        | 2x WARNING  reset-cdc           (external port + foreign domain)
//   cdc_async_fifo   | 8x WARNING  comb-before-sync    (two 4-bit gray pointers, each a
//                    |                                combinational encoder output)
//                    | 1x OK       synchronizer-2ff    (flag, clk_a -> clk_b)
//   cdc_eq_bin       | 4x WARNING  synced-bus          (4-bit binary pointer,
//                    |                                EQ compare is not Gray)
//   cdc_clk_div      | 1x OK       generated-clock     (divide-by-2 from clk_a)
//                    | 1x OK       same-clock-family   (clk_a -> div_clk crossing)
//                    | 4x OK       synchronizer-2ff    (div_clk -> clk_b sync)
//   cdc_gate_icg     | 1x OK       clock-gate-icg      (latch-held enable)
//   cdc_gate_reg     | 1x WARNING  clock-gate-reg      (registered comb AND)
//   cdc_gate_bad     | 1x ERROR    clock-gating-glitch (async enable)
//   cdc_reset_sync   | 2x OK       reset-sync-stage    (the 2 synchronizer FFs)
//                    | 1x OK       reset-sync-deassert (counter reset by tail Q)
//   cdc_constrained  | 1x WARNING  async-declared      (unsynchronized crossing,
//                    |                                SDC-declared async clocks)
//                    | 1x OK       synchronizer-2ff    (unaffected by pairing)
//
// Note: findings group by (dst domain, src domain set); a multi-bit
// group of synced crossings then splits by reconvergence (shared comb
// cell or sink FF in the forward cone).  The gray-bus bits upgrade to
// OK: they match the canonical Gray structure -- a bin->gray $xor
// encoder (gray[i] = w[i] ^ w[i+1] over one common vector) in the
// source domain and, in the destination domain, either a prefix-XOR
// decoder chain over all synced bits (cdc_gray_bus) or an EQ-compare of
// the synced bits against a local Gray-coded vector (cdc_async_fifo
// full/empty pointer checks; per-bit polarity on the synced operand, as
// in the full flag's inverted top bits, is allowed).  A binary pointer
// compared the same way (cdc_eq_bin) fails the encoder shape test and
// stays synced-bus WARNING; the flush flag's cone is disjoint and
// promotes to OK synchronizer-2ff.
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
//     Expect: 4 WARNING (comb-before-sync) -- one line per crossing bit.
//     Audit 2026-10-24: gray_a is the encoder's combinational output, so
//     the synchronizers can sample a multi-bit glitch; this used to be OK
//     gray-bus.  A registered Gray value whose counter steps by one is OK
//     gray-bus (tests/verilog/cdc_audit.ys r8_real_gray).
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
//     Expect: 8 WARNING comb-before-sync (audit 2026-10-24: wgray/rgray are
//     combinational encoder outputs; formerly 8 OK gray-bus: wgray->clk_b 4 bits, rgray->clk_a 4 bits,
//     recognized via the full/empty EQ compares against the local Gray
//     pointers) + 1 OK (synchronizer-2ff for the flag).
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

// ---------------------------------------------------------------------------
// (f) negative case for EQ-compare Gray recognition: a 4-bit *binary*
//     pointer synced through 2FF into the other domain and compared with
//     an $eq, full/empty style, but without Gray coding on the local side.
//     Expect: 4 WARNING (synced-bus) -- the EQ compare alone does not
//     upgrade; the local side fails the bin->gray encoder shape test.
// ---------------------------------------------------------------------------
module cdc_eq_bin (
    input  logic clk_a,
    input  logic clk_b,
    input  logic inc_a,
    input  logic inc_b,
    output logic empty
);
    logic [3:0] wbin;           // write pointer, clk_a domain (binary)
    logic [3:0] rbin;           // read pointer, clk_b domain (binary)
    logic [3:0] wsync1, wsync2; // wbin synchronized into clk_b

    always_ff @(posedge clk_a) wbin <= wbin + inc_a;
    always_ff @(posedge clk_b) rbin <= rbin + inc_b;

    always_ff @(posedge clk_b) wsync1 <= wbin;
    always_ff @(posedge clk_b) wsync2 <= wsync1;

    // reconvergence at an $eq, but the local side is binary, not Gray
    assign empty = (rbin == wsync2);
endmodule

// ---------------------------------------------------------------------------
// (g) generated clock: a divide-by-2 clock derived from clk_a.
//     The div domain is reported OK generated-clock; a clk_a -> div_clk
//     crossing is same-clock-family (OK, no synchronizer needed), while
//     the div -> clk_b crossing is a true CDC and needs the 2FF chain.
//     Expect: 1 OK generated-clock + 1 OK same-clock-family + 4 OK
//     synchronizer-2ff (cnt_div into clk_b).
// ---------------------------------------------------------------------------
module cdc_clk_div (
    input  logic clk_a,
    input  logic clk_b,
    input  logic inc,
    output logic [3:0] cnt_div_b,
    output logic kin_o
);
    // divide-by-2 generated clock (clk_a family)
    logic div_ff;
    always_ff @(posedge clk_a) div_ff <= ~div_ff;

    logic div_clk;
    assign div_clk = div_ff;

    logic [3:0] cnt_div;
    always_ff @(posedge div_clk) cnt_div <= cnt_div + 4'd1;

    // same-family crossing: clk_a domain -> div domain (no synchronizer)
    logic src_a;
    always_ff @(posedge clk_a) src_a <= inc;
    logic kin;
    always_ff @(posedge div_clk) kin <= src_a;

    // true CDC: div domain -> clk_b, properly synchronized
    logic [3:0] m1, m2;
    always_ff @(posedge clk_b) m1 <= cnt_div;
    always_ff @(posedge clk_b) m2 <= m1;
    assign cnt_div_b = m2;
    assign kin_o = kin;
endmodule

// ---------------------------------------------------------------------------
// (h) latch-based integrated clock gate (ICG): enable latched while the
//     clock is low, then ANDed with the clock.  The latch holds the
//     enable stable through the active clock phase, so the gate is
//     glitch-free.
//     Expect: 1 OK clock-gate-icg.
// ---------------------------------------------------------------------------
module cdc_gate_icg (
    input  logic clk,
    input  logic en,
    output logic [3:0] cnt
);
    logic en_l;
    always_latch begin
        if (~clk)
            en_l <= en;
    end

    logic gclk;
    assign gclk = clk & en_l;

    always_ff @(posedge gclk) cnt <= cnt + 4'd1;
endmodule

// ---------------------------------------------------------------------------
// (i) register-held combinational clock gate: the enable is registered in
//     the same domain, but it still changes right after the clock edge
//     while the clock is high -- the AND gate can emit runt pulses.
//     Expect: 1 WARNING clock-gate-reg.
// ---------------------------------------------------------------------------
module cdc_gate_reg (
    input  logic clk,
    input  logic en,
    output logic [3:0] cnt
);
    logic en_r;
    always_ff @(posedge clk) en_r <= en;

    logic gclk;
    assign gclk = clk & en_r;

    always_ff @(posedge gclk) cnt <= cnt + 4'd1;
endmodule

// ---------------------------------------------------------------------------
// (j) glitch-prone combinational clock gate: the enable is an
//     asynchronous input, free to change at any phase of the clock.
//     Expect: 1 ERROR clock-gating-glitch.
// ---------------------------------------------------------------------------
module cdc_gate_bad (
    input  logic clk,
    input  logic en_async,
    output logic [3:0] cnt
);
    logic gclk;
    assign gclk = clk & en_async;

    always_ff @(posedge gclk) cnt <= cnt + 4'd1;
endmodule

// ---------------------------------------------------------------------------
// (k) reset synchronizer: ext_rst_n asserts asynchronously, but the
//     deassertion is synchronized into clk_b by the classic 2-stage
//     chain (head D tied to 1, second stage D = head Q); the fanout FF's
//     async reset is driven by the chain tail.
//     Expect: 2 OK reset-sync-stage (the chain members) + 1 OK
//     reset-sync-deassert (the counter).
// ---------------------------------------------------------------------------
module cdc_reset_sync (
    input  logic clk_a,
    input  logic clk_b,
    input  logic ext_rst_n,
    output logic [3:0] cnt_b
);
    // 2-stage reset synchronizer in the clk_b domain
    logic rst_meta_n, rst_sync_n;
    always_ff @(posedge clk_b or negedge ext_rst_n)
        if (!ext_rst_n)
            rst_meta_n <= 1'b0;
        else
            rst_meta_n <= 1'b1;

    always_ff @(posedge clk_b or negedge ext_rst_n)
        if (!ext_rst_n)
            rst_sync_n <= 1'b0;
        else
            rst_sync_n <= rst_meta_n;

    // fanout flop: async assert, but its deassertion is synchronized
    always_ff @(posedge clk_b or negedge rst_sync_n)
        if (!rst_sync_n)
            cnt_b <= 4'd0;
        else
            cnt_b <= cnt_b + 4'd1;
endmodule

// ---------------------------------------------------------------------------
// (l) SDC pairing: an unsynchronized crossing between clocks the
//     constraints declare asynchronous (cdc_cases.sdc: cclk_a/cclk_b in
//     set_clock_groups -asynchronous, plus a false path cclk_a_fast ->
//     cclk_b; cclk_a carries both clocks).  The waiver downgrades the
//     ERROR to a WARNING async-declared but keeps it visible; the
//     properly synchronized crossing next to it is unaffected.
//     Expect: 1 WARNING async-declared + 1 OK synchronizer-2ff.
// ---------------------------------------------------------------------------
module cdc_constrained (
    input  logic cclk_a,
    input  logic cclk_b,
    input  logic din_c,
    output logic dout_c,
    output logic sync_o
);
    logic a;
    always_ff @(posedge cclk_a) a <= din_c;

    // unsynchronized crossing: pairs with the SDC async declaration
    always_ff @(posedge cclk_b) dout_c <= a;

    // properly synchronized crossing: unaffected by the pairing
    logic m, s;
    always_ff @(posedge cclk_b) m <= a;
    always_ff @(posedge cclk_b) s <= m;
    assign sync_o = s;
endmodule
