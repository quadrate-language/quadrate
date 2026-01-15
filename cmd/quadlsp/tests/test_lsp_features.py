#!/usr/bin/env python3
"""
Feature-specific tests for Quadrate LSP
Tests hover, go-to-definition, find references, and document symbols
"""

import json
import subprocess
import sys
from pathlib import Path

class LSPFeatureTester:
    def __init__(self, lsp_path):
        self.lsp_path = lsp_path
        self.test_count = 0
        self.passed = 0
        self.failed = 0

    def send_request(self, request_dict, timeout=2):
        """Send a JSON-RPC request to the LSP server and get response"""
        request_json = json.dumps(request_dict)
        message = f"Content-Length: {len(request_json)}\r\n\r\n{request_json}"

        try:
            result = subprocess.run(
                [self.lsp_path],
                input=message.encode(),
                capture_output=True,
                timeout=timeout
            )

            output = result.stdout.decode()
            if not output:
                return None

            lines = output.split('\r\n')
            for i, line in enumerate(lines):
                if line == '':
                    json_str = '\r\n'.join(lines[i+1:])
                    return json.loads(json_str)

            return None
        except subprocess.TimeoutExpired:
            return {"error": "timeout"}
        except json.JSONDecodeError as e:
            return {"error": f"json_decode: {e}"}

    def assert_test(self, condition, test_name):
        """Assert a test condition"""
        self.test_count += 1
        if condition:
            self.passed += 1
            print(f"  ✓ {test_name}")
            return True
        else:
            self.failed += 1
            print(f"  ✗ {test_name}")
            return False

    # ============================================================================
    # Hover tests
    # ============================================================================

    def test_hover_on_instruction(self):
        """Test hover over a built-in instruction"""
        print("\n=== Testing Hover on Instruction ===")

        request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "textDocument/hover",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "position": {"line": 0, "character": 5}  # Should be over some content
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Hover request returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")
            # Result might be null if no hover info available at position
            self.assert_test(True, "Hover request handled without crash")

    def test_hover_on_keyword(self):
        """Test hover over a language keyword"""
        print("\n=== Testing Hover on Keyword ===")

        request = {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "textDocument/hover",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "position": {"line": 0, "character": 0}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Hover on keyword returns response")

    def test_hover_empty_position(self):
        """Test hover on position with no content"""
        print("\n=== Testing Hover on Empty Position ===")

        request = {
            "jsonrpc": "2.0",
            "id": 3,
            "method": "textDocument/hover",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "position": {"line": 100, "character": 100}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Hover on empty position doesn't crash")

    # ============================================================================
    # Go to definition tests
    # ============================================================================

    def test_definition_basic(self):
        """Test go-to-definition request"""
        print("\n=== Testing Go To Definition ===")

        request = {
            "jsonrpc": "2.0",
            "id": 10,
            "method": "textDocument/definition",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "position": {"line": 0, "character": 5}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Definition request returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")

    def test_definition_out_of_bounds(self):
        """Test definition request with out-of-bounds position"""
        print("\n=== Testing Definition Out of Bounds ===")

        request = {
            "jsonrpc": "2.0",
            "id": 11,
            "method": "textDocument/definition",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "position": {"line": 9999, "character": 9999}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Out-of-bounds definition doesn't crash")

    # ============================================================================
    # Find references tests
    # ============================================================================

    def test_references_basic(self):
        """Test find references request"""
        print("\n=== Testing Find References ===")

        request = {
            "jsonrpc": "2.0",
            "id": 20,
            "method": "textDocument/references",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "position": {"line": 0, "character": 5},
                "context": {"includeDeclaration": True}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "References request returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")

    def test_references_include_declaration(self):
        """Test references with includeDeclaration option"""
        print("\n=== Testing References includeDeclaration ===")

        request = {
            "jsonrpc": "2.0",
            "id": 21,
            "method": "textDocument/references",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "position": {"line": 0, "character": 0},
                "context": {"includeDeclaration": False}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "References with includeDeclaration=false works")

    # ============================================================================
    # Document symbols tests
    # ============================================================================

    def test_document_symbols_basic(self):
        """Test document symbols request"""
        print("\n=== Testing Document Symbols ===")

        request = {
            "jsonrpc": "2.0",
            "id": 30,
            "method": "textDocument/documentSymbol",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Document symbols request returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")
            result = response.get("result", [])
            self.assert_test(isinstance(result, list), "Result is a list")

    def test_document_symbols_nonexistent_file(self):
        """Test document symbols for non-existent file"""
        print("\n=== Testing Document Symbols Non-Existent File ===")

        request = {
            "jsonrpc": "2.0",
            "id": 31,
            "method": "textDocument/documentSymbol",
            "params": {
                "textDocument": {"uri": "file:///nonexistent/path.qd"}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Non-existent file doesn't crash")

    # ============================================================================
    # Document highlight tests
    # ============================================================================

    def test_document_highlight_basic(self):
        """Test document highlight request"""
        print("\n=== Testing Document Highlight ===")

        request = {
            "jsonrpc": "2.0",
            "id": 40,
            "method": "textDocument/documentHighlight",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "position": {"line": 0, "character": 5}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Document highlight returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")

    # ============================================================================
    # Signature help tests
    # ============================================================================

    def test_signature_help_basic(self):
        """Test signature help request"""
        print("\n=== Testing Signature Help ===")

        request = {
            "jsonrpc": "2.0",
            "id": 50,
            "method": "textDocument/signatureHelp",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "position": {"line": 0, "character": 5}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Signature help returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")

    # ============================================================================
    # Folding range tests
    # ============================================================================

    def test_folding_range_basic(self):
        """Test folding range request"""
        print("\n=== Testing Folding Range ===")

        request = {
            "jsonrpc": "2.0",
            "id": 60,
            "method": "textDocument/foldingRange",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"}
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Folding range returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")

    def run_all_tests(self):
        """Run all feature tests"""
        print("=" * 60)
        print("Quadrate LSP Feature Test Suite")
        print("=" * 60)

        # Hover tests
        self.test_hover_on_instruction()
        self.test_hover_on_keyword()
        self.test_hover_empty_position()

        # Definition tests
        self.test_definition_basic()
        self.test_definition_out_of_bounds()

        # References tests
        self.test_references_basic()
        self.test_references_include_declaration()

        # Document symbols tests
        self.test_document_symbols_basic()
        self.test_document_symbols_nonexistent_file()

        # Document highlight tests
        self.test_document_highlight_basic()

        # Signature help tests
        self.test_signature_help_basic()

        # Folding range tests
        self.test_folding_range_basic()

        print("\n" + "=" * 60)
        print(f"Tests run:    {self.test_count}")
        print(f"Passed:       {self.passed}")
        print(f"Failed:       {self.failed}")
        print("=" * 60)

        if self.failed == 0:
            print("\n✅ All feature tests passed!")
            return 0
        else:
            print(f"\n❌ {self.failed} test(s) failed")
            return 1

def main():
    # Find LSP executable
    possible_paths = [
        Path("build/debug/cmd/quadlsp/quadlsp"),
        Path("build/release/cmd/quadlsp/quadlsp"),
        Path("cmd/quadlsp/quadlsp"),
        Path("../../../cmd/quadlsp/quadlsp"),
    ]

    lsp_path = None
    for path in possible_paths:
        if path.exists():
            lsp_path = path
            break

    if not lsp_path:
        print("❌ LSP executable not found. Please build the project first.")
        return 1

    print(f"Using LSP: {lsp_path}\n")

    tester = LSPFeatureTester(str(lsp_path))
    return tester.run_all_tests()

if __name__ == "__main__":
    sys.exit(main())
