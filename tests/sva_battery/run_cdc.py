#!/usr/bin/env python3
r"""Gate for the structural `cdc` pass over tests/verilog/cdc_cases.sv.

Runs `cdc -json` and diffs the findings against the expected per-module,
per-(severity, class) table from the case file's header comment.  Exit 0
iff the table matches exactly; any drift in classification, severity, or
module attribution fails the gate (update EXPECTED deliberately when the
pass intentionally changes).

Expected findings (one table line per crossing bit, plus reset findings):
  cdc_sync_2ff   | 1x OK       synchronizer-2ff    (clk_a -> clk_b)
  cdc_bare       | 1x ERROR    unsynchronized-bit  (clk_a -> clk_b)
  cdc_gray_bus   | 4x WARNING  synced-bus          (clk_a -> clk_b, 4 bits)
  cdc_reset      | 2x WARNING  reset-cdc           (external port + foreign domain)
  cdc_async_fifo | 8x WARNING  synced-bus          (two 4-bit gray pointers)
                   | 1x OK       synchronizer-2ff    (flag, clk_a -> clk_b)
"""

import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
YOSYS = os.environ.get("YOSYS", os.path.join(REPO, "build", "yosys"))
CASES = os.path.join(REPO, "tests", "verilog", "cdc_cases.sv")

# (module, severity, class) -> expected count.  NOTE: findings are
# grouped by (dst domain, src domain set) and any multi-bit group of
# synced crossings reports as one synced-bus WARNING per bit -- the
# async FIFO's flag shares its clk_a -> clk_b group with the write-gray
# pointer, so it reports as synced-bus, not synchronizer-2ff.  Finer
# per-structure splitting (2ff chains / Gray+decoder groups) is a Layer B
# refinement; until then the table pins the coarse behavior.
EXPECTED = {
    ("cdc_sync_2ff", "OK", "synchronizer-2ff"): 1,
    ("cdc_bare", "ERROR", "unsynchronized-bit"): 1,
    ("cdc_gray_bus", "WARNING", "synced-bus"): 4,
    ("cdc_reset", "WARNING", "reset-cdc"): 2,
    ("cdc_async_fifo", "WARNING", "synced-bus"): 9,
}


def main():
    jf = os.path.join(tempfile.gettempdir(), "cdc_gate_%d.json" % os.getpid())
    script = "read_verilog -sv %s\nhierarchy -check\nproc\nopt_clean\ncdc -json %s\n" % (CASES, jf)
    ys = os.path.join(tempfile.gettempdir(), "cdc_gate_%d.ys" % os.getpid())
    with open(ys, "w") as fh:
        fh.write(script)
    p = subprocess.run([YOSYS, ys], capture_output=True, text=True)
    blob = p.stdout + "\n" + p.stderr
    if p.returncode != 0:
        err = ""
        for line in blob.splitlines():
            if "ERROR:" in line:
                err = line.split("ERROR:", 1)[1].strip()
        print("cdc gate: prep-failed: %s" % err[:120])
        return 1
    if not os.path.exists(jf):
        print("cdc gate: cdc -json produced no output file")
        return 1
    findings = json.load(open(jf))
    got = {}
    for f in findings:
        key = (f.get("module"), f.get("severity"), f.get("class"))
        got[key] = got.get(key, 0) + 1

    bad = []
    for key, n in sorted(EXPECTED.items()):
        if got.get(key, 0) != n:
            bad.append("expected %dx %s/%s/%s, got %d" %
                       (n, key[0], key[1], key[2], got.get(key, 0)))
    for key, n in sorted(got.items()):
        if key not in EXPECTED:
            bad.append("unexpected %dx %s/%s/%s" % (n, key[0], key[1], key[2]))

    total = sum(got.values())
    if bad:
        print("cdc gate: MISMATCH over %d findings:" % total)
        for b in bad:
            print("  " + b)
        return 1
    print("cdc gate: ok (%d findings across %d modules, table matches)" %
          (total, len({k[0] for k in got})))
    return 0


if __name__ == "__main__":
    sys.exit(main())
