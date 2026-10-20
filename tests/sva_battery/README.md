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
`SVA_NOSPLIT=1`; the former `until-merged-attempts` kind is gone: one live bit is exact for a weak until, 2026-10-19):

| kind | where | what can be missed | battery files | detector seen firing |
|---|---|---|---|---|
| `first-match-overlap-window` | `first_match` over a ranged `##[lo:hi]` or `or` | an earlier parse end that belongs to ANOTHER overlapping attempt (beyond the static window) suppresses this attempt's `.first` | tfeat1, tfeat4 | xapprox p_fm_win |
| `first-match-any` | `first_match` over `and`/`intersect`, unbounded repetition, `[->]`/`[=]`, `within`, an instance body, or when the split budget is spent | `.first` degrades to the existential `.any` (later matches count) | none | xapprox p_fm_and |
| `first-match-locals-shared-slots` | `first_match` of a sequence with local variables | locals read from the shared slot file, not per parse | none | xapprox p_fm_loc |
| `locals-lowest-hot-age` | a local-variable read at a point several attempt ages can reach | the lowest hot age wins; violations visible only to a younger overlapping attempt are missed | none (the age fold keeps linear sequences one-hot; the per-offset split keeps ranged ones) | xapprox p_loc_win, p_fm_loc under SVA_NOSPLIT=1 |
| `locals-merged-window` | ranged `##` whose tail is an `$sva` subtree reading locals and could not be split per offset | tail compiled once over the merged window | none | not observed: an expression tail reading a local shows up as `locals-lowest-hot-age` (the actual effect); an `$sva` tail that defeats the split was not constructed |
| `noncons-rep-merged-counter` | `[->m:n]`/`[=m:n]` whose occurrence-count set would exceed 65 states (n > 64) | the merged counter only checks `k >= m` (upper bound vacuous) and counts from the first attempt | none | xapprox p_nc_big |
| `noncons-rep-seq-body-merged` | `[->]`/`[=]` over a SEQUENCE body (beyond the LRM's Boolean form) | occurrence ends of overlapping body attempts merge | tnoncons1 | xapprox p_ncseq |
| `empty-match-operand` | an `and` / `intersect` / `within` / `first_match` operand that can match empty (`[*0..]`, `[*]`) | the operand's empty match is dropped (it has no tick; inside `##` it is handled exactly) | none (2026-10-19) | xapprox p_empty_op |
| `prop-binop-unaligned` | property `or` / `implies` / `iff` with an operand whose failure tick varies per attempt (until, ranged always, variable-length antecedent, early-form sequences) | operand fail pulses of different attempts can pair (false counterexamples) or miss each other (missed failures); fixed-offset operands are aligned exactly | none (2026-10-19) | xapprox p_or_unal |
| `eventually-unaligned` | `(s_)eventually [m:n] p` with such a `p` | p's per-tick failures are paired as if p resolved at its own tick | none (2026-10-19) | xapprox p_ev_unal |
| `strong-seq-merged-attempts` | a variable-length sequence in property position (implication consequent, sequence property) in slot mode (locals), a multi-clock island, or with a window > 16 ticks (env `SVA_MERGED_STRONG=1` forces it) | one monitor serves overlapping attempts, so an end belonging to a LATER attempt inside this attempt's window excuses its miss | tfeat3, tfeat5, tmcdom1 (2026-10-19; `-exact` stops at a file's first approximation) | xapprox p_ssm (also on p_nc_loc, p_fm_loc: locals) |
| `seq-binop-merged-attempts` | `and` / `intersect` / `within` with a variable-length operand in slot mode, a multi-clock island, or an attempt span > 16; `intersect` / `within` with an unbounded operand | operand ends are paired across overlapping attempts (one attempt's end completes another's pair) | none (2026-10-19) | xapprox p_binop |
| `late-failure-tick` | an assume/restrict (or `-early`) sequence whose early failure cannot be built: slot mode (locals), multi-clock islands, windows > 16, a monitor holding counters, or an operand of `or` / `implies` / `iff` / `s_eventually` (compiled in the deadline form so it can be aligned exactly) | the failure is reported at the deadline, not the LRM tick; for an assumption a short trace can slip past it (spurious counterexamples, never false proofs) | none (2026-10-19) | xapprox q_late |
| `weak-as-strong` | `weak(seq)` as a property | compiled strongly: sound for model checking, conservative at the end of a finite simulation trace | tfeat1, twithin, tdeclclk | xapprox p_weak |

