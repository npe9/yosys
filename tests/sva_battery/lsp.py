#!/usr/bin/env python3
r"""Minimal LSP (Language Server Protocol) server for SystemVerilog
diagnostics, backed by the sv-yosys Verilog frontend.

Scope (register item "language server", diagnostics layer v1):
  - textDocument sync: FULL -- every didOpen/didChange re-lints the
    document through `yosys read_verilog -sv` (+ `hierarchy -check`)
    and publishes diagnostics.  The frontend is milliseconds per file,
    so full-sync re-lint is the honest v1 "incremental" story; a true
    incremental AST is future work.
  - Diagnostics: located and unlocated frontend errors and warnings,
    mapped from yosys log lines:
        file.sv:12: ERROR: message        -> severity 1 at line 12
        file.sv:1: Warning: message       -> severity 2 at line 1
        ERROR: message / Warning: message -> the linted file, line 1
  - No hover/definition/completion yet (needs a frontend symbol index).

Stdlib only: JSON-RPC over Content-Length-framed stdio.  Usage:
    python3 lsp.py [--yosys PATH]          (editor launches this)
Environment: YOSYS overrides --yosys.
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
YOSYS = os.environ.get("YOSYS", os.path.join(ROOT, "build", "yosys"))

# yosys log shapes (probe-verified):
#   bad2.sv:4: ERROR: syntax error, unexpected ';'
#   warn6.sv:5: ERROR:Failed to evaluate ...        (note: no space)
#   warn8.sv:1: Warning: Unknown identifier ...
#   ERROR: ... / Warning: ...                       (no location)
LOCATED = re.compile(r"^(?P<file>[^:\s][^:]*):(?P<line>\d+): (?P<sev>ERROR|Warning):\s?(?P<msg>.*)$")
UNLOCATED = re.compile(r"^(?P<sev>ERROR|Warning):\s?(?P<msg>.*)$")

SEVERITY = {"ERROR": 1, "Warning": 2}

LINT_SCRIPT = "read_verilog -sv {file}\nhierarchy -check\n"


def lint(text, yosys=None):
    """Run the frontend over `text`; return a list of LSP diagnostics."""
    yosys = yosys or YOSYS
    with tempfile.NamedTemporaryFile("w", suffix=".sv", delete=False) as fh:
        fh.write(text)
        path = fh.name
    try:
        # -q: banner/pass headers suppressed, warnings and errors kept
        proc = subprocess.run(
            [yosys, "-q", "-p", LINT_SCRIPT.format(file=path)],
            capture_output=True, text=True, timeout=30)
    except subprocess.TimeoutExpired:
        os.unlink(path)
        return [{"range": {"start": {"line": 0, "character": 0},
                           "end": {"line": 0, "character": 0}},
                 "severity": 1, "source": "yosys",
                 "message": "lint timed out"}]
    os.unlink(path)
    diags = []
    seen = set()
    for line in (proc.stderr + "\n" + proc.stdout).splitlines():
        m = LOCATED.match(line.strip()) or UNLOCATED.match(line.strip())
        if not m:
            continue
        msg = m.group("msg").strip()
        key = (m.group("sev"), msg)
        if key in seen:
            continue
        seen.add(key)
        lineno = int(m.groupdict().get("line") or 1) - 1
        diags.append({
            "range": {"start": {"line": lineno, "character": 0},
                      "end": {"line": lineno, "character": 0}},
            "severity": SEVERITY[m.group("sev")],
            "source": "yosys",
            "message": msg,
        })
    return diags


class JsonRpc:
    """Content-Length-framed JSON-RPC over binary stdio streams."""

    def __init__(self, rfile, wfile):
        self.rfile = rfile
        self.wfile = wfile

    def read(self):
        headers = {}
        while True:
            line = self.rfile.readline()
            if not line:
                return None
            line = line.decode("ascii", "replace").strip()
            if not line:
                break
            if ":" in line:
                k, v = line.split(":", 1)
                headers[k.strip().lower()] = v.strip()
        n = int(headers.get("content-length", 0))
        body = self.rfile.read(n)
        if not body:
            return None
        return json.loads(body.decode("utf-8"))

    def write(self, msg):
        body = json.dumps(msg).encode("utf-8")
        self.wfile.write(("Content-Length: %d\r\n\r\n" % len(body)).encode("ascii"))
        self.wfile.write(body)
        self.wfile.flush()


class Server:
    def __init__(self, rpc, yosys=None):
        self.rpc = rpc
        self.yosys = yosys or YOSYS
        self.docs = {}        # uri -> text
        self.last_hash = {}   # uri -> sha1 of last linted text
        self.shutdown = False

    def publish(self, uri):
        text = self.docs.get(uri, "")
        h = hashlib.sha1(text.encode("utf-8")).hexdigest()
        if self.last_hash.get(uri) == h:
            return
        self.last_hash[uri] = h
        self.rpc.write({"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics",
                        "params": {"uri": uri, "diagnostics": lint(text, self.yosys)}})

    def handle(self, msg):
        method = msg.get("method")
        mid = msg.get("id")
        params = msg.get("params") or {}
        if method == "initialize":
            return {"capabilities": {
                "textDocumentSync": 1,  # full
                "positionEncoding": "utf-16",
                "diagnosticProvider": {"identifier": "yosys",
                                       "interFileDependencies": False,
                                       "workspaceDiagnostics": False},
            }}
        if method == "initialized":
            return None
        if method == "shutdown":
            self.shutdown = True
            return None
        if method == "exit":
            sys.exit(0 if self.shutdown else 1)
        if method == "textDocument/didOpen":
            doc = params.get("textDocument", {})
            self.docs[doc.get("uri", "")] = doc.get("text", "")
            self.publish(doc.get("uri", ""))
            return None
        if method == "textDocument/didChange":
            uri = params.get("textDocument", {}).get("uri", "")
            changes = params.get("contentChanges", [])
            if changes:
                self.docs[uri] = changes[-1].get("text", "")
            self.publish(uri)
            return None
        if method == "textDocument/didClose":
            uri = params.get("textDocument", {}).get("uri", "")
            self.docs.pop(uri, None)
            self.last_hash.pop(uri, None)
            self.rpc.write({"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics",
                            "params": {"uri": uri, "diagnostics": []}})
            return None
        # pull-model diagnostic request (LSP 3.17): answer from cache
        if method == "textDocument/diagnostic":
            uri = params.get("textDocument", {}).get("uri", "")
            return {"kind": "full", "items": lint(self.docs.get(uri, ""), self.yosys)}
        # unknown request vs notification: respond to requests only
        if mid is not None:
            return {"error": {"code": -32601, "message": "method not found: %s" % method}}
        return None

    def loop(self):
        while True:
            msg = self.rpc.read()
            if msg is None:
                break
            if not isinstance(msg, dict) or msg.get("jsonrpc") != "2.0":
                continue
            result = self.handle(msg)
            if "id" in msg and msg.get("method"):  # a request, needs a response
                resp = {"jsonrpc": "2.0", "id": msg["id"]}
                if isinstance(result, dict) and "error" in result:
                    resp["error"] = result["error"]
                else:
                    resp["result"] = result
                self.rpc.write(resp)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--yosys", default=YOSYS)
    args = ap.parse_args()
    rpc = JsonRpc(sys.stdin.buffer, sys.stdout.buffer)
    Server(rpc, args.yosys).loop()


if __name__ == "__main__":
    main()
