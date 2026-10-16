#!/usr/bin/env python3
r"""Upstream-PR packaging: materialize a reviewable chunk of the
sva-frontend-ic3 fork as a branch off current upstream main.

The fork's 52 commits tangle features (sva2check, ic3, cdc, sim xprop,
battery), so chunks are FILE MANIFESTS, not cherry-picks: each chunk
branch starts at origin/main and receives the chunk's files from the
fork HEAD plus anchored CMake insertions.  Every chunk must build and
pass its own gate before it is pushed to fork/upstream/<name>; nothing
is ever pushed to YosysHQ from here (opening the actual PRs is a
human decision after review).

Chunks (see UPSTREAMING.md at the repo root for the full plan):
  cdc-sdc   structural CDC pass + SDC subset frontend + cases + gate
            (pure additions, no upstream drift on any file)

Usage: python3 upstream_chunk.py <chunk> [--push] [--keep]
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
CHUNK_BRANCH = "sva-frontend-ic3"

CDC_CMAKE_ANCHOR = "yosys_pass(edgetypes"
CDC_CMAKE_STANZAS = """yosys_pass(cdc
	cdc.cc
)
yosys_pass(read_sdc
	read_sdc.cc
)
"""

CHUNKS = {
    "cdc-sdc": {
        "files": [
            "passes/cmds/cdc.cc",
            "passes/cmds/read_sdc.cc",
            "passes/cmds/sdc_constraints.h",
            "tests/verilog/cdc_cases.sv",
            "tests/verilog/cdc_cases.sdc",
            "tests/verilog/cdc_cases.ys",
            "tests/sva_battery/run_cdc.py",
        ],
        "cmake": [(CDC_CMAKE_ANCHOR, CDC_CMAKE_STANZAS)],
        # main-port deltas: upstream's twine ID system dropped $nand/$nor/$dl
        # from the static constids table (proc normalizes them), and log_id
        # is gone (IdString::unescape() replaces it).  The fork's older
        # kernel still has all of these, so the fork sources stay canonical
        # and the port is recorded here per chunk.
        "port_patches": [
            ("passes/cmds/cdc.cc",
             "return cell->type.in(ID($and), ID($nand), ID($or), ID($nor),\n"
             "\t\t\t\tID($xor), ID($xnor), ID($not), ID($mux));",
             "return cell->type.in(ID($and), ID($or),\n"
             "\t\t\t\t// $nand/$nor left the static constids table on main\n"
             "\t\t\t\t// (proc normalizes them to $and/$or + $not)\n"
             "\t\t\t\tID($xor), ID($xnor), ID($not), ID($mux));"),
            ("passes/cmds/cdc.cc", "ID($dl), ID($dlatch)", "ID($dlatch)"),
            ("passes/cmds/cdc.cc", "log_id(module->name)",
             "module->name.unescape()"),
            ("passes/cmds/cdc.cc", "f.module = log_id(module);",
             "f.module = module->name.unescape();"),
        ],
        "gate": [sys.executable, os.path.join(HERE, "run_cdc.py")],
    },
}


def run(cmd, cwd=None, **kw):
    print("+", " ".join(cmd), flush=True)
    return subprocess.run(cmd, cwd=cwd, check=True, **kw)


def ensure_shadow_pkgconfig():
    """The shadow libedit.pc: this machine's system libedit.pc references
    a non-existent CLT SDK include dir, and upstream main's try_compile
    usability check fails the configure on it (the daily fork build
    survives on a stale CMake cache, which is why only fresh chunk
    builds hit this)."""
    d = "/tmp/pkgconfig-upstream"
    os.makedirs(d, exist_ok=True)
    pc = os.path.join(d, "libedit.pc")
    if not os.path.exists(pc):
        open(pc, "w").write(
            "prefix=/opt/homebrew\n"
            "exec_prefix=${prefix}\n"
            "libdir=${exec_prefix}/lib\n"
            "includedir=${prefix}/include\n\n"
            "Name: libedit\n"
            "Description: shadow .pc for chunk builds (system one references "
            "a missing SDK path)\n"
            "Version: 3.1\n"
            "Libs: -L${libdir} -ledit\n"
            "Cflags: -I${includedir}\n")
    # earlier revisions of this helper wrote editline.pc; the cmake
    # module searched is libedit, so a stale editline shadow is dead
    # weight -- drop it
    stale = os.path.join(d, "editline.pc")
    if os.path.exists(stale):
        os.unlink(stale)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("chunk", choices=sorted(CHUNKS))
    ap.add_argument("--push", action="store_true",
                    help="push the branch to fork/upstream/<chunk> after "
                         "build+gate pass")
    ap.add_argument("--keep", action="store_true",
                    help="keep the scratch worktree")
    args = ap.parse_args()
    spec = CHUNKS[args.chunk]
    branch = "upstream/%s" % args.chunk

    os.chdir(REPO)
    wt = os.path.join(tempfile.gettempdir(), "ychunk_%s" % args.chunk)
    if os.path.exists(wt):
        run(["git", "worktree", "remove", "--force", wt])
    if subprocess.run(["git", "rev-parse", "--verify", branch],
                      capture_output=True).returncode == 0:
        run(["git", "branch", "-D", branch])
    run(["git", "worktree", "add", wt, "-b", branch, "origin/main"])
    # worktrees don't check out submodules; abc is required to build
    run(["git", "-C", wt, "submodule", "update", "--init"])

    try:
        # chunk files from the fork HEAD
        run(["git", "checkout", CHUNK_BRANCH, "--"] + spec["files"], cwd=wt)
        # anchored CMake insertions
        for anchor, text in spec.get("cmake", []):
            path = os.path.join(wt, "passes", "cmds", "CMakeLists.txt")
            lines = open(path).read().splitlines(keepends=True)
            idx = next((i for i, l in enumerate(lines) if anchor in l), None)
            if idx is None:
                raise SystemExit("cmake anchor %r not found in %s"
                                 % (anchor, path))
            lines.insert(idx, text)
            open(path, "w").writelines(lines)
        # main-port patches (see the chunk manifest)
        for relpath, old, new in spec.get("port_patches", []):
            path = os.path.join(wt, relpath)
            src = open(path).read()
            if old not in src:
                raise SystemExit("port patch anchor not found in %s:\n%s"
                                 % (relpath, old[:120]))
            src = src.replace(old, new)
            open(path, "w").write(src)
        run(["git", "-C", wt, "add", "-A"])
        run(["git", "-C", wt, "-c", "user.name=npe", "-c",
             "user.email=npe@users.noreply.github.com", "commit",
             "-m", "Upstream chunk %s (see UPSTREAMING.md on the fork)\n\n"
                   "File-manifest packaging of the sva-frontend-ic3 fork; "
                   "each file is identical to the reviewed, gated state on "
                   "the fork branch." % args.chunk])

        # build against upstream main (see ensure_shadow_pkgconfig for why)
        ensure_shadow_pkgconfig()
        env = dict(os.environ, PKG_CONFIG_PATH="/tmp/pkgconfig-upstream:/tmp/pkgconfig")
        run(["cmake", "-S", ".", "-B", "build",
             # Tcl found via pkg-config on this machine links but its
             # symbols don't resolve here; chunk features need no Tcl
             "-DYOSYS_WITHOUT_TCL=ON"],
            cwd=wt, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        run(["cmake", "--build", "build", "-j", "8"], cwd=wt, env=env)

        # chunk gate
        if spec.get("gate"):
            env = dict(os.environ,
                       YOSYS=os.path.join(wt, "build", "yosys"))
            run(spec["gate"], cwd=HERE, env=env)

        if args.push:
            run(["git", "-C", wt, "push", "-f", "fork", branch])
        print("chunk %s: OK (branch %s%s)" %
              (args.chunk, branch, ", pushed to fork" if args.push else ""))
        return 0
    finally:
        if not args.keep and os.path.exists(wt):
            run(["git", "worktree", "remove", "--force", wt])


if __name__ == "__main__":
    sys.exit(main())
