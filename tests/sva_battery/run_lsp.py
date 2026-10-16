#!/usr/bin/env python3
r"""Gate for the LSP diagnostics server (lsp.py): exercises the
JSON-RPC protocol end-to-end against a live server process.

Scenario:
  1. initialize -> capabilities advertise full sync
  2. didOpen a document with a syntax error at line 4
     -> publishDiagnostics: one severity-1 diagnostic at line 3 (0-based)
  3. didChange fixing the error
     -> publishDiagnostics: empty (the "incremental" recheck)
  4. didOpen a document with a located frontend warning (unknown type
     name) -> publishDiagnostics: severity-2 diagnostic
  5. didOpen a clean document -> publishDiagnostics: empty
  6. shutdown/exit -> server exits 0

Exit 0 iff every expectation holds.
"""

import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
YOSYS = os.environ.get("YOSYS", os.path.join(ROOT, "build", "yosys"))

BAD = """module bad(input logic clk, input logic [3:0] a);
  logic [3:0] r;
  always_ff @(posedge clk) begin
    r <= a +;
  end
endmodule
"""

BAD_FIXED = """module bad(input logic clk, input logic [3:0] a);
  logic [3:0] r;
  always_ff @(posedge clk) begin
    r <= a + 4'd1;
  end
endmodule
"""

WARN = """module warn(input logic clk, input string fname);
  logic [7:0] mem [0:15];
  initial begin
    $readmemh(fname, mem);
  end
endmodule
"""

CLEAN = """module clean(input logic clk, input logic [3:0] a, output logic [3:0] o);
  always_ff @(posedge clk) o <= a + 4'd1;
endmodule
"""


class Client:
    def __init__(self, proc):
        self.proc = proc
        self.mid = 0
        self.published = {}

    def _send(self, msg):
        body = json.dumps(msg).encode()
        self.proc.stdin.write(("Content-Length: %d\r\n\r\n" % len(body)).encode())
        self.proc.stdin.write(body)
        self.proc.stdin.flush()

    def _recv(self):
        headers = {}
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise RuntimeError("server closed the connection")
            line = line.decode().strip()
            if not line:
                break
            k, v = line.split(":", 1)
            headers[k.strip().lower()] = v.strip()
        body = self.proc.stdout.read(int(headers["content-length"]))
        return json.loads(body.decode())

    def request(self, method, params=None):
        self.mid += 1
        self._send({"jsonrpc": "2.0", "id": self.mid, "method": method,
                    "params": params or {}})
        while True:
            msg = self._recv()
            self._on_notification(msg)
            if msg.get("id") == self.mid:
                if "error" in msg:
                    raise RuntimeError("rpc error: %s" % msg["error"])
                return msg.get("result")

    def notify(self, method, params=None):
        self._send({"jsonrpc": "2.0", "method": method, "params": params or {}})

    def _on_notification(self, msg):
        if msg.get("method") == "textDocument/publishDiagnostics":
            self.published[msg["params"]["uri"]] = msg["params"]["diagnostics"]

    def expect_diags(self, uri, timeout_msgs=50):
        """Read notifications until a publish for `uri` arrives."""
        for _ in range(timeout_msgs):
            if uri in self.published:
                return self.published.pop(uri)
            msg = self._recv()
            self._on_notification(msg)
        raise RuntimeError("no publishDiagnostics for %s" % uri)


def main():
    env = dict(os.environ, YOSYS=YOSYS)
    server = subprocess.Popen(
        [sys.executable, os.path.join(HERE, "lsp.py"), "--yosys", YOSYS],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, env=env)
    client = Client(server)
    failures = []

    def check(name, cond, detail=""):
        if cond:
            print("lsp gate: ok  %s" % name)
        else:
            print("lsp gate: FAIL %s %s" % (name, detail))
            failures.append(name)

    try:
        caps = client.request("initialize", {"processId": os.getpid(),
                                             "rootUri": None,
                                             "capabilities": {}})
        check("initialize caps", caps.get("capabilities", {}).get("textDocumentSync") == 1,
              json.dumps(caps)[:120])
        client.notify("initialized")

        uri_bad = "file:///lsp_bad.sv"
        client.notify("textDocument/didOpen",
                      {"textDocument": {"uri": uri_bad, "languageId": "systemverilog",
                                        "version": 1, "text": BAD}})
        diags = client.expect_diags(uri_bad)
        check("syntax error diag", len(diags) == 1 and diags[0]["severity"] == 1
              and diags[0]["range"]["start"]["line"] == 3,
              json.dumps(diags)[:200])

        client.notify("textDocument/didChange",
                      {"textDocument": {"uri": uri_bad, "version": 2},
                       "contentChanges": [{"text": BAD_FIXED}]})
        diags = client.expect_diags(uri_bad)
        check("fix clears diags", diags == [], json.dumps(diags)[:200])

        uri_warn = "file:///lsp_warn.sv"
        client.notify("textDocument/didOpen",
                      {"textDocument": {"uri": uri_warn, "languageId": "systemverilog",
                                        "version": 1, "text": WARN}})
        diags = client.expect_diags(uri_warn)
        check("warning diag", any(d["severity"] == 2 for d in diags),
              json.dumps(diags)[:200])

        uri_ok = "file:///lsp_ok.sv"
        client.notify("textDocument/didOpen",
                      {"textDocument": {"uri": uri_ok, "languageId": "systemverilog",
                                        "version": 1, "text": CLEAN}})
        diags = client.expect_diags(uri_ok)
        check("clean doc", diags == [], json.dumps(diags)[:200])

        # unknown request gets a method-not-found error response
        client.mid += 1
        client._send({"jsonrpc": "2.0", "id": client.mid, "method": "bogus/method",
                      "params": {}})
        while True:
            msg = client._recv()
            client._on_notification(msg)
            if msg.get("id") == client.mid:
                check("unknown method error", "error" in msg, json.dumps(msg)[:120])
                break

        client.request("shutdown")
        client.notify("exit")
        rc = server.wait(timeout=10)
        check("exit code", rc == 0, "rc=%s" % rc)
    finally:
        if server.poll() is None:
            server.kill()
            server.wait()

    if failures:
        print("lsp gate: %d FAILURES" % len(failures))
        return 1
    print("lsp gate: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
