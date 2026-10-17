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


def fold_live_legs(results):
    """s_until/s_until_with split into a safety leg `n` and a liveness
    twin `n$live`: the property is proved iff every leg is proved,
    disproved if any leg is (depth: the disproved safety leg's depth if
    any, else the liveness leg's)."""
    out = {}
    for name, verdict in results.items():
        if name.endswith("$live"):
            continue
        legs = [verdict]
        live = results.get(name + "$live")
        if live:
            legs.append(live)
        status = "proved" if all(s == "proved" for s, _ in legs) else \
                 "disproved" if any(s == "disproved" for s, _ in legs) else "undecided"
        depth = next((d for s, d in legs if s == "disproved" and d is not None), None)
        out[name] = (status, depth)
    return out


def ic3_timeout(path):
    """Per-file ic3 leg budget from `// IC3-TIMEOUT: N` (default 90s).
    The historically slow files (heavy proofs, multiclock prep) carry an
    explicit pragma so a loaded build machine doesn't manufacture
    infrastructure failures; the values assume a quiet machine."""
    try:
        with open(path) as fh:
            for line in fh:
                m = re.match(r'\s*//\s*IC3-TIMEOUT:\s*(\d+)', line)
                if m:
                    return int(m.group(1))
    except OSError:
        pass
    return 90


# path -> {property: sva_lowering tag} as the ic3 leg logged it (A3: every
# verdict line of the battery carries its tag; none may say "untagged")
LOWERING = {}


def run_ic3(path):
    proc = subprocess.run(
        [sys.executable, os.path.join(HERE, "run_ic3.py"), path,
         "--timeout", str(ic3_timeout(path)), "--yosys", YOSYS],
        capture_output=True, text=True)
    blob = proc.stdout + "\n" + proc.stderr
    if "prep-failed" in blob or "no summary" in blob or "timed out" in blob:
        return None, [l for l in blob.splitlines()
                      if "prep-failed" in l or "no summary" in l or "timed out" in l]
    results = {}
    for line in blob.splitlines():
        name, status, depth = None, None, None
        m = re.search(r"ic3: (\S+): (proved|disproved|undecided)(?: \(depth (\d+)\))?"
                      r"(?: \[lowering=([^\]]+)\])?", line)
        if m:
            name, status, depth = m.group(1), m.group(2), m.group(3)
            tagname = m.group(1).lstrip("\\")
            if tagname.startswith("bad_"):
                tagname = tagname[4:]
            LOWERING.setdefault(path, {}).setdefault(tagname, m.group(4) or "untagged")
        else:
            m = re.match(r"\s*(proved|disproved|undecided)\s+(\S+)(?: \(frame (\d+)\))?"
                         r"(?: \[lowering=([^\]]+)\])?", line)
            if m:
                status, name, depth = m.group(1), m.group(2), m.group(3)
                if status == "disproved" and depth:
                    depth = int(depth) - 1  # frame N+1 -> depth N
                tagname = name.lstrip("\\")
                if tagname.startswith("bad_"):
                    tagname = tagname[4:]
                if tagname.endswith("$live"):
                    tagname = tagname[:-5]
                LOWERING.setdefault(path, {}).setdefault(tagname, m.group(4) or "untagged")
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
    return fold_live_legs(results), []


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


TAG_HIST = {}


def main():
    files = sorted(glob.glob(os.path.join(HERE, "t*.sv")))
    n_props = 0
    failures = []
    for path in files:
        rel = os.path.basename(path)
        exp = expectations(path)
        n_props += len(exp)
        file_failures = []
        for leg in (check_ic3, check_bmc):
            for msg in leg(path, exp):
                file_failures.append(f"{rel}: {msg}")
        failures.extend(file_failures)
        print(f"{rel}: {'ok' if not file_failures else 'ISSUES'} ({len(exp)} properties)", flush=True)
        tags = LOWERING.get(path, {})
        for name, (kind, n) in sorted(exp.items()):
            verdict = "PROVED" if kind == "proved" else ("DIS@%d" % n if n is not None else "DIS")
            print(f"  {verdict:9s} {name} [lowering={tags.get(name, 'untagged')}]", flush=True)
            TAG_HIST[tags.get(name, 'untagged')] = TAG_HIST.get(tags.get(name, 'untagged'), 0) + 1
    print("\nlowering tags over the expectations:")
    for tag, c in sorted(TAG_HIST.items(), key=lambda kv: -kv[1]):
        print(f"  {c:4d}  {tag}")
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
