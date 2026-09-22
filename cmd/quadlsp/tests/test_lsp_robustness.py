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
  * frames that are unusual but valid keep the server running, and every
    request gets an answer or a JSON-RPC error
  * formatting documents larger than any fixed output buffer
  * UTF-16 positions, percent-encoded URIs and incremental didChange
  * rename, references, highlight and definition land on the exact name
"""

import json
import subprocess
import sys
import tempfile
import urllib.parse
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
                               "params": {**pos(0, 3), "newName": "[2J"}}))
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

    # -- helpers for request/response tests ----------------------------
    @staticmethod
    def request(req_id, method, params):
        return {"jsonrpc": "2.0", "id": req_id, "method": method, "params": params}

    @staticmethod
    def at(uri, line, character, **extra):
        params = {"textDocument": {"uri": uri}, "position": {"line": line, "character": character}}
        params.update(extra)
        return params

    def answer(self, messages, req_id, init=INIT):
        data = frame(init) + b"".join(m if isinstance(m, bytes) else frame(m) for m in messages)
        _, responses = self.drive(data)
        for r in responses:
            if r.get("id") == req_id and "method" not in r:
                return r
        return None

    @staticmethod
    def ranges(response):
        result = (response or {}).get("result") or []
        if isinstance(result, dict):
            result = [e for edits in (result.get("changes") or {}).values() for e in edits]
        return sorted((r["range"]["start"]["line"], r["range"]["start"]["character"],
                       r["range"]["end"]["character"]) for r in result)

    # -- framing and protocol errors ------------------------------------
    def test_protocol(self):
        print("\n-- protocol --")
        hover = self.request(5, "textDocument/hover", self.at(DOC_URI, 1, 13))
        body = json.dumps(hover).encode()
        frames = {
            "a lowercase content-length header": b"content-length: %d\r\n\r\n" % len(body) + body,
            "a header with no space after the colon": b"Content-Length:%d\r\n\r\n" % len(body) + body,
            "Content-Type after Content-Length":
                b"Content-Length: %d\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8\r\n\r\n"
                % len(body) + body,
            "a Content-Length: 0 frame before it": b"Content-Length: 0\r\n\r\n" + frame(hover),
            "a frame without Content-Length before it": b"Content-Type: text/plain\r\n\r\n" + frame(hover),
            "an oversized frame before it":
                b"Content-Length: 20000000\r\n\r\n" + b"x" * 20000000 + frame(hover),
        }
        for name, data in frames.items():
            response = self.answer([did_open(), data], 5)
            self.check("answers a request after %s" % name,
                       response is not None and "result" in response, "got %r" % (response,))

        response = self.answer([self.request(2, "nonsense/method", {})], 2)
        self.check("an unknown request gets MethodNotFound",
                   (response or {}).get("error", {}).get("code") == -32601, "got %r" % (response,))
        _, responses = self.drive(frame(INIT) + frame({"jsonrpc": "2.0", "method": "nonsense/notify"}))
        self.check("an unknown notification gets no response",
                   all(r.get("id") == 1 or "method" in r for r in responses), "got %r" % (responses,))
        for method, params in {"textDocument/hover": "a string",
                               "textDocument/definition": {"textDocument": {"uri": DOC_URI}},
                               "textDocument/rename": self.at(DOC_URI, 0, 3)}.items():
            response = self.answer([did_open(), self.request(2, method, params)], 2)
            self.check("%s with bad params gets InvalidParams" % method,
                       (response or {}).get("error", {}).get("code") == -32602, "got %r" % (response,))

        shutdown = self.request(2, "shutdown", None)
        response = self.answer([shutdown, self.request(3, "textDocument/documentSymbol",
                                                       {"textDocument": {"uri": DOC_URI}})], 3)
        self.check("a request after shutdown gets InvalidRequest",
                   (response or {}).get("error", {}).get("code") == -32600, "got %r" % (response,))
        exit_msg = {"jsonrpc": "2.0", "method": "exit"}
        rc, _ = self.drive(frame(INIT) + frame(shutdown) + frame(exit_msg))
        self.check("exit after shutdown exits 0", rc == 0, "rc %r" % (rc,))
        rc, _ = self.drive(frame(INIT) + frame(exit_msg))
        self.check("exit without shutdown exits 1", rc == 1, "rc %r" % (rc,))

    # -- formatting -----------------------------------------------------
    def test_formatting(self):
        print("\n-- formatting --")
        uri = "file:///big_format.qd"
        text = "".join("fn f%d( -- x:i64) {\n1\n}\n" % i for i in range(12000)) + "fn main() {\n f0 print\n}\n"
        response = self.answer([did_open(uri=uri, text=text),
                                self.request(2, "textDocument/formatting",
                                             {"textDocument": {"uri": uri},
                                              "options": {"tabSize": 4, "insertSpaces": False}})], 2)
        edits = (response or {}).get("result") or []
        new_text = edits[0]["newText"] if len(edits) == 1 else ""
        self.check("formats a document larger than 256 KiB in full",
                   len(text) > 262144 and new_text.endswith("fn main() {\n\tf0 print\n}\n")
                   and new_text.count("fn f") == 12000, "got %d edits, %d bytes" % (len(edits), len(new_text)))

        response = self.answer([did_open(text="fn main( { oh no\n"),
                                self.request(2, "textDocument/formatting",
                                             {"textDocument": {"uri": DOC_URI},
                                              "options": {"tabSize": 4, "insertSpaces": False}})], 2)
        self.check("does not format a document that does not parse",
                   response is not None and response.get("result") == [], "got %r" % (response,))

    # -- positions, URIs and document sync ------------------------------
    def test_positions(self):
        print("\n-- positions --")
        uri = "file:///utf16.qd"
        text = 'fn helper( -- x:i64) { 1 }\nfn main() {\n  "åäö😀" helper print\n}\n'
        line = '  "åäö😀" helper print'
        col16 = len(line[:line.index("helper")].encode("utf-16-le")) // 2

        response = self.answer([did_open(uri=uri, text=text),
                                self.request(2, "textDocument/definition", self.at(uri, 2, col16 + 1))], 2)
        location = (response or {}).get("result") or {}
        self.check("a UTF-16 position after non-ASCII text resolves the word under it",
                   location.get("range", {}).get("start") == {"line": 0, "character": 3},
                   "got %r" % (response,))

        response = self.answer([did_open(uri=uri, text=text),
                                self.request(2, "textDocument/references",
                                             self.at(uri, 0, 4, context={"includeDeclaration": True}))], 2)
        self.check("reference ranges are reported in UTF-16 code units",
                   self.ranges(response) == [(0, 3, 9), (2, col16, col16 + 6)], "got %r" % (response,))

        response = self.answer([did_open(uri=uri, text=text),
                                self.request(2, "textDocument/semanticTokens/full",
                                             {"textDocument": {"uri": uri}})], 2)
        data = ((response or {}).get("result") or {}).get("data") or []
        tokens, line_no, col = [], 0, 0
        for i in range(0, len(data) - 4, 5):
            line_no += data[i]
            col = data[i + 1] if data[i] else col + data[i + 1]
            tokens.append((line_no, col, data[i + 2]))
        self.check("semantic tokens use UTF-16 columns and lengths",
                   (2, 2, 7) in tokens and (2, col16, 6) in tokens, "got %r" % (tokens,))

        utf8_init = json.loads(json.dumps(INIT))
        utf8_init["params"]["capabilities"] = {"general": {"positionEncodings": ["utf-8", "utf-16"]}}
        data = frame(utf8_init) + frame(did_open(uri=uri, text=text)) + frame(
            self.request(2, "textDocument/references", self.at(uri, 0, 4, context={"includeDeclaration": True})))
        _, responses = self.drive(data)
        init_reply = next((r for r in responses if r.get("id") == 1), {})
        refs = next((r for r in responses if r.get("id") == 2), None)
        col8 = len(line[:line.index("helper")].encode())
        self.check("a client offering utf-8 gets byte columns",
                   init_reply.get("result", {}).get("capabilities", {}).get("positionEncoding") == "utf-8"
                   and self.ranges(refs) == [(0, 3, 9), (2, col8, col8 + 6)], "got %r" % (refs,))

        change = {"jsonrpc": "2.0", "method": "textDocument/didChange",
                  "params": {"textDocument": {"uri": DOC_URI, "version": 2}, "contentChanges": [
                      {"text": "fn one() { }\nfn main() { one }\n"},
                      {"range": {"start": {"line": 0, "character": 3}, "end": {"line": 0, "character": 6}},
                       "text": "two"},
                      {"range": {"start": {"line": 1, "character": 12}, "end": {"line": 1, "character": 15}},
                       "text": "two"}]}}
        response = self.answer([did_open(), change, self.request(2, "textDocument/documentSymbol",
                                                                  {"textDocument": {"uri": DOC_URI}})], 2)
        names = [s.get("name") for s in (response or {}).get("result") or []]
        self.check("didChange applies every content change in order", "two" in names and "helper" not in names,
                   "symbols %r" % (names,))

        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "my dir"
            folder.mkdir()
            (folder / "lib.qd").write_text("fn helper( -- x:i64) { 1 }\n")
            main_text = "fn main() {\n  helper print\n}\n"
            (folder / "main.qd").write_text(main_text)
            main_uri = "file://" + urllib.parse.quote(str(folder / "main.qd"))
            response = self.answer([did_open(uri=main_uri, text=main_text),
                                    self.request(2, "textDocument/definition", self.at(main_uri, 1, 3))], 2)
            target = ((response or {}).get("result") or {}).get("uri")
            self.check("percent-encoded URIs resolve sibling files",
                       target == "file://" + urllib.parse.quote(str(folder / "lib.qd")), "got %r" % (response,))

    # -- rename, references, highlight, definition ----------------------
    def test_names(self):
        print("\n-- names --")
        uri = "file:///names.qd"

        def rename(text, line, character, new_name="zz"):
            return self.answer([did_open(uri=uri, text=text),
                                self.request(2, "textDocument/rename",
                                             dict(self.at(uri, line, character), newName=new_name))], 2)

        text = "fn a() { } fn bb() { a }\nfn main() { bb }\n"
        self.check("renaming a function declared second on a line edits its name",
                   self.ranges(rename(text, 1, 12)) == [(0, 14, 16), (1, 12, 14)])
        response = self.answer([did_open(uri=uri, text=text),
                                self.request(2, "textDocument/documentHighlight", self.at(uri, 1, 12))], 2)
        self.check("highlight finds a function declared second on a line",
                   self.ranges(response) == [(0, 14, 16), (1, 12, 14)], "got %r" % (response,))
        response = self.answer([did_open(uri=uri, text=text),
                                self.request(2, "textDocument/definition", self.at(uri, 1, 12))], 2)
        self.check("definition covers exactly the function name",
                   ((response or {}).get("result") or {}).get("range") ==
                   {"start": {"line": 0, "character": 14}, "end": {"line": 0, "character": 16}},
                   "got %r" % (response,))

        text = "fn main() {\n  1 -> ab 2 -> b\n  ab b + print\n}\n"
        self.check("renaming local b leaves ab alone",
                   self.ranges(rename(text, 2, 5)) == [(1, 15, 16), (2, 5, 6)])
        text = "fn main() {\n  1 ->ab\n  ab print\n}\n"
        self.check("renaming a local bound without a space after the arrow",
                   self.ranges(rename(text, 2, 2)) == [(1, 6, 8), (2, 2, 4)])
        text = "fn first() {\n  1 -> x x print\n}\nfn second() {\n  2 -> x x print\n}\n"
        self.check("renaming a local stays inside its own function",
                   self.ranges(rename(text, 4, 9)) == [(4, 7, 8), (4, 9, 10)])

        text = "fn f(count:i64 -- ) {\n  count print\n}\nfn main() { 1 f }\n"
        self.check("renaming a parameter renames its declaration",
                   self.ranges(rename(text, 1, 3)) == [(0, 5, 10), (1, 2, 7)])
        for include, expected in ((True, [(0, 5, 10), (1, 2, 7)]), (False, [(1, 2, 7)])):
            response = self.answer([did_open(uri=uri, text=text),
                                    self.request(2, "textDocument/references",
                                                 self.at(uri, 1, 3, context={"includeDeclaration": include}))], 2)
            self.check("parameter references honour includeDeclaration=%s" % include,
                       self.ranges(response) == expected, "got %r" % (response,))

        text = "fn helper() { }\nfn main() { helper }\n"
        for bad in ("not valid!", "1abc", "print", "fn", "i64"):
            response = rename(text, 1, 13, bad)
            self.check("rename to %r is rejected" % bad,
                       (response or {}).get("error", {}).get("code") == -32602, "got %r" % (response,))
        response = rename(text, 1, 13, "zz")
        self.check("rename to a valid name succeeds", self.ranges(response) == [(0, 3, 9), (1, 12, 18)],
                   "got %r" % (response,))
        response = self.answer([did_open(uri=uri, text="fn main() { 1 print }\n"),
                                self.request(2, "textDocument/rename",
                                             dict(self.at(uri, 0, 15), newName="zz"))], 2)
        self.check("renaming a built-in is refused", "error" in (response or {}), "got %r" % (response,))
        response = self.answer([did_open(uri=uri, text=text),
                                self.request(2, "textDocument/prepareRename", self.at(uri, 1, 14))], 2)
        self.check("prepareRename returns the exact name range",
                   ((response or {}).get("result") or {}).get("range") ==
                   {"start": {"line": 1, "character": 12}, "end": {"line": 1, "character": 18}},
                   "got %r" % (response,))

    def run(self):
        print("Quadrate LSP robustness tests\n" + "=" * 40)
        self.test_ids()
        self.test_framing()
        self.test_lifecycle()
        self.test_params()
        self.test_size()
        self.test_protocol()
        self.test_formatting()
        self.test_positions()
        self.test_names()
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
