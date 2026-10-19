# sva_pbt — property-based tester for the SVA framework

QuickCheck-style: a **model** (an independent LRM semantics), a
**generator** (random properties + random traces + syntax mutants), the
**system under test** (frontend + `sva2check` + `sim`, and + `write_smt2`
+ `yosys-smtbmc`), and a **shrinker** that minimizes any disagreement.

    python3 pbt.py --selftest                 # model vs 10 calibrated cases
    python3 pbt.py --seed 1 --count 60        # one batch (3 traces/case, bound 10)
    python3 triage.py failures/<dir>          # trace table: model vs sim fires
    make pbt                                  # seeds 1..3

The model is `ref_sva.py`: sequences as sets of match end steps over a
finite prefix (3-valued: definite / open), properties as Kleene logic,
an attempt per step, and the attempt *fires* at the first step where its
verdict is definitely false.  It was written from IEEE 1800-2017 clause
16 only; it has never seen sva2check.  It is calibrated against the
framework on ten hand-built cases (`--selftest`) whose step convention
was measured, not assumed:

| convention | measured |
|---|---|
| violation fires at | the step the verdict becomes definite |
| `$past(x)` before step 0 | 0 (`-zinit` / `setundef -init -zero`) |
| `disable iff (c)` with c true at the firing step | attempt passes |
| sim reports | every firing step (set), e.g. `cnt==2 \|-> a` → `[2, 6]` |
| witness steps | `bound` steps, index 0 = first sampled step |

Design shell for every case: inputs `a b c d rst`, `r <= a`, `cnt <= cnt+1`
(so `r == $past(a)` is a tautology and `cnt(k) = k mod 4`).

## Result classes

| class | meaning | severity |
|---|---|---|
| OK | model and both legs agree (quiet, or BMC cex confirmed by the model) | — |
| SIM-VERDICT | sim fires on a trace the model says passes, or vice versa | high |
| SIM-STEP | same verdict, different firing steps | medium |
| BMC-FALSE-PROOF | BMC UNSAT to bound, model fires on a sampled trace | highest |
| BMC-CEX-WRONG | BMC counterexample that the model says does not fail at that step | high |
| BMC-LATE | BMC's first cex later than a model failure on a sampled trace | high |
| UNSUPPORTED | frontend/sva2check rejects legal syntax | listed |
| ACCEPTS-INVALID | a syntax mutant that must be rejected was accepted | listed |

"Correct" and "incorrect" properties are both covered: the model decides
per trace whether the property holds, so each case exercises traces that
fire and traces that do not; cases where no sampled trace fires are the
correct population and the BMC leg must agree (UNSAT) or produce a cex
the model confirms.

## Triage protocol

A disagreement is a bug in exactly one of: the model, the harness, the
framework.  Shrink first (`pbt.py` does it), then print the table with
`triage.py` and decide from the LRM text.  Two harness/model bugs were
found this way on day one (empty match counted as a sequence-property
match; cex witness read past its length) and fixed; everything below
survived that scrutiny.

## Confirmed framework divergences (2026-10-16, fork 04debf417)

Hand-verified against the LRM on the shrunk tables; `tests/sva_pbt/
failures/` holds the repros after a run.

| # | shape | framework behaviour | LRM |
|---|---|---|---|
| F1 | `p until_with q`, `p until q` | FIXED (same day): the `_with` obligation was not enforced, a failure at the attempt's own start step never fired, and a q seen by an earlier attempt masked a new one | 16.12.10 |
| F2 | `b[=n] \|-> q` | the match ending exactly at the n-th `b` is not considered (needs a later non-b step) | 16.9.2: `b[=n]` = `b[->n] ##1 !b[*0:$]`, empty tail included |
| F3 | `(e) ##0 b[->n]` as property | fires at a step where a match exists | 16.9.2 |
| F4 | `c ##[0:1] r` as property | sim never fires on failing traces (BMC does find the cex) | 16.12.1 |
| F5 | `a[*1:2]` as property | sim fires one step late and never for the last step's attempt | 16.12.1 -- 2026-10-19 (register v3 D5): fixed for assume/restrict by default (the late form let traces slip past assumptions) and for asserts under `sva2check -early` / env `SVA_EARLY=1` (one monitor copy per attempt slot; ~100x on ic3 proofs, so not the assert default).  `SVA_EARLY=1` pbt, 3 seeds x 60: 7 cases fixed, 0 regressions |
| F6 | `s_eventually[m:n] <temporal>` | sim never fires when every alternative fails (BMC does) | 16.12.14 |
| F7 | `disable iff (c) (nexttime[1] p)` | FIXED (same day): the condition was only ORed into the final check; it now clears every monitor state register while true (in-flight attempts abandoned, nothing arms on a disabled step) | 16.15 |
| F8 | `always[1:1] p` at the last trace step | sim reports the pending obligation as a failure | weak at trace end (convention; low) |
| F9 | `<temporal> implies q` | fails as soon as q is false, before the left side is determined (step only) | 16.12.4 (low) |
| F10 | `p1 or p2`, `p1 implies p2` with operands that resolve at different steps | a failure is reported as soon as one operand fails, while the other is still undecided (e.g. a sequence disjunct that can still match later); shows up as BMC counterexamples the model rejects | 16.12.4: the attempt is undecided until both sides are |
| S1 | `a ##[3:1] b` | accepted | reversed range; most tools reject (low) |

F2-F6 share one root cause: sva2check compiles ONE monitor per
sequence and starts it every tick, so match ends of different attempts
are merged (`strong_seq_fail` ORs `end` over the deadline window with
`start = 1'b1`): a later attempt's match hides an earlier attempt's
failure (F4), the failure is only reported at the sequence's maximum
length (F5, one step late / never for the last steps of a trace), and
occurrence counters are shared (F2, F3).  The exact fix is per-attempt
match tracking (ends tagged by match length, or the slot/age ring the
local-variable machinery already has), register v3 item D5.  Until
then `make pbt` is informative, not a gate: `pbt.py` exits nonzero on
any high-severity class.
