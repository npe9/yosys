# sva_fuzz — SVA lowering-fuzz differential oracle

Randomized grammar-based fuzzing of the SystemVerilog SVA frontend
(`frontends/ast` + `passes/sva/sva2check.cc`) with a differential
sim-vs-formal oracle. A fixed test battery can only match false-proof bugs
by construction (both legs come from the same lowering); this harness
generates random LRM-legal properties and checks that bounded model
checking and witness-replay simulation *agree* on every one of them.

## Oracle semantics

Each generated case is a `module top` with only top-level inputs (so the
stimulus fully controls the design), an optional free-running internal
counter, and exactly one `assert property`:

```systemverilog
module top(input clk, input a, input b, input c [, input d]);
  logic [1:0] cnt;
  always_ff @(posedge clk) cnt <= cnt + 1;
  p0: assert property (@(posedge clk) <generated property>);
endmodule
```

Registers are zero-initialized in both legs (the battery convention).

Per case, bound `B` (default 12):

1. **Sim leg** — `read_verilog -sv -formal case.sv; sva2check;
   hierarchy -check -top top; proc; flatten;` then
   `sim -r stim.yw -zinit -summary sum.json`.
   `stim.yw` is a Yosys witness with a `posedge clk` clock entry, one
   signal entry per data input, and `B` random per-step bit vectors
   (seeded PRNG; within a step's `bits` string signal *k*'s LSB sits at
   the highest string index, per `kernel/yw.cc` `get_bits`), covering
   exactly the steps `yosys-smtbmc -t B` checks (0..B-1) — a B+1st step
   would let sim observe the assertion past the formal bound and
   manufacture false class-A findings. The sim re-derives register
   states from the inputs, so prep matches the formal leg. The summary's
   `assertions` array records every fired assertion with its step (step
   *k* = k-th posedge).
2. **Formal leg** — mirrors `tests/sva_battery/run_bmc.py` exactly:
   `read_verilog -sv -formal; hierarchy -check -top top; sva2check; proc;
   async2sync; flatten; opt_clean; memory; opt_clean; setundef -init -zero;
   write_smt2 -wires out.smt2`, then
   `yosys-smtbmc -s z3 -t B --dump-yw cex.yw -m top out.smt2`.
   Exit 0 / `Status: PASSED` = no cex within the bound; `Assert failed in
   top: p0` after `Checking assertions in step j` = cex at step *j*.
3. **Cross-check**
   - BMC UNSAT + sim fired at step k ≤ B → **class A** (false proof — the
     monitor is provable yet the simulation finds a real failure; the most
     severe class: lowering / `write_smt2` / prep-mismatch bug).
   - BMC SAT at step j → replay `cex.yw` via
     `sim -r cex.yw -zinit -summary sum2.json` (no `-clock`: the yw clocks
     header drives the clock). `p0` must fire at the *same* step j.
     Otherwise → **class B** (cex irreproducible in sim: sim bug, yw
     translation bug, or prep mismatch).
   - BMC UNSAT + sim quiet, or BMC SAT + replay firing at j → agree.
4. **Determinism guard** — the sim leg runs twice on identical stimulus;
   the two summaries must match, otherwise the finding is flagged
   `N` (sim nondeterminism would pollute the oracle).

Step conventions line up: smtbmc checks the assertion at time *j* with
time-*j* inputs; the yw replay applies step *j*'s inputs and clocks the
posedge that produces summary step *j* (verified end-to-end by hand).

## Bug-class taxonomy

| class | meaning | severity |
|---|---|---|
| A | BMC UNSAT but sim fired ≤ B | false proof; most severe |
| B | BMC SAT at j, replay does not fire p0 at j | cex irreproducible |
| C | yosys ERROR / sva2check "unsupported $check" on generated legal syntax | frontend/lowering gap; deduped by normalized error signature, first repro saved |
| D | nonzero exit without an ERROR line (segfault/abort) | crash |
| E | infrastructure failure (timeout, no `Solver:` banner, missing outputs) | not a bug; counted separately |
| N | determinism guard mismatch | sim nondeterminism |

Anything that is not `agree-quiet`, `agree-cex` or `E` lands in the
failure corpus. Known-but-unlowered LRM constructs (e.g. sequence-level
`and`, which `sva2check` rejects with `unsupported sequence operator
'and'`) are generated at low rate on purpose so the corpus registers the
gap; they dedupe to one signature.

