#!/usr/bin/env python3
"""Parse a SystemVerilog tree with the local Yosys frontend and tally failures."""

import argparse
import os
import re
import resource
import subprocess
import tempfile
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed

KEYS = (
    "assert property",
    "`ASSERT",
    "`ASSUME",
    "`COVER",
    "assume property",
    "cover property",
    "cover sequence",
    "endproperty",
)


def iter_files(root, excludes):
    for dirpath, dirnames, files in os.walk(root):
        dirnames[:] = [d for d in dirnames if d != ".git"]
        rel = os.path.relpath(dirpath, root)
        if rel != "." and any(rel == ex or rel.startswith(ex + os.sep) for ex in excludes):
            dirnames[:] = []
            continue
        for fn in files:
            if fn.endswith((".sv", ".svh", ".v")):
                yield os.path.join(dirpath, fn)


def normalize(msg):
    msg = re.sub(r"/[^\s:]+\.(svh|sv|v)", "<file>", msg)
    msg = re.sub(r":\d+:", ":", msg)
    msg = re.sub(r"\s+", " ", msg).strip()
    return msg[:180]


def parse_one(yosys, path, inc_flags, timeout, assert_header, uvm_pkg="", defines=""):
    try:
        text = open(path, errors="ignore").read()
    except OSError as exc:
        return path, "fail", "read error %s" % exc
    if assert_header:
        text = text.replace('`include "prim_assert.sv"', '`include "%s"' % assert_header)
        if '`include "prim_assert' not in text and re.search(r"`[A-Z][A-Z0-9_]*\(", text):
            # Assertion fragment files (e.g. OpenTitan fpv/vip) use the
            # prim_assert macros without including the header themselves;
            # the including tb normally provides it. Prepend it.
            text = '`include "%s"\n' % assert_header + text
    with tempfile.NamedTemporaryFile("w", suffix=".sv", delete=False) as fh:
        fh.write(text)
        wrap = fh.name
    prelude = ""
    uses_uvm = uvm_pkg and ("uvm_pkg" in text or "uvm_macros" in text)
    if uses_uvm:
        # Read the (externized) UVM core first so that `import uvm_pkg::*`
        # and `uvm_macros.svh` resolve when the file is read with -DUVM.
        # -defer keeps the (unsynthesizable) class library out of genrtlil.
        prelude = "read_verilog -sv -formal -defer " + defines + inc_flags + " " + uvm_pkg + "\n"
    with tempfile.NamedTemporaryFile("w", suffix=".ys", delete=False) as fh:
        fh.write(prelude + "read_verilog -sv -formal " + (defines if uses_uvm else "") + inc_flags + " " + wrap + "\n")
        script = fh.name
    # A long -p argument shares the stack with elaboration and can SIGSEGV.
    if os.uname().sysname == "Darwin":
        cmd = ["zsh", "-f", "-c", 'ulimit -s 65520; exec "$1" -q "$2"', "yosys", yosys, script]
    else:
        cmd = [yosys, "-q", script]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        os.unlink(wrap)
        os.unlink(script)
        return path, "timeout", "timeout"
    os.unlink(wrap)
    os.unlink(script)
    blob = (proc.stdout or "") + "\n" + (proc.stderr or "")
    if proc.returncode == 0:
        return path, "ok", ""
    if proc.returncode < 0:
        return path, "crash", "signal %d" % (-proc.returncode)
    err = ""
    for line in blob.splitlines():
        if "ERROR:" in line:
            err = line.split("ERROR:", 1)[1].strip()
    return path, "fail", normalize(err or "unknown failure")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", required=True)
    ap.add_argument("--root", required=True)
    ap.add_argument("--yosys", required=True)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--timeout", type=int, default=20)
    ap.add_argument("--exclude", action="append", default=[])
    ap.add_argument("--prim-assert", default="")
    ap.add_argument(
        "--uvm",
        default=os.environ.get("UVM_HOME", os.path.join(os.path.dirname(os.path.abspath(__file__)), "uvm_stub")),
        help="Directory containing uvm_macros.svh (UVM core src).",
    )
    ap.add_argument(
        "--uvm-pkg",
        default=os.environ.get("UVM_PKG", "/tmp/uvm-filt/src/uvm_pkg.sv"),
        help="uvm_pkg.sv to read before files that reference UVM (empty to disable).",
    )
    ap.add_argument(
        "--uvm-define",
        action="store_true",
        help="Pass -DUVM so `ifdef UVM regions are parsed (requires --uvm-pkg).",
    )
    args = ap.parse_args()
    soft, hard = resource.getrlimit(resource.RLIMIT_STACK)
    target = 64 * 1024 * 1024
    if hard != resource.RLIM_INFINITY and 0 < hard < target:
        target = hard
    if soft < target:
        try:
            resource.setrlimit(resource.RLIMIT_STACK, (target, hard))
        except (ValueError, OSError):
            pass
    root = os.path.abspath(args.root)
    files = []
    inc_dirs = []
    seen = set()
    # Include search paths come from the whole tree: an excluded directory can
    # still hold headers (e.g. ibex vendor/lowrisc_ip/dv/sv/dv_utils for
    # dv_fcov_macros.svh) that surviving files include.
    for path in iter_files(root, []):
        parent = os.path.dirname(path)
        # Includes are written as "common_cells/registers.svh". The -I
        # directory is the parent of that prefix, not the prefix itself.
        candidates = [parent]
        if os.path.basename(parent) in ("common_cells", "axi"):
            candidates.append(os.path.dirname(parent))
        for directory in candidates:
            if directory not in seen:
                seen.add(directory)
                inc_dirs.append(directory)
    # Header-only directories (e.g. veer's design/include with *.vh) hold no
    # .sv/.svh/.v files, so the walk above never sees them; `include of those
    # headers still needs them on the search path.
    for dirpath, dirnames, fns in os.walk(root):
        dirnames[:] = [d for d in dirnames if d != ".git"]
        if any(fn.endswith(".vh") for fn in fns):
            if dirpath not in seen:
                seen.add(dirpath)
                inc_dirs.append(dirpath)
    for path in iter_files(root, args.exclude):
        try:
            text = open(path, errors="ignore").read()
        except OSError:
            continue
        if any(k in text for k in KEYS):
            files.append(path)
    files.sort()
    uvm = os.path.abspath(args.uvm) if args.uvm else ""
    if uvm and os.path.isdir(uvm) and uvm not in seen:
        inc_dirs.insert(0, uvm)
    uvm_pkg = ""
    defines = ""
    if args.uvm_define:
        if not args.uvm_pkg or not os.path.isfile(args.uvm_pkg):
            ap.error("--uvm-define requires an existing --uvm-pkg (run conformance/uvm_externize.py first)")
        uvm_pkg = os.path.abspath(args.uvm_pkg)
        defines = "-DUVM "
    header = os.path.abspath(args.prim_assert) if args.prim_assert else ""
    inc_flags = " ".join("-I" + d for d in inc_dirs)
    counts = Counter()
    buckets = Counter()
    passed = []
    print("CORPUS", args.name)
    print("ROOT", root)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futs = [
            pool.submit(
                parse_one,
                args.yosys,
                path,
                inc_flags,
                args.timeout,
                header,
                uvm_pkg,
                defines,
            )
            for path in files
        ]
        for fut in as_completed(futs):
            path, status, detail = fut.result()
            counts[status] += 1
            if status == "ok":
                passed.append(os.path.relpath(path, root))
            else:
                buckets[detail] += 1
    print("FILES", len(files))
    for key in ("ok", "fail", "crash", "timeout"):
        print("%s %d" % (key.upper(), counts[key]))
    print("PASSED")
    for rel in sorted(passed):
        print(rel)
    print("TOP_ERRORS")
    for detail, n in buckets.most_common(15):
        print("%4d  %s" % (n, detail))
    print()


if __name__ == "__main__":
    main()
