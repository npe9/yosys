# SVA verification battery

SystemVerilog-assertion battery for the sv-yosys frontend (`sva2check`),
the native `ic3` pass, and the `write_smt2` + `yosys-smtbmc` BMC flow.

## Running

From anywhere (the runners resolve the yosys binary via the `YOSYS` env
var; default `third_party/yosys/build/yosys` relative to the repo root):

    python3 run_ic3.py <file>.sv --timeout 90     # native IC3/PDR, no external solver
    python3 run_bmc.py <file>.sv                  # write_smt2 + yosys-smtbmc (z3 default, -s boolector)

Both drivers exit 0 iff every property matches its expectation.

## Expectations

Trailing comments on each `assert property` line:

    p_name: assert property (...); // PROVED
    p_name: assert property (...); // DIS @12

`PROVED` = no counterexample exists (ic3: inductive proof; BMC: no CEX to
bound). `DIS @N` = counterexample at step N, where

    N = ctr value = BMC step = ic3 frame - 1

`assume property` lines carry no expectation; they constrain the
environment (proved soundness: an assume+assert pair on the same
expression proves the engine honors the assumption).

## File classes

- `t*.sv` — the green battery. Run all: `for f in t*.sv; do ...`.
- `xfifo_dist.sv` — stress case, outside the `t*.sv` glob on purpose:
  pointer-distance invariants of a two-clock async FIFO that are ic3-hard
  (CTI explosion near stale-flag boundaries) and k-induction-hard
  (poisoned distance states persist under stuttering). BMC-holds to
  bound. Same class as the historic t7 strong-window timeout.

## Multiclock files

Files with a two-clock design (e.g. `tfifo.sv`, `tmcdom1.sv`,
`xfifo_dist.sv`) opt into the `clk2fflogic` interleaving prep via pragma
comments at the top of the file:

    // PREP-EXTRA-AFTER-ASYNC2SYNC: <passes>   (inserted after async2sync)
    // PREP-EXTRA-AFTER-MEMORY: <passes>       (inserted after memory; this is
                                              where clk2fflogic goes — see PLAN.md)

Files without pragmas keep the standard single-clock prep exactly.

## Known edges (fail by design, do not "fix" without a plan entry)

- `t30a.sv` — unsupported syntax (aborts in the parser).
- `t7.sv` — ic3 timeout (strong-windowed consequent scalability; see
  PLAN.md "ic3 pass scalability").
