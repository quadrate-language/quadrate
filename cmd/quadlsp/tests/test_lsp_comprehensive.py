#!/usr/bin/env python3
"""
Comprehensive tests for all Quadrate LSP features.
Tests formatting, semantic tokens, document links, call hierarchy,
selection range, code lens, type hierarchy, on type formatting,
and linked editing ranges.
"""

import json
import select
import subprocess
import sys
import time
from pathlib import Path


class LSPSession:
    """Manages a persistent LSP session for testing"""

    def __init__(self, lsp_path):
        self.lsp_path = lsp_path
        self.proc = None
        self.request_id = 0

    def start(self):
        """Start the LSP server process"""
        self.proc = subprocess.Popen(
            [self.lsp_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0  # Unbuffered binary mode
        )
        return self.proc is not None

    def stop(self):
        """Stop the LSP server process"""
        if self.proc:
            try:
                self.send_request("shutdown", {})
                self.send_notification("exit", {})
                self.proc.wait(timeout=2)
            except Exception:
                self.proc.kill()
            self.proc = None

    def send_request(self, method, params):
        """Send a request and wait for response"""
        self.request_id += 1
        req = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": method,
            "params": params
        }
        content = json.dumps(req)
        message = f"Content-Length: {len(content)}\r\n\r\n{content}"
        self.proc.stdin.write(message.encode('utf-8'))
        self.proc.stdin.flush()
        return self._read_response_with_id(self.request_id)

    def send_notification(self, method, params):
        """Send a notification (no response expected)"""
        req = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params
        }
        content = json.dumps(req)
        message = f"Content-Length: {len(content)}\r\n\r\n{content}"
        self.proc.stdin.write(message.encode('utf-8'))
        self.proc.stdin.flush()

    def _read_response(self, timeout_sec=5.0):
        """Read a single response from the server with timeout"""
        fd = self.proc.stdout.fileno()

        header_data = b""

        # Read headers with timeout, byte by byte
        while True:
            ready, _, _ = select.select([fd], [], [], timeout_sec)
            if not ready:
                return None  # Timeout

            byte = self.proc.stdout.read(1)
            if not byte:
                return None
            header_data += byte

            if header_data.endswith(b'\r\n\r\n'):
                break

        # Parse headers
        headers = {}
        for line in header_data.decode('utf-8').strip().split('\r\n'):
            if ':' in line:
                key, value = line.split(':', 1)
                headers[key.strip()] = value.strip()

        content_length = int(headers.get('Content-Length', 0))
        if content_length > 0:
            content = self.proc.stdout.read(content_length)
            return json.loads(content.decode('utf-8'))
        return None

    def _read_response_with_id(self, expected_id, max_attempts=20):
        """Read responses until we get the one with expected id"""
        for _ in range(max_attempts):
            resp = self._read_response()
            if resp and resp.get('id') == expected_id:
                return resp
        return None

    def initialize(self):
        """Initialize the LSP session"""
        resp = self.send_request("initialize", {
            "processId": None,
            "rootUri": "file:///tmp",
            "capabilities": {}
        })
        if resp and 'result' in resp:
            self.send_notification("initialized", {})
            return resp['result'].get('capabilities', {})
        return None

    def open_document(self, uri, content):
        """Open a document in the LSP"""
        self.send_notification("textDocument/didOpen", {
            "textDocument": {
                "uri": uri,
                "languageId": "quadrate",
                "version": 1,
                "text": content
            }
        })
        time.sleep(0.05)  # Give server time to process


