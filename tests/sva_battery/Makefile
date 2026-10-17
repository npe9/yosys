# SVA battery gate (gap register item 12, upstream-PR entry ticket).
# Runs both engines over the green t*.sv leg and checks per-property
# expectations.  x*-prefixed stress files need manual verdict reading
# (see README.md) and are not gated here.
# The sva_fuzz leg is a seeded (deterministic) differential spot-check of
# the SVA lowering; outputs land in a fresh /tmp dir, never in the tree.

YOSYS ?= $(abspath ../../build/yosys)
export YOSYS

FUZZ_SEED ?= 2
FUZZ_COUNT ?= 150
FUZZ_JOBS ?= 8
XPROP_SEED ?= 3
XPROP_COUNT ?= 40
# Conductor leg: prove.py runs the SMT and IC3 lanes in parallel over a
# representative mix (depth-0 pin, anchored ranges, $fair skip, s_until
# liveness legs, nested-temporal proofs) and checks the merged
# first-to-finish verdicts against the same expectations.  --cex keeps
# per-property .yw witnesses from both lanes and re-verifies every safety
# disproof by sim -r replay (assertion must fire at the recorded step);
# the --smt-only pass pins the smt-witness replay path deterministically
# (in the race, ic3 wins tprove0's disproofs on every quiet machine).
CONDUCTOR_FILES ?= tprove0.sv ts1.sv tanc1.sv tanc2.sv tfair2.sv tsuntil3.sv tresp1.sv

test:
	python3 run_battery.py
	python3 run_cdc.py
	python3 ../sva_fuzz/fuzz.py --seed $(FUZZ_SEED) --count $(FUZZ_COUNT) \
		--jobs $(FUZZ_JOBS) --out $$(mktemp -d /tmp/sva_fuzz_gate.XXXXXX)
	python3 prove.py --timeout 300 \
		--cex $$(mktemp -d /tmp/prove_cex.XXXXXX) $(CONDUCTOR_FILES)
	python3 prove.py --timeout 300 \
		--cex $$(mktemp -d /tmp/prove_cex_smt.XXXXXX) --smt-only tprove0.sv
	python3 ../sva_fuzz/xprop.py --golden \
		--out $$(mktemp -d /tmp/xprop_gold.XXXXXX)
	python3 ../sva_fuzz/xprop.py --seed $(XPROP_SEED) --count $(XPROP_COUNT) \
		--modes merge,pessimistic --techmap \
		--out $$(mktemp -d /tmp/xprop_gate.XXXXXX)
	python3 run_lsp.py

# Circuit-mutation leg (register v3 C3): baseline ic3 verdict per check,
# then single mutations of the circuit cells in each check's cone; a PROVED
# check no mutant disproves is SURVIVED (the triage list).  xmut.sv's
# per-class positive controls are checked on every run.  Separate target:
# minutes, not seconds.  The driver lives in the sv-yosys workspace
# (conformance/), four levels up from this directory in that checkout.
mutate:
	python3 ../../../../conformance/run_mutation.py --battery --jobs 4

.PHONY: test mutate