## Generator (gen.py)

`python3 gen.py --seed N --count M --out DIR [--bound B]` writes
`case_0000.sv` + `case_0000.yw` + a `manifest.json`.
Deterministic per `(seed, index)`; `fuzz.py` imports it directly.

- DUT varies per case: 3-4 data inputs, optional `cnt` (`logic [1:0]` or
  `[2:0]`, free-running or gated by `a`).
- Property alphabet (all probe-verified against this tree; fully
  parenthesized, conservative `property_expr` shapes only):
  - booleans: inputs, `cnt` bits, `1'b0/1'b1`, optionally wrapped in
    `$rose/$fell/$stable/$past(sig,1..2)` and `$stable/$changed` of
    small parenthesized exprs; `!`, `&&`, `||`, `->`, `<->`
  - sequences: `##N` (1..3), atom `[*N]`/`[*N:M]`/`[=N]` (1..4),
    `or`/`and`/`intersect`/`within`/`throughout`/`first_match`
    (repetitions apply to plain atoms only — `sva2check` rejects
    repetition of general sequences)
  - properties: `|->`, `|=>`, `not`, rare `s_always[N:M]` / `strong` /
    `weak` (bounded == safety); `if (b) p else q` at top level and as
    the direct `|->`/`|=>` consequent; `case (sel) ... default` with a
    1-2-bit selector; `nexttime`/`s_nexttime [1..2]` over booleans,
    implications, and (rare) pinned `ATOM ##N ATOM` sequences;
    all four abort operators `accept_on`/`reject_on`/`sync_accept_on`/
    `sync_reject_on`; implication nesting only parenthesized in a
    consequent
  - optional `disable iff (rst)` on the spec (a free `rst` input)
- Fully bounded (no `[* ]`/`[= ]`/`[->]` unbounded reps, no `##[N:$]`, no
  `until`/`s_eventually` liveness) so the BMC leg always decides.
- Depth ≤ 5, node budget 26.

A frontend reject is only a class-C result if the syntax is genuinely
legal — every emitted shape was checked against the parser first, and the
property is always human-recheckable from the saved `.sv`.

## Usage

```sh
# generate-only (inspection)
python3 gen.py --seed 1 --count 10 --out /tmp/cases

# full differential batch (300 cases ≈ a few minutes at --jobs 8)
python3 fuzz.py --seed 1 --count 300 --bound 12 --jobs 8

# report -> tests/sva_fuzz/report_seed1.md, failures -> tests/sva_fuzz/failures/
```

Environment variables `YOSYS` / `SMTBMC` override the binary paths
(defaults: `../../build/yosys` relative to this directory, i.e. the
in-tree cmake build).

## Failure corpus layout

```
tests/sva_fuzz/failures/
  A-1-0042/            # class A, master seed 1, case index 42
    case.sv            # the generated module (property under test)
    stim.yw            # random stimulus the sim leg replayed
    sum.json           # sim summary (fired assertions + steps)
    sim.ys formal.ys replay.ys   # yosys scripts for the three legs
    sim_leg.log formal_leg.log   # yosys output of each leg
    out.smt2 smtbmc.log cex.yw   # formal artifacts (cex.yw only when SAT)
    sum2.json replay.log         # cex-replay artifacts
    NOTES.txt          # class, description, exact re-run commands
```

Class C is saved once per distinct error signature (first occurrence);
later hits of the same signature are counted in the report only.

## Re-running a saved failure

```sh
cd tests/sva_fuzz/failures/A-1-0042
yosys sim.ys                      # sim leg -> sum.json
yosys formal.ys                   # formal prep -> out.smt2
yosys-smtbmc -s z3 -t 12 --dump-yw cex.yw -m top out.smt2
yosys replay.ys                   # cex replay -> sum2.json
```

The property under test is the single `p0: assert property` line in
`case.sv`; delta-debug it by shrinking that line while the mismatch
persists, and keep the module inputs/zero-init intact.

## Known gaps registered by the seed-1 batch (bound 12)

Real bugs found in the first 300-case run (all class C/D, no class A/B
false proofs or cex-irreproducibility in this batch):