Failure ticks (register v3 D5): a sequence in property position fails at
its deadline (its longest match length) unless the check is an
assume/restrict or sva2check runs with `-early` (pragma
`// SVA2CHECK-FLAGS: -early`; env `SVA_EARLY=1` for test sweeps), which
report the failure at the tick no thread of the attempt survives -- the
LRM's tick.  Verdicts of asserts are the same either way (an attempt dead
early also fails at its deadline); the early form costs one monitor copy
per attempt slot.  Assumptions need it: the late form accepted prefixes the
LRM already rejects (tassume_early.sv).  Expectations that differ carry an
`LRM @N` note; tearly.sv pins the early ticks.

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
diagnostics, not a hard stop).  A bare reference to a named
sequence/property/let inside an assertion is the wire sva2check resolves
by name, and an undeclared local assigned in a match item (`(e, w = x)`)
is the assertion's local variable: neither is an implicit-net warning nor
a degradation (tests/verilog/sva_decl_ref_not_degraded.ys; tc1/tp7b).  The `*_tb.sv` files that instantiate a
dut from a sibling file are the documented "missing dut" edge in
run_battery.py: standalone they now fail `hierarchy -check` instead of
reaching "no summary".

## System functions inside concurrent assertions, and two-state `===`

Until 2026-10-17 every `$f(...)` call inside a property expression that
was not `$past`/`$rose`/`$fell`/`$stable`/`$changed`/`$sampled` became a
`$sva` call cell whose output wire nothing ever drove: `!$isunknown(x)`
(OpenTitan's `ASSERT_KNOWN`), `$countones(x) <= 1`, `$size(x) == 4` were
verdicts on a free 1-bit wire (DIS @0, no circuit in the cone; the
mutation leg found 202 such OpenTitan disproofs).  Now the frontend
compiles them: `$isunknown` is constant 0 (two-state semantics, with a
warning per call; this applies to every `-formal` consumer, `sim`
included), `$countones`/`$onehot`/`$onehot0` are a popcount,
`$bits`/`$size`/`$clog2` and `$high`/`$low`/`$left`/`$right` of a declared
vector fold, anything else is an error ("would become an undriven
placeholder").  `tsysf.sv` pins all of them with independent
disproofs.  A check whose compiled body is a constant (ASSERT_KNOWN, a
tautology the frontend folded) carries `sva_lowering = exact:const`: it
proves nothing about the circuit, and the mutation leg classes it FREE.

Separately, `sva2check` folds `$eqx`/`$nex` against a constant with an x or
z bit to 0/1 (`fold_x_compares`, one warning per module): the formal
engines read an x constant as 0, so `a === 4'bxxxx` had meant `a == 0`.
This changes the meaning of user `===`/`!==` against x constants in every
flow that runs `sva2check`; four-state comparisons belong to `sim -xprop`,
which does not.

## Verdict hygiene (register v3 B5)

- `ic3` refuses a design whose modules carry `sv_degraded` /
  `sv_permissive` (the frontend's `-permissive` marks) and prints the
  list first either way; `-allow-degraded` runs anyway.  The drivers pass
  it only for a file that documents its leniency (`// READ-FLAGS:
  -permissive`); the smtbmc leg guards with `select -assert-mod-count 0
  A:sv_degraded A:sv_permissive %u`.  The ladder and the mutation leg
  measure degraded files knowingly (`-allow-degraded`; the ladder's CLEAN
  columns are the headline).  Pins: tests/verilog/ic3_refuse_degraded.ys,
  ic3_allow_degraded.ys.
- A cover property is reported per cover as `not-checked` (ic3 line
  `cover not checked [lowering=placeholder:flavor cover]`), never folded
  into a proved count; the battery expectation is `// NOT-CHECKED`
  (`tb5_cover.sv`).  D4 turns this into reachability.
- A proof whose compiled body is a constant prints as `PROVED(const)` in
  run_battery (tag `exact:const`): true, but not a statement about the
  circuit.
- `sim` (B1/B2): a design that still carries unlowered formal cells
  (`$sva`, `$past`, ...) is refused with the list printed first
  (`-allow-unlowered` runs anyway; they read x); an x-valued check is
  reported as "is x-valued (not a counterexample ...)" instead of
  "failed"; the run ends with "N check(s) never evaluated to a known
  value" when any check never saw 0/1 while enabled.  Pins:
  tests/verilog/sim_refuse_unlowered.ys, sim_xvalued.ys.

## Circuit mutation leg (register v3 C3, `conformance/run_mutation.py`)

"Prove and disprove every assertion by changing the behaviour of the
circuit, and see whether it is caught."  For every check the leg takes
the full ic3 verdict as the baseline, then applies single mutations
(yosys `mutate`: invert / stick-at / conditional-invert one output bit of
one cell) to the CIRCUIT cells in the check's fan-in cone and re-runs a
bounded engine.  Circuit = the input cone of the design's registers,
latches, memories and output ports minus the cells sva2check created;
the assertion's own Boolean layer (the `==`, `!`, `||` of the property
expression, feeding nothing but the check) is glue, not circuit, because
mutating it changes the property rather than the design and would
"catch" a tautology.

Classes: a PROVED check is CAUGHT when a mutant in its cone disproves it,
SURVIVED when at least three mutants ran in its cone, none disproved it,
and at least one was OBSERVABLE at the bits the check samples (a
tautology, a vacuous proof, or a lowering hole: the list to triage),
NOCHANGE@K when every surviving mutant was invisible to it (the
equivalence filter: gold/gate copies of the frozen lowered design, the
original ports dropped, the sampled circuit bits exposed per bit, `miter
-equiv` + bounded BMC; a mutant that changes none of them within K cannot
be caught by that check, whatever it does elsewhere), UNDER-MUTATED below
three, FREE when no circuit cell feeds it (inputs only, or
constant-folded).  The lowered design is frozen as RTLIL once per file
after the prep's flatten + opt_clean; every mutant, cone and miter run
reads that file, so names are stable by construction.  A DIS check is HEALED@K when a mutant makes
its counterexample vanish within K steps, MOVED when only the depth
changes, STUCK@K otherwise, FREE-DIS when nothing in the circuit feeds
it.  A mutant that heals every disproved check of a file that carries
assumptions is an assumption kill (unsatisfiable environment), labelled
and not counted.  `xmut.sv` is the positive control: one property per
class, checked on every `--battery` run (`// MUT: <class>` comments;
`p_big` pins the equivalence filter: a tautology sampling two bits of an
8-bit counter is NOCHANGE for every low-bit mutant).

A kill shows the verdict is sensitive to the design logic through the
lowering; it does not show the lowering is LRM-correct (the oracle legs'
job).  This leg finds fakes, not wrong answers.  `make mutate` runs it
over the battery; the corpus runs are in `conformance/runs/mutation_*`.

## Imported constant functions

A wildcard-imported package function called by its bare name in a
localparam evaluates, at module scope and inside another package (the
package's own `import` is applied in the package pass; the evaluator
accepts the result variable spelled by the function's own name).
`tpkgfn.sv` pins it against the qualified form and a DIS control.
`SVA_DEBUG_FCALL=1` makes the constant-function evaluator report why an
evaluation failed instead of degrading silently.
A wildcard-imported package function inlined in a continuous assign keeps
its body (the inlined block's result references are renamed with the
declaration); `tpkginl.sv` pins a concrete value with a one-bit-off DIS
control (this was the stress `func` XFAIL).
A cast binds tighter than a unary operator (`~W'(p)` is `~(W'(p))`,
IEEE 1800-2017 11.3.2): the grammar used to fold `~W` into the size
(tests/verilog/cast_unary_precedence.ys); this was the "Static cast with
zero or negative size" class in package functions.
A plain operator over a sequence/property operand (`c && (a |=> b)`,
`!(a |=> b)`, `c ? (a |=> b) : 1`) is refused ("Boolean operator over a
sequence/property expression"); it used to lower the property to a 1-bit
placeholder and produce a verdict (tests/verilog/sva_bool_over_property.ys).
Boolean-valued SVA leaves (`inside`, `weq`/`wne`, match items) are still
operands of plain operators.

## Type parameters and the `TOP:` pragma

`parameter type T = <default>` is a typedef that is also a parameter (the
parser marks it `(* type_param *)`, the module advertises it to
hierarchy); an instance's `.T(<typedef name>)` -- module-local, file-scope,
package-qualified or an enum -- is spliced into the derived module by
`derive`, so ports declared `T` take the override's width.  `tdtype.sv`
pins it, including a module-local typedef (passed as `module::name`), an
anonymous type at the instance (`.T(logic [7:0])`, declared as a private
typedef of the enclosing module by the parser), and a struct override
whose member type and range bound are the parent's own typedef and
localparam (they travel with the override, transitively).  A file whose design sits beside a package has no unique
root for `hierarchy -check`: `// TOP: <module>` names it, and
run_ic3 / run_bmc / prove honour the pragma when no `--top` is given.

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
