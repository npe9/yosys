# SVA verification battery

SystemVerilog-assertion battery for the sv-yosys frontend (`sva2check`),
the native `ic3` pass, and the `write_smt2` + `yosys-smtbmc` BMC flow.

## Running

In-tree (cmake build tree; runs the whole gate below):

    cmake --build <build-dir> --target test-sva

Individual pieces from anywhere (the runners resolve the yosys binary
via the `YOSYS` env var; default `third_party/yosys/build/yosys`
relative to the repo root):

    make test            # in this directory: battery + cdc + seeded fuzz
                         # + conductor leg (with --cex replay checks) + xprop legs
    python3 run_ic3.py <file>.sv --timeout 90     # native IC3/PDR, no external solver
    python3 run_ic3.py <file>.sv --cex <dir>/     # ... plus a .yw witness per disproof
    python3 run_bmc.py <file>.sv                  # write_smt2 + yosys-smtbmc (z3 default, -s boolector)
    python3 run_bmc.py <file>.sv --dump-yw <dir>  # ... plus a .yw witness per BMC round
    python3 prove.py <file>.sv...                 # conductor: both lanes in parallel,
                                                  # first-to-finish per property
    python3 prove.py <file>.sv --cex <dir>        # ... and re-verify every safety
                                                  # disproof by sim -r witness replay
    python3 prove.py --smt-only / --ic3-only f.sv # single-lane runs (bisect conflicts)

LSP diagnostics server (register item "language server", v1: diagnostics
only -- full-sync re-lint through the yosys frontend, no navigation yet):

    python3 lsp.py --yosys $YOSYS        # stdio JSON-RPC server for editors
    python3 run_lsp.py                   # protocol gate (make test leg)

Both drivers exit 0 iff every property matches its expectation.
The conductor additionally records which lane settled each property and
exits 1 on cross-engine conflicts (a cex depth disagreement, or one lane
proving what the other disproved -- both engines are sound on the
identical prep, so that means an engine bug; the sequential battery legs
remain the referee).

With `--cex <dir>` the conductor keeps per-property artifacts under
`<dir>/<file>/`: `ic3_<prop>.yw` / `smt_rN.yw` witnesses from both lanes,
plus `replay_<prop>.{vcd,json,ys}` from replaying each safety disproof on
the settling lane's own prep (`sim -r <w> -summary <json> -vcd <vcd>`);
the battery requires the replay summary to record the assertion firing at
exactly the recorded cex step (REPLAY-OK).  Liveness (`$live` leg)
disproofs are lasso traces and are not replay-checked.

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

## Lowering provenance (`sva_lowering` tags)

`sva2check` writes `(* sva_lowering = "..." *)` on every `$check` it
touches, and every verdict printer in this directory (`run_ic3.py`,
`run_bmc.py`, `prove.py`, `run_battery.py`) prints it as
`[lowering=...]` next to the verdict, so a PROVED never hides the way
the property was compiled.  `run_battery.py` ends with a histogram of
the tags over all expectations; the gate is that no line says
`untagged`.

| tag | meaning |
|---|---|
| `exact` | compiled by the documented single-monitor lowering with no approximation taken |
| `approx:<kind>[,<kind>]` | one or more of the approximations below was taken; each is also a warning naming the check |
| `placeholder:<why>` | the check was skipped (cover flavor, no sampling clock, malformed/non-`$sva` spec); its `A` port is still the frontend placeholder and no engine may trust it |
| `immediate` / `plain` | a non-temporal check the frontend already compiled (immediate assertion / clocked plain expression) |

`sva2check -exact` turns every approximation into an error.  The
approximation kinds, what they can miss, the battery files that depend
on them (from `sva2check -exact` over every `t*.sv`, 2026-10-17) and the
positive control that shows each detector firing (`xapprox.sv`, plain /
`SVA_NOSPLIT=1` / `SVA_NOANC=1`; `t8.sv` for until):

