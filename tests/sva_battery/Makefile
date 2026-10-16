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

test:
	python3 run_battery.py
	python3 run_cdc.py
	python3 ../sva_fuzz/fuzz.py --seed $(FUZZ_SEED) --count $(FUZZ_COUNT) \
		--jobs $(FUZZ_JOBS) --out $$(mktemp -d /tmp/sva_fuzz_gate.XXXXXX)
	python3 ../sva_fuzz/xprop.py --golden \
		--out $$(mktemp -d /tmp/xprop_gold.XXXXXX)
	python3 ../sva_fuzz/xprop.py --seed $(XPROP_SEED) --count $(XPROP_COUNT) \
		--modes merge,pessimistic --techmap \
		--out $$(mktemp -d /tmp/xprop_gate.XXXXXX)

.PHONY: test