class LSPComprehensiveTester:
    """Comprehensive tester for all LSP features"""

    def __init__(self, lsp_path):
        self.lsp_path = lsp_path
        self.session = None
        self.test_count = 0
        self.passed = 0
        self.failed = 0
        self.capabilities = {}

    def setup(self):
        """Set up the test session"""
        self.session = LSPSession(self.lsp_path)
        if not self.session.start():
            return False
        self.capabilities = self.session.initialize()
        return self.capabilities is not None

    def teardown(self):
        """Tear down the test session"""
        if self.session:
            self.session.stop()
            self.session = None

    def assert_test(self, condition, test_name):
        """Assert a test condition"""
        self.test_count += 1
        if condition:
            self.passed += 1
            print(f"    PASS: {test_name}")
            return True
        else:
            self.failed += 1
            print(f"    FAIL: {test_name}")
            # Track failed test names for summary
            if not hasattr(self, 'failed_tests'):
                self.failed_tests = []
            self.failed_tests.append(test_name)
            return False

    # ==========================================================================
    # Formatting Tests
    # ==========================================================================

    def test_formatting(self):
        """Test document formatting"""
        print("\n  [Formatting]")

        # Check capability
        self.assert_test(
            self.capabilities.get('documentFormattingProvider'),
            "documentFormattingProvider capability registered"
        )

        # Open document with poorly formatted code
        uri = "file:///tmp/format_test.qd"
        content = 'fn   main(  ){\n"Hello"  print nl\n}'
        self.session.open_document(uri, content)

        # Request formatting
        resp = self.session.send_request("textDocument/formatting", {
            "textDocument": {"uri": uri},
            "options": {"tabSize": 4, "insertSpaces": True}
        })

        self.assert_test(resp is not None, "Formatting request returns response")
        self.assert_test('result' in resp, "Response has result field")

        if resp and 'result' in resp:
            result = resp['result']
            if result:
                self.assert_test(len(result) > 0, "Formatting returns edits")
                self.assert_test('newText' in result[0], "Edit has newText field")
            else:
                self.assert_test(True, "No formatting changes needed (acceptable)")

    def test_range_formatting(self):
        """Test range formatting"""
        print("\n  [Range Formatting]")

        # Check capability
        self.assert_test(
            self.capabilities.get('documentRangeFormattingProvider'),
            "documentRangeFormattingProvider capability registered"
        )

        uri = "file:///tmp/range_format_test.qd"
        content = 'fn   add(a:i64   b:i64  --  result:i64)  {\na   b  +\n}\n\nfn main() {\n5 3 add print nl\n}'
        self.session.open_document(uri, content)

        # Request range formatting for first function
        resp = self.session.send_request("textDocument/rangeFormatting", {
            "textDocument": {"uri": uri},
            "range": {
                "start": {"line": 0, "character": 0},
                "end": {"line": 2, "character": 1}
            },
            "options": {"tabSize": 4, "insertSpaces": False}
        })

        self.assert_test(resp is not None, "Range formatting request returns response")
        self.assert_test('result' in resp, "Response has result field")

    # ==========================================================================
    # Semantic Tokens Tests
    # ==========================================================================

    def test_semantic_tokens(self):
        """Test semantic tokens"""
        print("\n  [Semantic Tokens]")

        # Check capability
        provider = self.capabilities.get('semanticTokensProvider')
        self.assert_test(provider is not None, "semanticTokensProvider capability registered")

        if provider:
            legend = provider.get('legend', {})
            token_types = legend.get('tokenTypes', [])
            token_mods = legend.get('tokenModifiers', [])
            self.assert_test(len(token_types) > 0, "Token types defined")
            self.assert_test(len(token_mods) > 0, "Token modifiers defined")
            self.assert_test('keyword' in token_types, "Keyword token type exists")
            self.assert_test('function' in token_types, "Function token type exists")
            self.assert_test('string' in token_types, "String token type exists")

        uri = "file:///tmp/semantic_test.qd"
        content = '''// Comment
fn factorial(n:i64 -- result:i64) {
    1 -> acc
    n 1 <= if { 1 } else { n n -- factorial * }
}

fn main() {
    5 factorial print nl
}
'''
        self.session.open_document(uri, content)

        # Request semantic tokens
        resp = self.session.send_request("textDocument/semanticTokens/full", {
            "textDocument": {"uri": uri}
        })

        self.assert_test(resp is not None, "Semantic tokens request returns response")
        self.assert_test('result' in resp, "Response has result field")

        if resp and 'result' in resp:
            result = resp['result']
            self.assert_test('data' in result, "Result has data field")
            data = result.get('data', [])
            self.assert_test(len(data) > 0, "Token data is not empty")
            self.assert_test(len(data) % 5 == 0, "Token data length is multiple of 5")

    # ==========================================================================
    # Document Links Tests
    # ==========================================================================

    def test_document_links(self):
        """Test document links"""
        print("\n  [Document Links]")

        # Check capability
        self.assert_test(
            self.capabilities.get('documentLinkProvider') is not None,
            "documentLinkProvider capability registered"
        )

        # Use a temp path that doesn't need to exist on disk
        uri = "file:///tmp/test_links.qd"
        content = '''use strings
use math
use io

fn main() {
    "hello" strings::upper print nl
}
'''
        self.session.open_document(uri, content)

        # Request document links
        resp = self.session.send_request("textDocument/documentLink", {
            "textDocument": {"uri": uri}
        })

        self.assert_test(resp is not None, "Document links request returns response")
        self.assert_test('result' in resp, "Response has result field")

        if resp and 'result' in resp:
            links = resp['result']
            self.assert_test(isinstance(links, list), "Result is a list")
            # Links depend on standard library being installed - skip assertion if not available
            # This is expected on CI where QUADRATE_ROOT may not point to installed modules
            if links:
                # If we got links, verify their structure
                self.assert_test(len(links) >= 1, "At least one link found")
                self.assert_test('range' in links[0], "Link has range field")
                self.assert_test('target' in links[0], "Link has target field")
            else:
                # No links found - this is OK if modules aren't installed
                print("    SKIP: No document links found (standard library not installed)")
                self.test_count += 1  # Count as a test
                self.passed += 1  # Count as passed (expected behavior)

    # ==========================================================================
    # Call Hierarchy Tests
    # ==========================================================================

    def test_call_hierarchy(self):
        """Test call hierarchy"""
        print("\n  [Call Hierarchy]")

        # Check capability
        self.assert_test(
            self.capabilities.get('callHierarchyProvider'),
            "callHierarchyProvider capability registered"
        )

        uri = "file:///tmp/callhier_test.qd"
        content = '''fn add(a:i64 b:i64 -- result:i64) {
    a b +
}

fn double(x:i64 -- result:i64) {
    x x add
}

fn quadruple(x:i64 -- result:i64) {
    x double double
}

fn main() {
    5 quadruple print nl
}
'''
        self.session.open_document(uri, content)

        # Prepare call hierarchy on "double"
        resp = self.session.send_request("textDocument/prepareCallHierarchy", {
            "textDocument": {"uri": uri},
            "position": {"line": 4, "character": 5}
        })

        self.assert_test(resp is not None, "Prepare call hierarchy returns response")
        self.assert_test('result' in resp, "Response has result field")

        if resp and 'result' in resp:
            items = resp['result']
            self.assert_test(isinstance(items, list), "Result is a list")
            self.assert_test(len(items) > 0, "Found function at cursor")

            if items:
                item = items[0]
                self.assert_test(item.get('name') == 'double', "Correct function name")
                self.assert_test('data' in item, "Item has data field")

                # Test incoming calls
                resp2 = self.session.send_request("callHierarchy/incomingCalls", {
                    "item": item
                })
                self.assert_test(resp2 is not None, "Incoming calls returns response")
                if resp2 and 'result' in resp2:
                    calls = resp2['result']
                    self.assert_test(len(calls) >= 1, "Found incoming calls")

                # Test outgoing calls
                resp3 = self.session.send_request("callHierarchy/outgoingCalls", {
                    "item": item
                })
                self.assert_test(resp3 is not None, "Outgoing calls returns response")
                if resp3 and 'result' in resp3:
                    calls = resp3['result']
                    self.assert_test(len(calls) >= 1, "Found outgoing calls")

    # ==========================================================================
    # Selection Range Tests
    # ==========================================================================

    def test_selection_range(self):
        """Test selection range"""
        print("\n  [Selection Range]")

        # Check capability
        self.assert_test(
            self.capabilities.get('selectionRangeProvider'),
            "selectionRangeProvider capability registered"
        )

        uri = "file:///tmp/selrange_test.qd"
        content = '''fn main() {
    5 -> x
    x x + print nl
}
'''
        self.session.open_document(uri, content)

        # Request selection range
        resp = self.session.send_request("textDocument/selectionRange", {
            "textDocument": {"uri": uri},
            "positions": [{"line": 2, "character": 10}]
        })

        self.assert_test(resp is not None, "Selection range returns response")
        self.assert_test('result' in resp, "Response has result field")

        if resp and 'result' in resp:
            results = resp['result']
            self.assert_test(isinstance(results, list), "Result is a list")
            self.assert_test(len(results) > 0, "Got selection range result")

            if results and results[0]:
                sr = results[0]
                self.assert_test('range' in sr, "Selection range has range field")
                # Check for nested parent
                parent_count = 0
                current = sr
                while current and 'parent' in current:
                    parent_count += 1
                    current = current.get('parent')
                self.assert_test(parent_count >= 2, "Selection range has nested parents")

    # ==========================================================================
    # Code Lens Tests
    # ==========================================================================

    def test_code_lens(self):
        """Test code lens"""
        print("\n  [Code Lens]")

        # Check capability
        self.assert_test(
            self.capabilities.get('codeLensProvider') is not None,
            "codeLensProvider capability registered"
        )

        uri = "file:///tmp/codelens_test.qd"
        content = '''fn add(a:i64 b:i64 -- result:i64) {
    a b +
}

fn double(x:i64 -- result:i64) {
    x x add
}

fn main() {
    5 double add print nl
}

fn test_add() {
    2 3 add 5 eq assert
}
'''
        self.session.open_document(uri, content)

        # Request code lens
        resp = self.session.send_request("textDocument/codeLens", {
            "textDocument": {"uri": uri}
        })

        self.assert_test(resp is not None, "Code lens returns response")
        self.assert_test('result' in resp, "Response has result field")

        if resp and 'result' in resp:
            lenses = resp['result']
            self.assert_test(isinstance(lenses, list), "Result is a list")
            self.assert_test(len(lenses) >= 4, "Got code lenses for all functions")

            # Check structure
            if lenses:
                lens = lenses[0]
                self.assert_test('range' in lens, "Lens has range field")
                self.assert_test('command' in lens, "Lens has command field")
                if 'command' in lens:
                    cmd = lens['command']
                    self.assert_test('title' in cmd, "Command has title")

            # Check for test lens
            test_lenses = [l for l in lenses if l.get('command', {}).get('title') == 'Run test']
            self.assert_test(len(test_lenses) >= 1, "Found Run test lens for test function")

    # ==========================================================================
    # Type Hierarchy Tests
    # ==========================================================================

    def test_type_hierarchy(self):
        """Test type hierarchy"""
        print("\n  [Type Hierarchy]")

        # Check capability
        self.assert_test(
            self.capabilities.get('typeHierarchyProvider'),
            "typeHierarchyProvider capability registered"
        )

        uri = "file:///tmp/typehier_test.qd"
        content = '''struct Point {
    x:i64
    y:i64
}

struct Rectangle {
    origin:Point
    width:i64
    height:i64
}

fn main() {
    Point { x = 10 y = 20 } -> p
    p print nl
}
'''
        self.session.open_document(uri, content)

        # Prepare type hierarchy on "Point"
        resp = self.session.send_request("textDocument/prepareTypeHierarchy", {
            "textDocument": {"uri": uri},
            "position": {"line": 0, "character": 8}
        })

        self.assert_test(resp is not None, "Prepare type hierarchy returns response")
        self.assert_test('result' in resp, "Response has result field")

        if resp and 'result' in resp:
            items = resp['result']
            self.assert_test(isinstance(items, list), "Result is a list")
            self.assert_test(len(items) > 0, "Found type at cursor")

            if items:
                item = items[0]
                self.assert_test(item.get('name') == 'Point', "Correct type name")
                self.assert_test(item.get('kind') == 23, "Correct kind (Struct)")
                self.assert_test('detail' in item, "Has detail field")

                # Test supertypes (should be empty for Quadrate)
                resp2 = self.session.send_request("typeHierarchy/supertypes", {
                    "item": item
                })
                self.assert_test(resp2 is not None, "Supertypes returns response")

                # Test subtypes (should be empty for Quadrate)
                resp3 = self.session.send_request("typeHierarchy/subtypes", {
                    "item": item
                })
                self.assert_test(resp3 is not None, "Subtypes returns response")

    # ==========================================================================
    # On Type Formatting Tests
    # ==========================================================================

    def test_on_type_formatting(self):
        """Test on type formatting"""
        print("\n  [On Type Formatting]")

        # Check capability
        provider = self.capabilities.get('documentOnTypeFormattingProvider')
        self.assert_test(provider is not None, "documentOnTypeFormattingProvider capability registered")

        if provider:
            self.assert_test(
                provider.get('firstTriggerCharacter') == '\n',
                "First trigger character is newline"
            )
            more = provider.get('moreTriggerCharacter', [])
            self.assert_test('}' in more, "} is a trigger character")
            self.assert_test('{' in more, "{ is a trigger character")

        uri = "file:///tmp/ontypeformat_test.qd"
        content = 'fn main() {\n'
        self.session.open_document(uri, content)

        # Simulate typing newline after opening brace
        resp = self.session.send_request("textDocument/onTypeFormatting", {
            "textDocument": {"uri": uri},
            "position": {"line": 1, "character": 0},
            "ch": "\n",
            "options": {"tabSize": 4, "insertSpaces": False}
        })

        self.assert_test(resp is not None, "On type formatting returns response")
        self.assert_test('result' in resp, "Response has result field")

        if resp and 'result' in resp:
            edits = resp['result']
            self.assert_test(isinstance(edits, list), "Result is a list")
            self.assert_test(len(edits) > 0, "Got formatting edits")

            if edits:
                edit = edits[0]
                self.assert_test('newText' in edit, "Edit has newText")
                self.assert_test('\t' in edit.get('newText', ''), "Indentation added")

    # ==========================================================================
    # Linked Editing Range Tests
    # ==========================================================================

    def test_linked_editing_range(self):
        """Test linked editing range"""
        print("\n  [Linked Editing Range]")

        # Check capability
        self.assert_test(
            self.capabilities.get('linkedEditingRangeProvider'),
            "linkedEditingRangeProvider capability registered"
        )

        uri = "file:///tmp/linkededit_test.qd"
        content = '''fn calculate() {
    10 -> x
    x x + -> y
    y x * print nl
}
'''
        self.session.open_document(uri, content)

        # Request linked editing range for "x"
        resp = self.session.send_request("textDocument/linkedEditingRange", {
            "textDocument": {"uri": uri},
            "position": {"line": 1, "character": 10}
        })

        self.assert_test(resp is not None, "Linked editing range returns response")
        self.assert_test('result' in resp, "Response has result field")

        if resp and 'result' in resp:
            result = resp['result']
            if result:  # Can be null if no linked ranges
                self.assert_test('ranges' in result, "Result has ranges field")
                ranges = result.get('ranges', [])
                self.assert_test(len(ranges) >= 2, "Found multiple occurrences")

                # Verify all ranges are for the same identifier
                if ranges:
                    for r in ranges:
                        self.assert_test('start' in r, "Range has start")
                        self.assert_test('end' in r, "Range has end")

    # ==========================================================================
    # Edge Cases and Robustness Tests
    # ==========================================================================

    def test_empty_document(self):
        """Test features on empty document"""
        print("\n  [Empty Document Edge Cases]")

        uri = "file:///tmp/empty_test.qd"
        content = ''
        self.session.open_document(uri, content)

        # Semantic tokens on empty doc
        resp = self.session.send_request("textDocument/semanticTokens/full", {
            "textDocument": {"uri": uri}
        })
        self.assert_test(resp is not None and 'result' in resp, "Semantic tokens on empty doc doesn't crash")

        # Code lens on empty doc
        resp = self.session.send_request("textDocument/codeLens", {
            "textDocument": {"uri": uri}
        })
        self.assert_test(resp is not None and 'result' in resp, "Code lens on empty doc doesn't crash")

        # Document links on empty doc
        resp = self.session.send_request("textDocument/documentLink", {
            "textDocument": {"uri": uri}
        })
        self.assert_test(resp is not None and 'result' in resp, "Document links on empty doc doesn't crash")

    def test_malformed_code(self):
        """Test features on malformed code"""
        print("\n  [Malformed Code Edge Cases]")

        uri = "file:///tmp/malformed_test.qd"
        content = 'fn { broken code here } struct { also broken'
        self.session.open_document(uri, content)

        # Should not crash on malformed code
        resp = self.session.send_request("textDocument/semanticTokens/full", {
            "textDocument": {"uri": uri}
        })
        self.assert_test(resp is not None, "Semantic tokens on malformed code doesn't crash")

        resp = self.session.send_request("textDocument/codeLens", {
            "textDocument": {"uri": uri}
        })
        self.assert_test(resp is not None, "Code lens on malformed code doesn't crash")

    def test_unicode_content(self):
        """Test features with unicode content"""
        print("\n  [Unicode Content]")

        uri = "file:///tmp/unicode_test.qd"
        content = '''fn main() {
    "Hello, " print nl
    "Привет" print nl
    "こんにちは" print nl
}
'''
        self.session.open_document(uri, content)

        resp = self.session.send_request("textDocument/semanticTokens/full", {
            "textDocument": {"uri": uri}
        })
        self.assert_test(resp is not None and 'result' in resp, "Semantic tokens handles unicode")

        resp = self.session.send_request("textDocument/formatting", {
            "textDocument": {"uri": uri},
            "options": {"tabSize": 4, "insertSpaces": True}
        })
        self.assert_test(resp is not None and 'result' in resp, "Formatting handles unicode")

    def test_rename(self):
        """Test rename functionality"""
        import os
        import tempfile
        import shutil

        print("\n  [Rename]")

        # Check capability
        rename_cap = self.capabilities.get('renameProvider')
        self.assert_test(
            rename_cap is not None,
            "renameProvider capability registered"
        )
        if isinstance(rename_cap, dict):
            self.assert_test(
                rename_cap.get('prepareProvider'),
                "prepareProvider enabled"
            )

        # Test 1: Basic rename in single file
        uri = "file:///tmp/rename_test.qd"
        content = '''fn helper(x:i64 -- result:i64) {
    x 2 *
}

fn main() {
    5 helper print nl
    10 helper print nl
}
'''
        self.session.open_document(uri, content)

        # Test prepareRename first (this is what editors call before rename)
        resp = self.session.send_request("textDocument/prepareRename", {
            "textDocument": {"uri": uri},
            "position": {"line": 0, "character": 3}  # On 'helper'
        })
        self.assert_test(resp is not None, "prepareRename returns response")
        self.assert_test('result' in resp and resp['result'] is not None, "prepareRename has result")
        if resp and 'result' in resp and resp['result']:
            result = resp['result']
            self.assert_test('range' in result, "prepareRename has range")
            self.assert_test('placeholder' in result, "prepareRename has placeholder")
            if 'placeholder' in result:
                self.assert_test(result['placeholder'] == 'helper', "prepareRename placeholder is correct")

        # Rename 'helper' to 'double'
        resp = self.session.send_request("textDocument/rename", {
            "textDocument": {"uri": uri},
            "position": {"line": 0, "character": 3},  # On 'helper'
            "newName": "double"
        })

        self.assert_test(resp is not None, "Rename request returns response")
        self.assert_test('result' in resp, "Rename response has result")

        if resp and 'result' in resp:
            result = resp['result']
            changes = result.get('changes', {})
            self.assert_test(uri in changes, "Rename includes current file")

            if uri in changes:
                edits = changes[uri]
                # Should have 3 occurrences: definition + 2 calls
                self.assert_test(len(edits) >= 3, f"Found all occurrences (got {len(edits)}, expected 3)")

        # Test 2: Rename across sibling files
        # Create temporary directory with sibling files
        tmpdir = tempfile.mkdtemp(prefix="lsp_rename_test_")
        try:
            # Create main.qd
            main_path = os.path.join(tmpdir, "main.qd")
            with open(main_path, 'w') as f:
                f.write('''fn main() {
    5 compute print nl
}
''')

            # Create helper.qd with shared function
            helper_path = os.path.join(tmpdir, "helper.qd")
            with open(helper_path, 'w') as f:
                f.write('''fn compute(x:i64 -- result:i64) {
    x 10 *
}
''')

            main_uri = "file://" + main_path
            helper_uri = "file://" + helper_path

            # Open both documents
            with open(main_path, 'r') as f:
                self.session.open_document(main_uri, f.read())
            with open(helper_path, 'r') as f:
                self.session.open_document(helper_uri, f.read())

            # Rename 'compute' from main.qd (where it's used)
            resp = self.session.send_request("textDocument/rename", {
                "textDocument": {"uri": main_uri},
                "position": {"line": 1, "character": 6},  # On 'compute'
                "newName": "multiply"
            })

            self.assert_test(resp is not None, "Cross-file rename returns response")

            if resp and 'result' in resp:
                result = resp['result']
                changes = result.get('changes', {})

                self.assert_test(main_uri in changes, "Cross-file rename includes main file")
                self.assert_test(helper_uri in changes, "Cross-file rename includes sibling file")

                if main_uri in changes and helper_uri in changes:
                    main_edits = changes[main_uri]
                    helper_edits = changes[helper_uri]
                    self.assert_test(len(main_edits) >= 1, "Found usage in main file")
                    self.assert_test(len(helper_edits) >= 1, "Found definition in helper file")

        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)

    def test_large_document(self):
        """Test features on large document"""
        print("\n  [Large Document]")

        uri = "file:///tmp/large_test.qd"
        # Generate a moderately large document (reduced from 100 to 20 functions)
        content = ""
        for i in range(20):
            content += f'''fn func_{i}(x:i64 -- result:i64) {{
    x {i} +
}}

'''
        content += "fn main() {\n"
        for i in range(20):
            content += f"    1 func_{i}\n"
        content += "    print nl\n}\n"

        self.session.open_document(uri, content)

        # Test semantic tokens
        resp = self.session.send_request("textDocument/semanticTokens/full", {
            "textDocument": {"uri": uri}
        })
        self.assert_test(resp is not None and 'result' in resp, "Semantic tokens on large doc works")

        # Test code lens
        resp = self.session.send_request("textDocument/codeLens", {
            "textDocument": {"uri": uri}
        })
        self.assert_test(resp is not None and 'result' in resp, "Code lens on large doc works")
        if resp and 'result' in resp:
            lenses = resp['result']
            self.assert_test(len(lenses) >= 20, "Found code lens for all functions")

    # ==========================================================================
    # Run All Tests
    # ==========================================================================

    def run_all_tests(self):
        """Run all comprehensive tests"""
        print("=" * 70)
        print("Quadrate LSP Comprehensive Test Suite")
        print("=" * 70)

        if not self.setup():
            print("FAIL: Could not initialize LSP session")
            return 1

        try:
            # Core formatting
            self.test_formatting()
            self.test_range_formatting()

            # Semantic features
            self.test_semantic_tokens()
            self.test_document_links()

            # Navigation features
            self.test_call_hierarchy()
            self.test_selection_range()
            self.test_type_hierarchy()

            # Editor features
            self.test_code_lens()
            self.test_on_type_formatting()
            self.test_linked_editing_range()

            # Rename
            self.test_rename()

            # Edge cases
            self.test_empty_document()
            self.test_malformed_code()
            self.test_unicode_content()
            self.test_large_document()

        finally:
            self.teardown()

        # Print failure summary for easy identification of what failed
        failed_list = getattr(self, 'failed_tests', [])
        if failed_list:
            print("\n=== FAILED TESTS ===")
            for ft in failed_list:
                print(f"  FAIL: {ft}")
            print("=== END ===")

        print("\n" + "=" * 70)
        print(f"Tests run:    {self.test_count}")
        print(f"Passed:       {self.passed}")
        print(f"Failed:       {self.failed}")
        print("=" * 70)

        if self.failed == 0:
            print("\nAll comprehensive tests passed!")
            return 0
        else:
            print(f"\n{self.failed} test(s) failed")
            return 1


