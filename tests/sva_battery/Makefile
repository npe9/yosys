# SVA battery gate (gap register item 12, upstream-PR entry ticket).
# Runs both engines over the green t*.sv leg and checks per-property
# expectations.  x*-prefixed stress files need manual verdict reading
# (see README.md) and are not gated here.

YOSYS ?= $(abspath ../../build/yosys)
export YOSYS

test:
	python3 run_battery.py

.PHONY: test
