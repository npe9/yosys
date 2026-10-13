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
- `tchk*.sv` — checker blocks (IEEE 1800-2009 17): checkers as reusable
  property containers (ports, nested instantiation, checker-local
  variables renamed per instance, `default clocking` inside a checker).
  Instances are inlined at the AST during simplify, so verdicts keep the
  assertion labels from the checker body; a checker instantiated more
  than once reports the first instance's copies under the bare labels and
  later ones under genrtlil's auto-suffixed duplicates.
- `xlive*.sv` — liveness (`s_eventually` of a plain expression), handled
  by ic3's native two-phase gfp engine, not by BMC (`run_bmc.py` does not
  support liveness). Expectations are `// PROVED` / `// DIS` with no `@N`
  (disproofs report depth 0; with `-cex <prefix>` a disproof additionally
  writes a lasso witness `<prefix><name>.yw`, replayable with the same
  prep plus `setundef -init -zero` via `sim -r <file>.yw -vcd trace.vcd`).
  Read the verdict lines manually -- files with a `DIS` expectation exit
  nonzero from `run_ic3.py` by design.
- `tmcloc1.sv` — multiclock local-variable scoreboard (8-bit data,
  strong `##[2:3]`, DIS pair). Ic3-stress: the slot/island state space
  grinds generalization (t7 class); use `--timeout 500`. `tmcloc2.sv`
  is the green 1-bit sibling (in the `t*.sv` glob).
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

A third pragma opts the file into the ic3 bounded-reachability pre-pass:

    // IC3-BMC: <depth>                        (passed as `ic3 -bmc <depth>`)

Deep counterexamples that would need one IC3 frame per step (t7's
ctr-wrap CEXes at depth 256/259) are found in <depth> cheap unrolled
solves instead; a miss falls through to k-induction and IC3 as usual.

## Counterexample minimization

`min_cex.py` delta-debugs an `ic3 -cex` safety witness (.yw): it replays
the witness through the battery prep (same pipeline as run_ic3.py plus
`setundef -init -zero`, with the PREP-EXTRA-* pragmas honored; the prepped
RTLIL is written once to a temp dir and every trial replays via
`yosys -p "read_rtlil prep.il; sim -r trial.yw -summary sum.json"`),
then greedily clears data-input bits of the steps up to the fired step k
and truncates the suffix, keeping a change iff the SAME assertion still
fires at the SAME step k:

    python3 min_cex.py <file>.sv <prefix><prop>.yw [--top TOP] [--jobs 4] [-o OUT.yw]

Output defaults to `<prefix><prop>.min.yw`. The target assertion is the
rarest-firing path in the replay summary (a collateral constant-false
property, e.g. tconst1's p1, fires at every step and is not chased);
prefix steps are never removed -- indices would shift and step k would
no longer mean the same cycle. `--clock` is accepted but unused: the yw
header carries the clock and `sim -r` rejects `-clock`. Note: `ic3`'s
default k-induction pre-pass disproves on a throwaway solver, so cex
export currently requires `-kind 0` (and a design with registers -- a
purely combinational disproof has no state vector to export).

## Known edges (fail by design, do not "fix" without a plan entry)

- `t30a.sv` — unsupported syntax (aborts in the parser).
