# SVA conformance corpus harness

Parses every SystemVerilog file carrying SVA keywords in an external
RTL corpus (OpenTitan, ibex, CVA6, core-v-verif, axi4-fvip, Caliptra)
with the in-tree frontend and tallies failures. Corpora live OUTSIDE
this repo (workspace third_party/); this directory carries the harness
and the recorded results.

    python3 run_corpus.py --name ibex --root <corpus> \
        --yosys ../../build/yosys --prim-assert prim_assert.sv \
        --exclude dv --jobs 8 --timeout 20

`prim_assert.sv` is the conformance macro header: the complete lowRISC
assertion-macro set (prim_assert_standard_macros + prim_assert_sec_cm +
prim_flop_macros) written in the shapes this frontend supports, used to
substitute the upstream headers (whose macro continuations drop
backslashes inside comments). Assertion fragment files (OpenTitan
fpv/vip style) get the header prepended automatically.

Latest sweep: see all_summary.txt (668/668 RTL files across six
corpora, 2026-10-12).
