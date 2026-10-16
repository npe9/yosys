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
