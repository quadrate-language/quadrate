#!/usr/bin/env python3
"""
Robustness tests for the Quadrate language server.

Every case here is a regression for a crash or a wedge found by fuzzing the
server the way a broken -- or merely unusual -- client would drive it. The
server must answer or ignore each one, and must never terminate on its own.

Covered:
  * request ids that are strings, floats or objects (JSON-RPC allows all of
    them; stoi() on the id aborted the process)
  * Content-Length headers that are not usable numbers
  * didClose evicting the document and retiring its diagnostics
  * didChange with empty text being honoured rather than skipped
  * out-of-range positions, unknown URIs and unknown methods
"""

import json
import subprocess
import sys
from pathlib import Path

TIMEOUT = 30


def frame(obj):
    body = json.dumps(obj).encode()
    return b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body


INIT = {"jsonrpc": "2.0", "id": 1, "method": "initialize",
        "params": {"processId": None, "rootUri": None, "capabilities": {}}}

DOC_URI = "file:///robustness.qd"
DOC_TEXT = "fn helper() { }\nfn main() { helper }\n"


def did_open(uri=DOC_URI, text=DOC_TEXT, version=1):
    return {"jsonrpc": "2.0", "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": uri, "languageId": "quadrate",
                                        "version": version, "text": text}}}


