#!/usr/bin/env python3
"""In-tree SVA battery gate: runs both engines over the green t*.sv leg
and checks per-property expectations (// PROVED / // DIS @N comments).

N = ctr value = BMC step = ic3 frame - 1.  Exit 0 iff the green leg is
clean.  Known edges (t30a prep-fail, t40*/tfix_* sim testbenches with no
properties) are asserted, not skipped silently.  x-prefixed stress files
(xlive*, xfifo_dist, xmcloc1) need per-file timeouts and manual verdict
reading; they are listed but not gated here.
"""

import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
YOSYS = os.environ.get("YOSYS", os.path.join(ROOT, "build", "yosys"))

EXPECT = re.compile(r"//\s*(PROVED|DIS)(?:\s*@(\d+))?")


def expectations(path):
    """property name -> ('proved', None) | ('dis', n-or-None), in file order."""
    out = {}
    for line in open(path, errors="ignore"):
        m = EXPECT.search(line)
        if m and ":" in line:
            name = line.split(":", 1)[0].strip()
            if name and re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", name):
                out[name] = (m.group(1).lower(), int(m.group(2)) if m.group(2) else None)
    return out


def run_ic3(path):
    proc = subprocess.run(
        [sys.executable, os.path.join(HERE, "run_ic3.py"), path, "--timeout", "90", "--yosys", YOSYS],
        capture_output=True, text=True)
    blob = proc.stdout + "\n" + proc.stderr
    if "prep-failed" in blob or "no summary" in blob or "timed out" in blob:
        return None, [l for l in blob.splitlines()
                      if "prep-failed" in l or "no summary" in l or "timed out" in l]
    results = {}
    for line in blob.splitlines():
        name, status, depth = None, None, None
        m = re.search(r"ic3: (\S+): (proved|disproved|undecided)(?: \(depth (\d+)\))?", line)
        if m:
            name, status, depth = m.group(1), m.group(2), m.group(3)
        else:
            m = re.match(r"\s*(proved|disproved|undecided)\s+(\S+)(?: \(frame (\d+)\))?", line)
            if m:
                status, name, depth = m.group(1), m.group(2), m.group(3)
                if status == "disproved" and depth:
                    depth = int(depth) - 1  # frame N+1 -> depth N
        if name is None:
            continue
        # cell names arrive escaped (\bad_ok) and sva2check prefixes
        # declaration-body checks with bad_; normalize to the label
        name = name.lstrip("\\")
        if name.startswith("bad_"):
            name = name[4:]
        if name in results:
            continue
        results[name] = (status, int(depth) if depth else None)
    return results, []


def run_bmc(path):
    proc = subprocess.run(
        [sys.executable, os.path.join(HERE, "run_bmc.py"), path, "--yosys", YOSYS],
        capture_output=True, text=True)
    return proc.stdout + "\n" + proc.stderr


def check_ic3(path, exp):
    results, infra = run_ic3(path)
    bad = []
    rel = os.path.basename(path)
    if infra:
        if rel in KNOWN_EDGES and any(KNOWN_EDGES[rel](l) for l in infra):
            return []
        return [f"ic3 leg infrastructure: {i}" for i in infra]
    if results is None:
        return []
    for name, (kind, n) in exp.items():
        got = results.get(name)
        if got is None:
            bad.append(f"{name}: ic3 produced no verdict")
            continue
        status, depth = got
        if kind == "proved" and status != "proved":
            bad.append(f"{name}: expected PROVED, ic3 says {status} (depth {depth})")
        elif kind == "dis":
            if status != "disproved":
                bad.append(f"{name}: expected DIS, ic3 says {status}")
            elif n is not None and depth is not None and depth != n:
                bad.append(f"{name}: expected DIS @{n}, ic3 depth {depth}")
    return bad


def check_bmc(path, exp):
    blob = run_bmc(path)
    bad = []
    for line in blob.splitlines():
        if "MISMATCH" in line or "MISSING" in line:
            bad.append("bmc leg: " + line.strip())
        elif "prep-failed" in line:
            # documented edges never reach the formal legs (t30a: parser
            # rejection by design; t40*/tfix_*: sim testbenches whose DUT
            # port directions the formal prep rejects)
            if os.path.basename(path) not in KNOWN_EDGES:
                bad.append("bmc leg: " + line.strip())
    return bad


# Documented edges: file -> predicate matching the expected infrastructure
# line from the ic3 leg (they never reach property verdicts).
KNOWN_EDGES = {
    "t30a.sv": lambda l: "prep-failed" in l,
    "t40_case1.sv": lambda l: "no summary" in l,
    "t40_tb.sv": lambda l: "no summary" in l,
    "t41_case1.sv": lambda l: "no summary" in l,
    "t41_tb.sv": lambda l: "no summary" in l,
    "t42_case1.sv": lambda l: "no summary" in l,
    "t42_tb.sv": lambda l: "no summary" in l,
    "t43_tb.sv": lambda l: "no summary" in l,
    "t43.sv": lambda l: "no summary" in l,
    "tfix_a.sv": lambda l: "no summary" in l,
    "tfix_b_tb.sv": lambda l: "no summary" in l,
    "tfix_b.sv": lambda l: "no summary" in l,
    "tfix_c.sv": lambda l: "no summary" in l,
}


def main():
    files = sorted(glob.glob(os.path.join(HERE, "t*.sv")))
    n_props = 0
    failures = []
    for path in files:
        rel = os.path.basename(path)
        exp = expectations(path)
        n_props += len(exp)
        for leg in (check_ic3, check_bmc):
            for msg in leg(path, exp):
                failures.append(f"{rel}: {msg}")
        print(f"{rel}: {'ok' if not failures else 'ISSUES'} ({len(exp)} properties)", flush=True)
    stress = sorted(os.path.basename(p) for p in
                    glob.glob(os.path.join(HERE, "x*.sv")))
    print(f"\ngreen leg: {len(files)} files, {n_props} properties")
    print("stress files (manual verdict reading, not gated):", ", ".join(stress))
    if failures:
        print("\nFAILURES:")
        for f in failures:
            print(" ", f)
        sys.exit(1)
    print("\nBATTERY GREEN")
    sys.exit(0)


if __name__ == "__main__":
    main()