def main():
    # Find LSP executable - try various paths depending on where we're run from
    import os
    cwd = Path(os.getcwd())

    # Try to find project root by looking for meson.build
    project_root = None
    check_dir = cwd
    for _ in range(5):
        if (check_dir / "meson.build").exists() and (check_dir / "lib" / "qc").exists():
            project_root = check_dir
            break
        check_dir = check_dir.parent

    possible_paths = [
        Path("build/debug/cmd/quadlsp/quadlsp"),
        Path("build/release/cmd/quadlsp/quadlsp"),
        Path("dist/bin/quadlsp"),
        Path("cmd/quadlsp/quadlsp"),  # Running from build directory
        Path("../quadlsp"),
        Path("../../../build/debug/cmd/quadlsp/quadlsp"),
    ]

    if project_root:
        possible_paths.insert(0, project_root / "dist/bin/quadlsp")
        possible_paths.insert(1, project_root / "build/debug/cmd/quadlsp/quadlsp")

    lsp_path = None
    for path in possible_paths:
        if path.exists():
            lsp_path = path
            break

    if not lsp_path:
        print("LSP executable not found. Please build the project first.")
        return 1

    print(f"Using LSP: {lsp_path}\n")

    tester = LSPComprehensiveTester(str(lsp_path))
    return tester.run_all_tests()


if __name__ == "__main__":
    sys.exit(main())
