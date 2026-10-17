# Upstreaming plan — sva-frontend-ic3 fork

This document is the packaging plan for landing the fork's work on
YosysHQ/yosys as a series of reviewable PRs.  It is maintainer-facing;
the day-to-day development log is in the workspace PLAN.md.

**Policy:** branches under `upstream/` on `fork`
(`git@github.com:npe9/yosys.git`) are packaging artifacts ready for a
human to open as PRs against YosysHQ/yosys `main`.  Nothing is ever
pushed to YosysHQ automatically; opening each PR is an explicit human
decision after reviewing the branch diff.

## How chunks are made

`tests/sva_battery/upstream_chunk.py <chunk> [--push]` materializes a
chunk: a scratch worktree branches from **current origin/main**, the
chunk's files are copied from the reviewed, gated fork HEAD (file
manifests, not cherry-picks — the fork's 52 commits tangle features),
anchored CMake insertions are applied, and the result must **build
against upstream main and pass the chunk's own gate** before it may be
pushed to `fork/upstream/<chunk>`.

## Series (in proposed PR order)

### 0. `typedef-dims` — typedef'd packed multi-dimensional types lose their lanes
- **Files:** one hunk in `frontends/ast/simplify.cc` (typedef-to-wire
  resolution keeps the template's `dimensions` metadata instead of
  regenerating it from the already-compacted flat range),
  `tests/svtypes/typedef_multidim_lanes.ys`.
- **Deps:** none.  **Reproduces on upstream main af9f71e51**: with
  `typedef logic [4:0][7:0] plane_t; plane_t s;` the lane select `s[2]`
  elaborates to bit 2 (`{7'h00, i[2]}`), silently miscompiling every
  design that indexes a typedef'd packed array (OpenTitan prim_keccak's
  `box_t`/`plane_t`: the whole round function collapsed to 49 cells).
- **Gate:** the new `.ys` (SAT-proved lane/bit/function-argument
  selects); `tests/svtypes` green.
- **Value:** smallest possible correctness PR in the series; open it
  first.  Found 2026-10-16 via the Keccak-f[1600] known-answer test.
- **Status:** fix landed on the fork (fedb3a9f0); chunk not yet materialized.

### 0b. Fork-vs-upstream parity (2026-10-16 session 1, second commit)
Running upstream's own suite (`make vanilla-test`) against the fork
found six fork regressions, all fixed and all covered by EXISTING
upstream tests (so the chunk-3 branch now passes them unchanged):
`tests/simple/{dynslice,partsel,sign_part_assign,memory,process,
realexpr,operators}.v`, `sat/counters-repeat.ys`,
`various/dynamic_part_select.ys`, `svtypes/struct_dynamic_range.ys`,
`verilog/unreachable_case_sign.ys`, `svinterfaces/{load_and_derive,
resolve_types}.ys`.  Fork-only root causes (nothing to upstream):
binop2rtlil operand signs, the force/disable passes' case-arm compare,
real->int truncation, `ref` keyword, lexer typedef lookahead vs
interface ports.  Remaining fork-only failures are the deliberate
error->warning relaxations listed in PLAN.md (policy decision pending:
errors by default + `read_verilog -permissive` for the sweeps) and one
documented semantic delta (`smt2_fair_cells.ys`: $fair is skipped with
a warning instead of erroring; the fork test expectation should change
with the branch offer).  Known residual delta: arithmetic/comparison
cells still use the fork's matched-sign rule (per-operand signs crash
the frontend on two corpus files; open item).
  (The companion parameter-array fixes from the same session --
  multi-dimensional `localparam int R [5][5]` folds and dynamic-index
  reads via `$shiftx` -- depend on the fork's unpacked parameter-array
  support, which upstream main does not parse, so they ride with the
  `sva-frontend` chunk.)