| kind | where | what can be missed | battery files | detector seen firing |
|---|---|---|---|---|
| `first-match-overlap-window` | `first_match` over a ranged `##[lo:hi]` or `or` | an earlier parse end that belongs to ANOTHER overlapping attempt (beyond the static window) suppresses this attempt's `.first` | tfeat1, tfeat4 | xapprox p_fm_win |
| `first-match-any` | `first_match` over `and`/`intersect`, unbounded repetition, `[->]`/`[=]`, `within`, an instance body, or when the split budget is spent | `.first` degrades to the existential `.any` (later matches count) | none | xapprox p_fm_and |
| `first-match-locals-shared-slots` | `first_match` of a sequence with local variables | locals read from the shared slot file, not per parse | none | xapprox p_fm_loc |
| `locals-lowest-hot-age` | a local-variable read at a point several attempt ages can reach | the lowest hot age wins; violations visible only to a younger overlapping attempt are missed | none (the age fold keeps linear sequences one-hot; the per-offset split keeps ranged ones) | xapprox p_loc_win, p_fm_loc under SVA_NOSPLIT=1 |
| `locals-merged-window` | ranged `##` whose tail is an `$sva` subtree reading locals and could not be split per offset | tail compiled once over the merged window | none | not observed: an expression tail reading a local shows up as `locals-lowest-hot-age` (the actual effect); an `$sva` tail that defeats the split was not constructed |
| `noncons-rep-merged-counter` | `[->m:n]`/`[=m:n]` with a finite bound inside a `##` chain, outside the anchored per-attempt path | the merged counter only checks `k >= m` (upper bound vacuous) | none | xapprox p_nc_loc under SVA_NOANC=1 |
| `noncons-rep-seq-body-merged` | `[->]`/`[=]` over a SEQUENCE body (beyond the LRM's Boolean form) | occurrence ends of overlapping body attempts merge | tnoncons1 | xapprox p_ncseq |
| `noncons-rep-unbounded-top-age` | anchored `[->]`/`[=]` in an unbounded property | the saturated top age falls back to the time-0 count | none | xapprox p_nc_loc |
| `until-merged-attempts` | `until`/`until_with` not at the property root (pulse start) | a `q` seen by an earlier attempt does not close the attempt starting now; one monitor merges them | t8 | t8 p_utw_b |
| `weak-as-strong` | `weak(seq)` as a property | compiled strongly: sound for model checking, conservative at the end of a finite simulation trace | tfeat1, twithin, tdeclclk | xapprox p_weak |

The positive controls earned their keep immediately: no detector ever
fired on a NAMED property, because `assert property (@(posedge clk) q)`
with an unclocked `property q` was being lowered to `!q` on the undriven
name wire and reported as converted (fixed the same day; `tdeclclk.sv`
pins it).

Not in this table because they are bugs, not documented approximations:
the single-monitor divergences F2-F6 and F10 listed in
`tests/sva_pbt/README.md` (non-consecutive repetition ends, `##0` with
goto, sequence properties firing late or hidden by a neighbouring
attempt, bounded `s_eventually`, `or`/`implies` deciding early).  Their
fix is per-attempt tracking (register v3 D5 / C0), after which this table
shrinks with it.

## Strict reads and the `READ-FLAGS` pragma

Since register v3 B4 every frontend degradation (a dropped loop or
block, a constant-0 substitution, an unknown type read as `logic`, an
unresolved function or task, an implicit net under `default_nettype
none`, an always_comb latch, ...) is an ERROR in `read_verilog`;
`read_verilog -permissive` (and `hierarchy -check -permissive` for a
missing module) accepts it with a warning per occurrence and appends
`"<reason> @file:line"` to the module's `(* sv_degraded *)` attribute,
and marks every module `(* sv_permissive *)`.  The battery reads
strictly.  A file that documents a leniency it relies on carries
`// READ-FLAGS: -permissive` (run_ic3/run_bmc/prove honour it; today
only `tfix_b.sv`, for a conditional `disable` that the frontend
ignores).  The LSP lints with `-permissive` (an editor wants
diagnostics, not a hard stop).  The `*_tb.sv` files that instantiate a
dut from a sibling file are the documented "missing dut" edge in
run_battery.py: standalone they now fail `hierarchy -check` instead of
reaching "no summary".

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
header carries the clock and `sim -r` rejects `-clock`. Note: since
2026-10-16 neither `-kind 0` nor registers are required for cex export --
the exporter re-derives the witness by an exact unroll on its own solver
(`export_cex`), pre-pass disproofs export directly without re-running the
IC3 loop, and a purely combinational depth-0 disproof exports a
single-step witness of its failing inputs (tprove0).

## Known edges (fail by design, do not "fix" without a plan entry)

- `t30a.sv` — unsupported syntax (aborts in the parser).
