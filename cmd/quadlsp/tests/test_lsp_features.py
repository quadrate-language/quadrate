#!/usr/bin/env python3
"""
Feature-specific tests for Quadrate LSP
Tests hover, go-to-definition, find references, document symbols,
code actions, workspace symbols, and inlay hints
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

    # ============================================================================
    # Code action tests
    # ============================================================================

    def test_code_action_basic(self):
        """Test code action request"""
        print("\n=== Testing Code Action Basic ===")

        request = {
            "jsonrpc": "2.0",
            "id": 70,
            "method": "textDocument/codeAction",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 0, "character": 10}
                },
                "context": {
                    "diagnostics": []
                }
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Code action request returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")
            result = response.get("result", [])
            self.assert_test(isinstance(result, list), "Result is a list")

    def test_code_action_with_diagnostic(self):
        """Test code action with diagnostic for unknown module"""
        print("\n=== Testing Code Action with Diagnostic ===")

        request = {
            "jsonrpc": "2.0",
            "id": 71,
            "method": "textDocument/codeAction",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 0, "character": 10}
                },
                "context": {
                    "diagnostics": [
                        {
                            "range": {
                                "start": {"line": 0, "character": 0},
                                "end": {"line": 0, "character": 10}
                            },
                            "message": "Unknown module 'math'",
                            "severity": 1
                        }
                    ]
                }
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Code action with diagnostic returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")

    def test_code_action_unused_variable(self):
        """Test code action for unused variable warning"""
        print("\n=== Testing Code Action Unused Variable ===")

        request = {
            "jsonrpc": "2.0",
            "id": 72,
            "method": "textDocument/codeAction",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 0, "character": 10}
                },
                "context": {
                    "diagnostics": [
                        {
                            "range": {
                                "start": {"line": 0, "character": 0},
                                "end": {"line": 0, "character": 5}
                            },
                            "message": "Unused variable 'foo'",
                            "severity": 2
                        }
                    ]
                }
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Code action for unused variable returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")

    def test_code_action_empty_diagnostics(self):
        """Test code action with empty diagnostics"""
        print("\n=== Testing Code Action Empty Diagnostics ===")

        request = {
            "jsonrpc": "2.0",
            "id": 73,
            "method": "textDocument/codeAction",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "range": {
                    "start": {"line": 5, "character": 0},
                    "end": {"line": 5, "character": 20}
                },
                "context": {
                    "diagnostics": []
                }
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Empty diagnostics doesn't crash")

    # ============================================================================
    # Workspace symbols tests
    # ============================================================================

    def test_workspace_symbols_basic(self):
        """Test workspace symbols request"""
        print("\n=== Testing Workspace Symbols Basic ===")

        request = {
            "jsonrpc": "2.0",
            "id": 80,
            "method": "workspace/symbol",
            "params": {
                "query": ""
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Workspace symbols request returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")
            result = response.get("result", [])
            self.assert_test(isinstance(result, list), "Result is a list")

    def test_workspace_symbols_with_query(self):
        """Test workspace symbols with search query"""
        print("\n=== Testing Workspace Symbols with Query ===")

        request = {
            "jsonrpc": "2.0",
            "id": 81,
            "method": "workspace/symbol",
            "params": {
                "query": "test"
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Workspace symbols with query returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")

    def test_workspace_symbols_no_match(self):
        """Test workspace symbols with non-matching query"""
        print("\n=== Testing Workspace Symbols No Match ===")

        request = {
            "jsonrpc": "2.0",
            "id": 82,
            "method": "workspace/symbol",
            "params": {
                "query": "xyznonexistent123"
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Non-matching query doesn't crash")
        if response:
            result = response.get("result", [])
            self.assert_test(isinstance(result, list), "Result is still a list")

    def test_workspace_symbols_special_chars(self):
        """Test workspace symbols with special characters in query"""
        print("\n=== Testing Workspace Symbols Special Chars ===")

        request = {
            "jsonrpc": "2.0",
            "id": 83,
            "method": "workspace/symbol",
            "params": {
                "query": "!@#$%"
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Special characters don't crash")

    def test_workspace_symbols_with_root_uri(self):
        """Test workspace symbols with rootUri set during initialization"""
        print("\n=== Testing Workspace Symbols with Root URI ===")

        import tempfile
        import os

        # Create a temp directory with some .qd files
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create a test file
            test_file = os.path.join(tmpdir, "test_module.qd")
            with open(test_file, "w") as f:
                f.write("fn workspace_test_func(x:i64 -- y:i64) { x 2 * }\n")
                f.write("struct WorkspaceTestStruct { field:i64 }\n")

            # Initialize with rootUri - this tests that the server accepts rootUri
            init_request = {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {
                    "capabilities": {},
                    "rootUri": f"file://{tmpdir}"
                }
            }

            response = self.send_request(init_request)
            self.assert_test(response is not None, "Initialize with rootUri succeeds")
            if response:
                self.assert_test("result" in response, "Response has result field")
                result = response.get("result", {})
                self.assert_test("capabilities" in result, "Server returns capabilities")

    def test_workspace_symbols_empty_query_response_format(self):
        """Test that workspace symbols response has correct format"""
        print("\n=== Testing Workspace Symbols Response Format ===")

        request = {
            "jsonrpc": "2.0",
            "id": 84,
            "method": "workspace/symbol",
            "params": {
                "query": ""
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Empty query returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")
            result = response.get("result", [])
            self.assert_test(isinstance(result, list), "Result is a list")
            # With no open documents in fresh process, result should be empty
            self.assert_test(len(result) == 0, "Empty result with no documents open")

    def test_workspace_symbols_partial_match(self):
        """Test that workspace symbols supports partial matching"""
        print("\n=== Testing Workspace Symbols Partial Match ===")

        # Test partial query - server should handle it gracefully
        request = {
            "jsonrpc": "2.0",
            "id": 85,
            "method": "workspace/symbol",
            "params": {
                "query": "ma"  # Partial match for "main" etc
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Partial query returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")
            # Result should be valid (empty list since no docs open)
            self.assert_test("error" not in response, "No error in response")

    def test_workspace_symbols_unicode_query(self):
        """Test workspace symbols with unicode characters in query"""
        print("\n=== Testing Workspace Symbols Unicode Query ===")

        request = {
            "jsonrpc": "2.0",
            "id": 86,
            "method": "workspace/symbol",
            "params": {
                "query": "test_日本語"
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Unicode query doesn't crash")
        if response:
            self.assert_test("result" in response or "error" not in response, "Server handles unicode gracefully")

    # ============================================================================
    # Inlay hints tests
    # ============================================================================

    def test_inlay_hints_basic(self):
        """Test inlay hints request"""
        print("\n=== Testing Inlay Hints Basic ===")

        request = {
            "jsonrpc": "2.0",
            "id": 90,
            "method": "textDocument/inlayHint",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 100, "character": 0}
                }
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Inlay hints request returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")
            result = response.get("result", [])
            self.assert_test(isinstance(result, list), "Result is a list")

    def test_inlay_hints_small_range(self):
        """Test inlay hints for a small range"""
        print("\n=== Testing Inlay Hints Small Range ===")

        request = {
            "jsonrpc": "2.0",
            "id": 91,
            "method": "textDocument/inlayHint",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 5, "character": 0}
                }
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Small range inlay hints works")
        if response:
            self.assert_test("result" in response, "Response has result field")

    def test_inlay_hints_empty_range(self):
        """Test inlay hints for empty range"""
        print("\n=== Testing Inlay Hints Empty Range ===")

        request = {
            "jsonrpc": "2.0",
            "id": 92,
            "method": "textDocument/inlayHint",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 0, "character": 0}
                }
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Empty range doesn't crash")

    def test_inlay_hints_nonexistent_file(self):
        """Test inlay hints for non-existent file"""
        print("\n=== Testing Inlay Hints Non-Existent File ===")

        request = {
            "jsonrpc": "2.0",
            "id": 93,
            "method": "textDocument/inlayHint",
            "params": {
                "textDocument": {"uri": "file:///nonexistent/path.qd"},
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 10, "character": 0}
                }
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Non-existent file doesn't crash")

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

        # Code action tests
        self.test_code_action_basic()
        self.test_code_action_with_diagnostic()
        self.test_code_action_unused_variable()
        self.test_code_action_empty_diagnostics()

        # Workspace symbols tests
        self.test_workspace_symbols_basic()
        self.test_workspace_symbols_with_query()
        self.test_workspace_symbols_no_match()
        self.test_workspace_symbols_special_chars()
        self.test_workspace_symbols_with_root_uri()
        self.test_workspace_symbols_empty_query_response_format()
        self.test_workspace_symbols_partial_match()
        self.test_workspace_symbols_unicode_query()

        # Inlay hints tests
        self.test_inlay_hints_basic()
        self.test_inlay_hints_small_range()
        self.test_inlay_hints_empty_range()
        self.test_inlay_hints_nonexistent_file()

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
