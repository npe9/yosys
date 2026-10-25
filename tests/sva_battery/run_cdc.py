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
  cdc_gray_bus   | 4x WARNING  comb-before-sync    (clk_a -> clk_b, 4 bits: the Gray
                   |                                value is combinational, not a launch flop)
  cdc_reset      | 2x WARNING  reset-cdc           (external port + foreign domain)
  cdc_async_fifo | 8x WARNING  comb-before-sync    (two 4-bit gray pointers, each a
                   |                                combinational encoder output)
                   | 1x OK       synchronizer-2ff    (flag, clk_a -> clk_b)
  cdc_eq_bin     | 4x WARNING  synced-bus          (4-bit binary pointer,
                   |                                EQ compare is not Gray)
  cdc_clk_div    | 1x OK       generated-clock     (divide-by-2 from clk_a)
                   | 1x OK       same-clock-family   (clk_a -> div_clk)
                   | 4x OK       synchronizer-2ff    (div_clk -> clk_b sync)
  cdc_gate_icg   | 1x OK       clock-gate-icg      (latch-held enable)
  cdc_gate_reg   | 1x WARNING  clock-gate-reg      (registered comb AND)
  cdc_gate_bad   | 1x ERROR    clock-gating-glitch (async enable)
  cdc_reset_sync | 2x OK       reset-sync-stage    (the 2 synchronizer FFs)
                   | 1x OK       reset-sync-deassert (counter reset by tail Q)
  cdc_constrained| 1x WARNING  async-declared      (unsynchronized crossing,
                   |                                SDC-declared async clocks)
                   | 1x OK       synchronizer-2ff    (unaffected by pairing)
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

# (module, severity, class) -> expected count.  Findings group by
# (dst domain, src domain set); a multi-bit group of synced crossings
# then splits by reconvergence: bits whose forward cones merge (shared
# comb cell or sink FF, e.g. the FIFO pointers' EQ compares or the Gray
# decoder XOR chain) form a bus, while a bit whose cone is disjoint from
# its group siblings is promoted to OK synchronizer-2ff (the async FIFO's
# flush flag).  A merged bus upgrades to OK gray-bus when it matches the
# canonical Gray structure -- bin->gray $xor encoder on the source side
# and, on the destination side, either a prefix-XOR decoder chain over
# all synced bits or an $eq compare against a local Gray-coded vector
# (FIFO full/empty style); cdc_eq_bin's local side is binary, so it stays
# synced-bus WARNING.
EXPECTED = {
    ("cdc_sync_2ff", "OK", "synchronizer-2ff"): 1,
    ("cdc_bare", "ERROR", "unsynchronized-bit"): 1,
    ("cdc_gray_bus", "WARNING", "comb-before-sync"): 4,
    ("cdc_reset", "WARNING", "reset-cdc"): 2,
    ("cdc_async_fifo", "WARNING", "comb-before-sync"): 8,
    ("cdc_async_fifo", "OK", "synchronizer-2ff"): 1,
    ("cdc_eq_bin", "WARNING", "synced-bus"): 4,
    # clock network analysis (generated clocks, gating, families)
    ("cdc_clk_div", "OK", "generated-clock"): 1,
    ("cdc_clk_div", "OK", "same-clock-family"): 1,
    ("cdc_clk_div", "OK", "synchronizer-2ff"): 4,
    ("cdc_gate_icg", "OK", "clock-gate-icg"): 1,
    ("cdc_gate_reg", "WARNING", "clock-gate-reg"): 1,
    ("cdc_gate_bad", "ERROR", "clock-gating-glitch"): 1,
    # reset deassert sequencing
    ("cdc_reset_sync", "OK", "reset-sync-stage"): 2,
    ("cdc_reset_sync", "OK", "reset-sync-deassert"): 1,
    # SDC pairing (cdc_cases.sdc declares cclk_a/cclk_b async)
    ("cdc_constrained", "WARNING", "async-declared"): 1,
    ("cdc_constrained", "OK", "synchronizer-2ff"): 1,
}

# read_sdc -dump lines the parser must produce for cdc_cases.sdc
# (parser coverage: clocks, groups, false/multicycle paths, io delays,
# max delay, generated clock)
DUMP_EXPECT = [
    "clock cclk_a period 10.000",
    "clock cclk_b period 10.000",
    "clock cclk_a_fast period 4.000",
    "clock cclk_div period 0.000 generated",
    "clock-groups async: cclk_a vs cclk_b",
    "false_path cclk_a_fast -> cclk_b",
    "multicycle_path 3 cclk_a -> cclk_b -setup",
    "io-delay input port:din_c clock cclk_a max 2.000",
    "io-delay output port:dout_c clock cclk_b max 1.500",
    "max-delay 12.000 cclk_a -> cclk_b",
]


def main():
    jf = os.path.join(tempfile.gettempdir(), "cdc_gate_%d.json" % os.getpid())
    sdc = os.path.join(os.path.dirname(CASES), "cdc_cases.sdc")
    script = ("read_verilog -sv %s\nhierarchy -check\nproc\nopt_clean\n"
              "read_sdc %s\ncdc -json %s\n" % (CASES, sdc, jf))
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

    # parser coverage: read_sdc -dump on the same file
    dump_script = ("read_verilog -sv %s\nhierarchy -check\n"
                   "read_sdc -dump %s\n" % (CASES, sdc))
    ys2 = os.path.join(tempfile.gettempdir(), "cdc_gate_dump_%d.ys" % os.getpid())
    with open(ys2, "w") as fh:
        fh.write(dump_script)
    d = subprocess.run([YOSYS, ys2], capture_output=True, text=True)
    if d.returncode != 0:
        print("cdc gate: read_sdc -dump failed")
        return 1
    dump_lines = [l.strip() for l in (d.stdout + d.stderr).splitlines()]
    for want in DUMP_EXPECT:
        if want not in dump_lines:
            bad.append("read_sdc -dump missing line: %r" % want)

    total = sum(got.values())
    if bad:
        print("cdc gate: MISMATCH over %d findings:" % total)
        for b in bad:
            print("  " + b)
        return 1
    print("cdc gate: ok (%d findings across %d modules, table matches, "
          "sdc dump ok)" % (total, len({k[0] for k in got})))
    return 0


if __name__ == "__main__":
    sys.exit(main())