class Runner:
    def __init__(self, lsp_path):
        self.lsp = str(lsp_path)
        self.passed = 0
        self.failed = 0

    def drive(self, data):
        """Feed raw bytes to a fresh server; return (returncode, responses)."""
        try:
            proc = subprocess.run([self.lsp], input=data, capture_output=True, timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            return None, []
        responses = []
        for chunk in proc.stdout.split(b"Content-Length:"):
            _, _, body = chunk.partition(b"\r\n\r\n")
            body = body.strip()
            if not body:
                continue
            try:
                responses.append(json.loads(body))
            except json.JSONDecodeError:
                pass
        return proc.returncode, responses

    def check(self, name, condition, detail=""):
        if condition:
            self.passed += 1
            print("✓ %s" % name)
        else:
            self.failed += 1
            print("✗ %s%s" % (name, (" -- " + detail) if detail else ""))

    def survives(self, name, data):
        """The server must exit cleanly (never on a signal) and never hang."""
        rc, responses = self.drive(data)
        if rc is None:
            self.check(name, False, "server hung")
            return []
        if rc < 0:
            self.check(name, False, "server died on signal %d" % -rc)
            return []
        self.check(name, True)
        return responses

    # -- request ids ----------------------------------------------------
    def test_ids(self):
        print("\n-- request ids --")
        cases = {
            "string id": "abc-123",
            "numeric string id": "5",
            "empty string id": "",
            "unicode id": "id-åäö",
        }
        for name, value in cases.items():
            data = frame(INIT) + frame({"jsonrpc": "2.0", "id": value,
                                        "method": "textDocument/documentSymbol",
                                        "params": {"textDocument": {"uri": DOC_URI}}})
            self.survives("survives a %s" % name, data)

        # A string id must come back as a string, not coerced to a number.
        rc, responses = self.drive(
            frame(INIT) + frame(did_open())
            + frame({"jsonrpc": "2.0", "id": "5", "method": "textDocument/documentSymbol",
                     "params": {"textDocument": {"uri": DOC_URI}}}))
        echoed = [r.get("id") for r in responses if r.get("id") is not None]
        self.check("a string id is echoed back as a string", "5" in echoed,
                   "ids seen: %r" % (echoed,))

        # An integer id must stay an integer.
        rc, responses = self.drive(
            frame(INIT) + frame(did_open())
            + frame({"jsonrpc": "2.0", "id": 7, "method": "textDocument/documentSymbol",
                     "params": {"textDocument": {"uri": DOC_URI}}}))
        echoed = [r.get("id") for r in responses if r.get("id") is not None]
        self.check("an integer id is echoed back as an integer", 7 in echoed,
                   "ids seen: %r" % (echoed,))

        for name, value in {"float id": 1.5, "object id": {"a": 1}, "array id": [1]}.items():
            self.survives("survives a %s" % name,
                          frame(INIT) + frame({"jsonrpc": "2.0", "id": value,
                                               "method": "shutdown", "params": {}}))

    # -- framing --------------------------------------------------------
    def test_framing(self):
        print("\n-- message framing --")
        raw = {
            "non-numeric Content-Length": b"Content-Length: abc\r\n\r\n{}",
            "negative Content-Length": b"Content-Length: -1\r\n\r\n{}",
            "overflowing Content-Length": b"Content-Length: 999999999999\r\n\r\n{}",
            "Content-Length beyond the cap": b"Content-Length: 99999999\r\n\r\n{}",
            "zero Content-Length": b"Content-Length: 0\r\n\r\n",
            "header without a body": b"Content-Length: 500\r\n\r\n{\"jsonrpc\"",
            "no header at all": b'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}',
            "body that is not JSON": frame({}) [:-2] + b"xx",
            "JSON that is not an object": b"Content-Length: 7\r\n\r\n[1,2,3]",
        }
        for name, data in raw.items():
            self.survives("survives %s" % name, data)

    # -- document lifecycle ---------------------------------------------
    def test_lifecycle(self):
        print("\n-- document lifecycle --")

        def symbols_after(*messages):
            data = frame(INIT) + b"".join(frame(m) for m in messages) + frame(
                {"jsonrpc": "2.0", "id": 99, "method": "textDocument/documentSymbol",
                 "params": {"textDocument": {"uri": DOC_URI}}})
            _, responses = self.drive(data)
            for r in responses:
                if r.get("id") == 99:
                    return r.get("result") or []
            return None

        base = symbols_after(did_open())
        self.check("an open document reports its symbols", bool(base),
                   "got %r" % (base,))

        cleared = symbols_after(did_open(), {
            "jsonrpc": "2.0", "method": "textDocument/didChange",
            "params": {"textDocument": {"uri": DOC_URI, "version": 2},
                       "contentChanges": [{"text": ""}]}})
        self.check("didChange to empty text clears the document", cleared == [],
                   "expected no symbols, got %r" % (cleared,))

        closed = symbols_after(did_open(), {
            "jsonrpc": "2.0", "method": "textDocument/didClose",
            "params": {"textDocument": {"uri": DOC_URI}}})
        self.check("didClose evicts the document", closed == [],
                   "expected no symbols, got %r" % (closed,))

        # Closing must retire the squiggles the client is still showing.
        data = (frame(INIT)
                + frame(did_open(text="fn main( { oh no\n"))
                + frame({"jsonrpc": "2.0", "method": "textDocument/didClose",
                         "params": {"textDocument": {"uri": DOC_URI}}}))
        _, responses = self.drive(data)
        published = [r["params"]["diagnostics"] for r in responses
                     if r.get("method") == "textDocument/publishDiagnostics"]
        self.check("a broken document publishes diagnostics",
                   any(len(d) > 0 for d in published), "published: %r" % (published,))
        self.check("didClose publishes an empty diagnostics list",
                   bool(published) and published[-1] == [],
                   "last publish was %r" % (published[-1] if published else None,))

        self.survives("survives didClose for a document never opened",
                      frame(INIT) + frame({"jsonrpc": "2.0", "method": "textDocument/didClose",
                                           "params": {"textDocument": {"uri": "file:///nope.qd"}}}))
        self.survives("survives a repeated didClose",
                      frame(INIT) + frame(did_open())
                      + frame({"jsonrpc": "2.0", "method": "textDocument/didClose",
                               "params": {"textDocument": {"uri": DOC_URI}}}) * 3)
        self.survives("survives didChange with no text member",
                      frame(INIT) + frame(did_open())
                      + frame({"jsonrpc": "2.0", "method": "textDocument/didChange",
                               "params": {"textDocument": {"uri": DOC_URI},
                                          "contentChanges": [{}]}}))

    # -- hostile parameters ---------------------------------------------
    def test_params(self):
        print("\n-- request parameters --")
        pos = lambda line, ch: {"textDocument": {"uri": DOC_URI},
                                "position": {"line": line, "character": ch}}
        cases = {
            "negative positions": pos(-5, -5),
            "positions past the end": pos(10 ** 9, 10 ** 9),
            "int64-max positions": pos(2 ** 63 - 1, 2 ** 63 - 1),
            "fractional positions": pos(0.5, 0.5),
        }
        for name, params in cases.items():
            self.survives("survives %s" % name,
                          frame(INIT) + frame(did_open())
                          + frame({"jsonrpc": "2.0", "id": 2, "method": "textDocument/hover",
                                   "params": params}))

        self.survives("survives a request for an unopened document",
                      frame(INIT) + frame({"jsonrpc": "2.0", "id": 2,
                                           "method": "textDocument/hover", "params": pos(0, 0)}))
        self.survives("survives an unknown method",
                      frame(INIT) + frame({"jsonrpc": "2.0", "id": 2,
                                           "method": "nonsense/method", "params": {}}))
        self.survives("survives params of the wrong type",
                      frame(INIT) + frame({"jsonrpc": "2.0", "id": 2,
                                           "method": "textDocument/hover", "params": "a string"}))
        self.survives("survives a rename to an empty name",
                      frame(INIT) + frame(did_open())
                      + frame({"jsonrpc": "2.0", "id": 2, "method": "textDocument/rename",
                               "params": {**pos(0, 3), "newName": ""}}))
        self.survives("survives a rename to control characters",
                      frame(INIT) + frame(did_open())
                      + frame({"jsonrpc": "2.0", "id": 2, "method": "textDocument/rename",
                               "params": {**pos(0, 3), "newName": "[2J‮"}}))
        self.survives("survives call-hierarchy data with a non-numeric line",
                      frame(INIT) + frame(did_open())
                      + frame({"jsonrpc": "2.0", "id": 2, "method": "callHierarchy/incomingCalls",
                               "params": {"item": {"name": "main", "kind": 12, "uri": DOC_URI,
                                                   "data": "%s|main|not-a-number" % DOC_URI,
                                                   "range": {"start": {"line": 0, "character": 0},
                                                             "end": {"line": 0, "character": 1}},
                                                   "selectionRange": {
                                                       "start": {"line": 0, "character": 0},
                                                       "end": {"line": 0, "character": 1}}}}}))

    # -- size -----------------------------------------------------------
    def test_size(self):
        print("\n-- large input --")
        self.survives("survives a 1 MB document",
                      frame(INIT) + frame(did_open(uri="file:///big.qd",
                                                   text="fn main() { " + "1 drop " * 100000 + "}\n")))
        self.survives("survives a deeply nested document",
                      frame(INIT) + frame(did_open(uri="file:///deep.qd",
                                                   text="fn main() {" + "{" * 20000 + "}" * 20000 + "}\n")))
        self.survives("survives a document that is all parse errors",
                      frame(INIT) + frame(did_open(uri="file:///bad.qd", text="}" * 50000)))
        self.survives("survives a flood of requests",
                      frame(INIT) + frame(did_open()) + b"".join(
                          frame({"jsonrpc": "2.0", "id": i, "method": "textDocument/documentSymbol",
                                 "params": {"textDocument": {"uri": DOC_URI}}})
                          for i in range(3, 503)))

    def run(self):
        print("Quadrate LSP robustness tests\n" + "=" * 40)
        self.test_ids()
        self.test_framing()
        self.test_lifecycle()
        self.test_params()
        self.test_size()
        print("\n" + "=" * 40)
        print("passed: %d  failed: %d" % (self.passed, self.failed))
        return 1 if self.failed else 0


def main():
    candidates = [
        Path("dist/bin/quadlsp"),
        Path("build/release/cmd/quadlsp/quadlsp"),
        Path("build/debug/cmd/quadlsp/quadlsp"),
        Path("cmd/quadlsp/quadlsp"),
        Path("../../../cmd/quadlsp/quadlsp"),
    ]
    lsp = next((p for p in candidates if p.exists()), None)
    if lsp is None:
        print("❌ quadlsp not found; build the project first.")
        return 1
    print("Using LSP: %s\n" % lsp)
    return Runner(lsp).run()


if __name__ == "__main__":
    sys.exit(main())
