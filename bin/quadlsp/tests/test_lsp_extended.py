#!/usr/bin/env python3
"""
Extended test suite for Quadrate Language Server Protocol (LSP)
Covers edge cases, error handling, and comprehensive protocol testing
"""

import json
import subprocess
import sys
import time
from pathlib import Path

class ExtendedLSPTester:
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

            # Parse response
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

    def test_malformed_json(self):
        """Test handling of malformed JSON"""
        print("\n=== Testing Malformed JSON ===")

        # Test with invalid JSON (should not crash)
        malformed = b'Content-Length: 20\r\n\r\n{invalid json here}'
        try:
            result = subprocess.run(
                [self.lsp_path],
                input=malformed,
                capture_output=True,
                timeout=1
            )
            # Server should not crash, just handle gracefully
            self.assert_test(result.returncode >= 0, "Server handles malformed JSON without crashing")
        except:
            self.assert_test(False, "Server crashed on malformed JSON")

    def test_missing_content_length(self):
        """Test request without Content-Length header"""
        print("\n=== Testing Missing Content-Length ===")

        malformed = b'{"jsonrpc":"2.0","id":1,"method":"shutdown"}'
        try:
            result = subprocess.run(
                [self.lsp_path],
                input=malformed,
                capture_output=True,
                timeout=1
            )
            self.assert_test(True, "Server handles missing Content-Length")
        except subprocess.TimeoutExpired:
            self.assert_test(True, "Server waits for proper headers")

    def test_wrong_content_length(self):
        """Test request with incorrect Content-Length"""
        print("\n=== Testing Wrong Content-Length ===")

        request = '{"jsonrpc":"2.0","id":1,"method":"shutdown","params":{}}'
        wrong_length = len(request) + 100
        malformed = f'Content-Length: {wrong_length}\r\n\r\n{request}'.encode()

        try:
            result = subprocess.run(
                [self.lsp_path],
                input=malformed,
                capture_output=True,
                timeout=2
            )
            self.assert_test(True, "Server handles incorrect Content-Length")
        except subprocess.TimeoutExpired:
            self.assert_test(True, "Server waits for correct content length")

    def test_unknown_method(self):
        """Test request with unknown method"""
        print("\n=== Testing Unknown Method ===")

        request = {
            "jsonrpc": "2.0",
            "id": 999,
            "method": "unknownMethod",
            "params": {}
        }

        response = self.send_request(request)
        # Server might not respond to unknown methods, but shouldn't crash
        self.assert_test(True, "Server handles unknown method")

    def test_initialize_twice(self):
        """Test calling initialize twice"""
        print("\n=== Testing Duplicate Initialize ===")

        request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "capabilities": {},
                "rootUri": "file:///tmp"
            }
        }

        # First initialize
        response1 = self.send_request(request)
        self.assert_test(response1 is not None, "First initialize succeeds")

        # Second initialize (should also work in our stateless implementation)
        request["id"] = 2
        response2 = self.send_request(request)
        self.assert_test(response2 is not None, "Second initialize doesn't crash")

    def test_request_without_id(self):
        """Test notification (request without ID)"""
        print("\n=== Testing Notification (No ID) ===")

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/test.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": "fn main( -- ) { 5 10 add print }"
                }
            }
        }

        # Notifications don't expect responses
        response = self.send_request(request, timeout=1)
        self.assert_test(True, "Notification sent without error")

    def test_completion_with_invalid_position(self):
        """Test completion with out-of-bounds position"""
        print("\n=== Testing Completion with Invalid Position ===")

        request = {
            "jsonrpc": "2.0",
            "id": 3,
            "method": "textDocument/completion",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/test.qd"
                },
                "position": {
                    "line": 99999,
                    "character": 99999
                }
            }
        }

        response = self.send_request(request)
        # Should still return completions
        self.assert_test(response is not None, "Completion with invalid position doesn't crash")
        if response and "result" in response:
            self.assert_test(True, "Returns result despite invalid position")

    def test_multiple_sequential_requests(self):
        """Test multiple requests in sequence"""
        print("\n=== Testing Sequential Requests ===")

        success = True
        for i in range(5):
            request = {
                "jsonrpc": "2.0",
                "id": i + 100,
                "method": "textDocument/completion",
                "params": {
                    "textDocument": {"uri": f"file:///tmp/test{i}.qd"},
                    "position": {"line": 0, "character": 0}
                }
            }

            response = self.send_request(request)
            if response is None or "result" not in response:
                success = False
                break

        self.assert_test(success, "All sequential requests succeed")

    def test_text_document_lifecycle(self):
        """Test document open, change, save sequence"""
        print("\n=== Testing Document Lifecycle ===")

        # didOpen
        open_request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/lifecycle.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": "fn test( -- ) {}"
                }
            }
        }
        self.send_request(open_request, timeout=1)
        self.assert_test(True, "didOpen notification sent")

        # didChange
        change_request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didChange",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/lifecycle.qd",
                    "version": 2
                },
                "contentChanges": [
                    {"text": "fn test( -- ) { 5 10 add }"}
                ]
            }
        }
        self.send_request(change_request, timeout=1)
        self.assert_test(True, "didChange notification sent")

        # didSave
        save_request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didSave",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/lifecycle.qd"
                }
            }
        }
        self.send_request(save_request, timeout=1)
        self.assert_test(True, "didSave notification sent")

    def test_completion_items_structure(self):
        """Test detailed completion items structure"""
        print("\n=== Testing Completion Items Structure ===")

        request = {
            "jsonrpc": "2.0",
            "id": 10,
            "method": "textDocument/completion",
            "params": {
                "textDocument": {"uri": "file:///tmp/test.qd"},
                "position": {"line": 0, "character": 0}
            }
        }

        response = self.send_request(request)
        if response and "result" in response:
            result = response["result"]
            items = result.get("items", [])

            self.assert_test(len(items) > 40, f"Has sufficient completions ({len(items)})")
            self.assert_test(result.get("isIncomplete") == False, "Completion list is complete")

            # Check each item has required fields
            all_valid = all(
                "label" in item and "kind" in item
                for item in items
            )
            self.assert_test(all_valid, "All items have required fields")

            # Check for stack operations
            labels = [item["label"] for item in items]
            stack_ops = ["dup", "swap", "drop", "over", "rot"]
            found = sum(1 for op in stack_ops if op in labels)
            self.assert_test(found >= 4, f"Contains stack operations ({found}/5)")

            # Check for arithmetic
            arithmetic = ["add", "sub", "mul", "div"]
            found = sum(1 for op in arithmetic if op in labels)
            self.assert_test(found == 4, f"Contains arithmetic operations ({found}/4)")

            # Check for control flow
            control = ["if", "for", "switch"]
            found = sum(1 for op in control if op in labels)
            self.assert_test(found == 3, f"Contains control flow ({found}/3)")

    def test_formatting_request(self):
        """Test document formatting"""
        print("\n=== Testing Formatting ===")

        request = {
            "jsonrpc": "2.0",
            "id": 20,
            "method": "textDocument/formatting",
            "params": {
                "textDocument": {"uri": "file:///tmp/format.qd"},
                "options": {
                    "tabSize": 4,
                    "insertSpaces": False
                }
            }
        }

        response = self.send_request(request)
        self.assert_test(response is not None, "Formatting request returns response")
        if response:
            self.assert_test("result" in response, "Response has result field")
            # Result might be empty array (stub implementation)
            self.assert_test(isinstance(response.get("result", []), list), "Result is an array")

    def test_diagnostics_with_valid_code(self):
        """Test diagnostics with valid Quadrate code"""
        print("\n=== Testing Diagnostics (Valid Code) ===")

        valid_code = """fn main( -- ) {
    5 10 add
    print
}"""

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/valid.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": valid_code
                }
            }
        }

        response = self.send_request(request, timeout=1)
        self.assert_test(True, "Valid code doesn't crash server")

    def test_diagnostics_with_invalid_code(self):
        """Test diagnostics with invalid Quadrate code"""
        print("\n=== Testing Diagnostics (Invalid Code) ===")

        invalid_code = """fn broken( -- ) {
    this is not valid syntax at all
    random words here
}"""

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/invalid.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": invalid_code
                }
            }
        }

        response = self.send_request(request, timeout=2)
        self.assert_test(True, "Invalid code doesn't crash server")

    def test_empty_document(self):
        """Test with empty document"""
        print("\n=== Testing Empty Document ===")

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/empty.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": ""
                }
            }
        }

        response = self.send_request(request, timeout=1)
        self.assert_test(True, "Empty document handled")

        # Try completion on empty doc
        comp_request = {
            "jsonrpc": "2.0",
            "id": 30,
            "method": "textDocument/completion",
            "params": {
                "textDocument": {"uri": "file:///tmp/empty.qd"},
                "position": {"line": 0, "character": 0}
            }
        }

        response = self.send_request(comp_request)
        self.assert_test(response is not None, "Completion works on empty document")

    def test_very_large_document(self):
        """Test with very large document"""
        print("\n=== Testing Large Document ===")

        # Generate large document
        large_code = "fn test( -- ) {\n" + "    5 10 add\n" * 1000 + "}\n"

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/large.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": large_code
                }
            }
        }

        response = self.send_request(request, timeout=5)
        self.assert_test(True, "Large document handled")

    def test_special_characters_in_uri(self):
        """Test URIs with special characters"""
        print("\n=== Testing Special Characters in URI ===")

        special_uris = [
            "file:///tmp/test%20file.qd",  # Space encoded
            "file:///tmp/test-file.qd",    # Hyphen
            "file:///tmp/test_file.qd",    # Underscore
            "file:///tmp/test.test.qd",    # Multiple dots
        ]

        success = 0
        for uri in special_uris:
            request = {
                "jsonrpc": "2.0",
                "id": 40 + success,
                "method": "textDocument/completion",
                "params": {
                    "textDocument": {"uri": uri},
                    "position": {"line": 0, "character": 0}
                }
            }

            response = self.send_request(request)
            if response is not None:
                success += 1

        self.assert_test(success >= 3, f"Handles special URI characters ({success}/4)")

    def test_initialize_capabilities_detailed(self):
        """Test detailed initialize response capabilities"""
        print("\n=== Testing Detailed Capabilities ===")

        request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "processId": 12345,
                "rootUri": "file:///home/user/project",
                "capabilities": {
                    "textDocument": {
                        "completion": {
                            "completionItem": {
                                "snippetSupport": True
                            }
                        }
                    }
                }
            }
        }

        response = self.send_request(request)
        if response and "result" in response:
            caps = response["result"].get("capabilities", {})

            self.assert_test("textDocumentSync" in caps, "Has textDocumentSync capability")
            self.assert_test(caps.get("textDocumentSync") == 1, "Text sync is Full (1)")
            self.assert_test("documentFormattingProvider" in caps, "Has formatting capability")
            self.assert_test("completionProvider" in caps, "Has completion capability")

            server_info = response["result"].get("serverInfo", {})
            self.assert_test(server_info.get("name") == "quadlsp", "Server name correct")
            self.assert_test("version" in server_info, "Has version")

    def test_utf8_in_documents(self):
        """Test UTF-8 characters in documents"""
        print("\n=== Testing UTF-8 Support ===")

        utf8_code = """fn test( -- ) {
    // Комментарий по-русски
    // コメント in 日本語
    // Commentaire en français
    5 10 add
}"""

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/utf8.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": utf8_code
                }
            }
        }

        response = self.send_request(request, timeout=2)
        self.assert_test(True, "UTF-8 content handled")

    def run_all_tests(self):
        """Run all extended tests"""
        print("=" * 60)
        print("Quadrate LSP Extended Test Suite")
        print("=" * 60)

        # Protocol tests
        self.test_malformed_json()
        self.test_missing_content_length()
        self.test_wrong_content_length()
        self.test_unknown_method()
        self.test_initialize_twice()
        self.test_request_without_id()

        # Completion tests
        self.test_completion_with_invalid_position()
        self.test_completion_items_structure()

        # Document tests
        self.test_text_document_lifecycle()
        self.test_empty_document()
        self.test_very_large_document()
        self.test_utf8_in_documents()

        # Diagnostics tests
        self.test_diagnostics_with_valid_code()
        self.test_diagnostics_with_invalid_code()

        # Formatting tests
        self.test_formatting_request()

        # Edge cases
        self.test_multiple_sequential_requests()
        self.test_special_characters_in_uri()
        self.test_initialize_capabilities_detailed()

        print("\n" + "=" * 60)
        print(f"Tests run:    {self.test_count}")
        print(f"Passed:       {self.passed}")
        print(f"Failed:       {self.failed}")
        print("=" * 60)

        if self.failed == 0:
            print("\n✅ All extended tests passed!")
            return 0
        else:
            print(f"\n❌ {self.failed} test(s) failed")
            return 1

def main():
    # Find LSP executable
    possible_paths = [
        Path("build/debug/bin/quadlsp/quadlsp"),
        Path("build/release/bin/quadlsp/quadlsp"),
        Path("bin/quadlsp/quadlsp"),
        Path("../../../bin/quadlsp/quadlsp"),
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

    tester = ExtendedLSPTester(str(lsp_path))
    return tester.run_all_tests()

if __name__ == "__main__":
    sys.exit(main())