### 0c. `cast-fixes` — two static-cast bugs that reproduce on upstream main
- **Files:** `frontends/verilog/verilog_parser.y` (two `%precedence` lines
  swapped: OP_CAST above UNARY_OPS, IEEE 1800-2017 11.3.2 makes the cast a
  primary), `frontends/ast/simplify.cc` (one hunk: a cast to an unsized
  base type is 1 bit, keyed on the parser's no-range marker),
  `tests/verilog/cast_unary_precedence.ys`, `tests/verilog/cast_unsized_type.ys`.
- **Reproduces on stock main (yosys-main worktree):** `assign y = ~W'(p);`
  with `localparam int W = 4` -> "Static cast with zero or negative size"
  (the size parses as ~4); `assign y = logic'(p);` -> the same error.
  Both are SAT-pinned by the two tests.  Found 2026-10-17 via OpenTitan's
  package functions (register v3 D7b) and prim_max_tree.
- **Status:** landed on the fork (0bd82fcad, 203653d5d); chunk not yet
  materialized.  Small and independent: open right after `typedef-dims`.

### 1. `cdc-sdc` — structural CDC analysis + SDC subset frontend
- **Files:** `passes/cmds/cdc.cc`, `passes/cmds/read_sdc.cc`,
  `passes/cmds/sdc_constraints.h`, `tests/verilog/cdc_cases.{sv,sdc,ys}`,
  `tests/sva_battery/run_cdc.py`.
- **Deps:** none — every file is new; zero drift vs upstream main.
- **Gate:** `run_cdc.py` (35 findings / 12 modules table + read_sdc
  `-dump` coverage).  Status: **materialized, gated, pushed**
  (`fork/upstream/cdc-sdc`).
- **Value:** standalone structural CDC linter (synchronizers, Gray
  buses, clock-gating glitch classes, reset deassert sequencing,
  generated clocks) plus a dependency-free SDC constraint subset
  parser whose declarations pair with the findings.

### 2. `sim-xprop` — four-state X-propagation modes for `sim`
- **Files:** `passes/sat/sim.cc` (diff — main drifted ~533 lines, so
  this chunk needs a 3-way rebase, not a file copy),
  `tests/sva_fuzz/xprop.py`, golden expectations.
- **Deps:** none.
- **Gate:** xprop golden + seeded differential (merge/pessimistic x
  techmap containment).
- **Value:** LRM merge vs pessimistic mux-select X semantics and a
  differential fuzz oracle for the `sim` pass.
- **Status:** planned (needs the rebase work above).

### 3. `sva-frontend` — SystemVerilog assertions end to end (THE MONSTER)
- **Files:** `frontends/ast/*` (~5.7k lines of the initial squash),
  `frontends/verilog/*` (lexer/preproc), `passes/sva/sva2check.cc`,
  `passes/ic3/*`, `tests/sva_battery/*` (battery, conductor, LSP,
  minimizer), `tests/sva_fuzz/*`, cmake wiring.
- **Deps:** none technically, but this is the review bottleneck:
  realistically needs to be offered as a long-lived branch with the
  battery (`make test` / `test-sva`) as its entry ticket, and split
  further (frontend grammar vs lowering vs engines) only if a
  maintainer asks.
- **Honesty hooks the branch offer must quote (register v3, 2026-10-17):**
  every `$check` carries `(* sva_lowering = exact | approx:<kinds> |
  placeholder:<why> | immediate | plain *)` and every verdict printer
  shows it; `sva2check -exact` refuses approximations; the approximation
  table (kind, what it misses, battery files) lives in
  `tests/sva_battery/README.md`; the independent LRM model
  `tests/sva_pbt` lists the known single-monitor divergences (F2-F6,
  F10) that per-attempt tracking still has to fix.
- **Circuit-mutation evidence (register v3 C3, 2026-10-17):** the branch
  offer quotes `conformance/run_mutation.py`'s per-assertion table (a
  proof is CAUGHT when a mutation of the circuit in its cone disproves it,
  FREE when no circuit feeds it); the leg found that every system function
  inside a concurrent assertion had been an undriven `$sva` call cell
  (ASSERT_KNOWN, `$countones(x) <= 1`, `$size(x) == 4` were verdicts on a
  free wire), fixed in genrtlil; a constant-body check is tagged
  `exact:const` so no verdict printer calls it a proof about the circuit.
- **Read policy (register v3 B4, 2026-10-17):** the fork's frontend
  leniencies are errors by default and live behind `read_verilog
  -permissive` / `hierarchy -check -permissive`, which warn per
  occurrence and mark modules `(* sv_degraded *)` / `(* sv_permissive *)`
  (proc_dlatch keys its always_comb latch policy on the latter).  Of
  upstream's 20 "policy" tests the fork now passes 18 by default; the
  remaining two are expectation-only differences (bug2493: a cell name
  used as a wire is still an implicit net; positional_args: the fork
  connects interface instances positionally through implicit member
  wires that hierarchy resolves, upstream errors) plus the documented
  smt2_fair_cells skip.
- **Status:** planned; documented as the high-risk/high-value endgame.

### 4. `lsp` — editor diagnostics server
- **Files:** `tests/sva_battery/lsp.py`, `run_lsp.py`.
- **Deps:** none (works against vanilla yosys); value rides on
  whatever frontend the user's yosys has.
- **Status:** planned; may ship with chunk 3 or standalone.

## Rebase hygiene

Upstream main moves fast (~344 commits in the three weeks the fork
has existed).  Chunks are rematerialized from current origin/main
whenever a PR is about to be opened — the manifest content comes from
the fork HEAD, so re-running `upstream_chunk.py` refreshes the base.
The fork branch itself rebases onto main only when a chunk needs it;
the battery pragmas (`IC3-TIMEOUT`) absorb machine-load flakes during
verification.