- **sva2check crash**: any sampled-value function (`$rose/$fell/$stable/
  $past(sig,1)`) as the LHS boolean of `throughout` aborts with an
  uncaught `std::out_of_range` (SIGABRT, sometimes SIGSEGV under
  `always`). Five independent hits (indexes 30/143/161/229/231), one
  root cause; minimized to `always ($fell(b) throughout a)` in each
  `failures/D-1-*/MIN.sv` with a mechanism analysis in `ANALYSIS.txt`.
- **constant property**: `assert property (@(posedge clk) 1'b1)` is
  skipped as a "malformed spec", leaving the x-reading placeholder:
  the constant-TRUE assertion fires in sim and "fails" in BMC.
  `failures/C-1-176` (+`ANALYSIS.txt`).
- **sequence `and`**: LRM-legal but rejected with "unsupported sequence
  operator 'and'" (`failures/C-1-35`); emitted deliberately at low rate.

Deterministic rejects the generator already avoids (probe-discovered,
documented in gen.py comments): `[=N]` outside direct implication
antecedents; single-bound `[*N]` anywhere below a property-position
sequence; `or/and/intersect` anywhere below a within/throughout RHS;
`not` of `->`/`<->`/`or`/repetitions; repetition of a non-atom sequence;
`strong`/`weak`/`if` in operand position of `always`/`not`.

## xprop.py — 4-state X-propagation differential fuzzer

A separate harness (same directory, no shared code with the SVA fuzzer
above). Each case is a random small combinational/registered expression
tree over uninitialized registers (the X source) and driven inputs;
a Yosys witness drives the inputs and `sim -r ... -vcd` produces the
output trace. A Python 4-state reference implements the kernel's own
`const_*` semantics (`kernel/calc.cc`), including the elaboration width
rules (self-determined widths, context extension) verified against
`genrtlil`'s emitted `$cell` widths — the reference re-derives what the
frontend emitted, so a mismatch means sim diverges from the documented
kernel semantics.

Flags beyond `--seed/--count/--out`:

- `--modes merge,pessimistic` — differential-check `sim` in each
  x-propagation mode. `merge` (default) is the LRM conditional-operator
  table (x select merges equal arms per bit). `pessimistic` enables the
  `sim -xprop pessimistic` knob: any x in a `$mux`/`$_MUX_` select (or a
  `$bwmux` select bit) poisons the result, even where the arms agree
  (`$pmux` is already all-x on an x select via `is_onehot`). The poison
  lands at the ternary cell's WIDTH — and the cell width is `max` of the
  arm *produced* widths under the assignment context, not the context
  itself (genrtlil.cc AST_TERNARY), which the reference models via
  `arm_w`: plain-leaf/const arms emit at self width and only the connect
  extension stays definite; context-propagating arms (and/or/xor/xnor/
  add/sub/not/neg, shifts) carry the context as their emission width; the
  1-bit-result ops (`$eq/$lt/...`, `$reduce_*`, `$logic_not`) emit Y at
  `max(hint,1)` (probe-verified Y_WIDTH = context); concat arms extend
  to the full hint (genrtlil.cc AST_CONCAT). Two more pipeline
  realities: identical-arm muxes never reach sim (genrtlil emits the
  cell but proc's bundled `opt_expr -keepdc` folds `A == B` muxes,
  x-safe under merge but pessimism-unsafe), and the pessimism flag must
  thread through *every* eval_node branch (muxes nest inside comparison
  operands and reduction arguments).
- `--techmap` — per case also runs `proc; techmap` (simlib gate
  netlist: `$_AND_`/`$_MUX_`/`$_DFF_P_`/ripple adders/...) and checks
  the RTL->gate X-containment invariant: wherever the coarse-cell sim is
  definite, the gate netlist must agree definite; where the RTL is x the
  gate may be x *or definite* (techmap is structurally less pessimistic:
  per-bit gates beat `big2const`, carry chains contaminate upward only,
  barrel-shifter mux trees merge over a bounded position set). A
  definite-vs-definite mismatch or gate-x-over-RTL-definite is a
  soundness finding; the proc-x -> gate-definite direction is counted as
  expected drift, not a failure.
- `--golden` — deterministic pin of the mode divergence (no fuzz
  cases): `sel ? a : b` with the select x and both arms 1 must give
  `o=1` in merge and `o=x` in pessimistic, plus the gate-containment
  check on the same design. This is the regression the sva_battery gate
  runs.

`make test` in `tests/sva_battery` runs `--golden` and a seeded
`--modes merge,pessimistic --techmap` batch.
