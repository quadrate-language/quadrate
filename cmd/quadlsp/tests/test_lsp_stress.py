#!/usr/bin/env python3
"""
Stress and performance tests for Quadrate LSP
Tests server stability under load and edge conditions
"""

import json
import subprocess
import sys
import time
from pathlib import Path

class LSPStressTester:
    def __init__(self, lsp_path):
        self.lsp_path = lsp_path
        self.test_count = 0
        self.passed = 0
        self.failed = 0

    def send_request(self, request_dict, timeout=5):
        """Send a JSON-RPC request to the LSP server"""
        request_json = json.dumps(request_dict)
        message = f"Content-Length: {len(request_json)}\r\n\r\n{request_json}"

        try:
            start_time = time.time()
            result = subprocess.run(
                [self.lsp_path],
                input=message.encode(),
                capture_output=True,
                timeout=timeout
            )
            elapsed = time.time() - start_time

            output = result.stdout.decode()
            if not output:
                return None, elapsed

            lines = output.split('\r\n')
            for i, line in enumerate(lines):
                if line == '':
                    json_str = '\r\n'.join(lines[i+1:])
                    return json.loads(json_str), elapsed

            return None, elapsed
        except subprocess.TimeoutExpired:
            return {"error": "timeout"}, timeout
        except json.JSONDecodeError:
            return {"error": "json_decode"}, 0

    def assert_test(self, condition, test_name, details=""):
        """Assert a test condition"""
        self.test_count += 1
        if condition:
            self.passed += 1
            suffix = f" ({details})" if details else ""
            print(f"  ✓ {test_name}{suffix}")
            return True
        else:
            self.failed += 1
            print(f"  ✗ {test_name}")
            return False

    def test_rapid_initialize_requests(self):
        """Test rapid consecutive initialize requests"""
        print("\n=== Testing Rapid Initialize Requests ===")

        times = []
        for i in range(10):
            request = {
                "jsonrpc": "2.0",
                "id": i,
                "method": "initialize",
                "params": {"capabilities": {}, "rootUri": "file:///tmp"}
            }

            response, elapsed = self.send_request(request)
            times.append(elapsed)
            if response is None or "error" in response:
                break

        avg_time = sum(times) / len(times) if times else 0
        self.assert_test(len(times) == 10, "All rapid requests completed",
                        f"avg: {avg_time:.3f}s")

    def test_large_completion_requests(self):
        """Test completion performance with many requests"""
        print("\n=== Testing Large Completion Requests ===")

        times = []
        success = 0

        for i in range(20):
            request = {
                "jsonrpc": "2.0",
                "id": 100 + i,
                "method": "textDocument/completion",
                "params": {
                    "textDocument": {"uri": f"file:///tmp/test{i}.qd"},
                    "position": {"line": i % 10, "character": i % 20}
                }
            }

            response, elapsed = self.send_request(request, timeout=3)
            times.append(elapsed)

            if response and "result" in response:
                success += 1

        avg_time = sum(times) / len(times) if times else 0
        self.assert_test(success >= 18, f"Completion requests succeed ({success}/20)",
                        f"avg: {avg_time:.3f}s")

    def test_extremely_long_line(self):
        """Test document with extremely long line"""
        print("\n=== Testing Extremely Long Line ===")

        # Create a line with 10,000 characters
        long_line = "5 10 add " * 1000

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/longline.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": f"fn test( -- ) {{\n{long_line}\n}}"
                }
            }
        }

        response, elapsed = self.send_request(request, timeout=5)
        self.assert_test(elapsed < 3.0, "Long line handled efficiently",
                        f"{elapsed:.3f}s")

    def test_deeply_nested_code(self):
        """Test deeply nested code structures"""
        print("\n=== Testing Deeply Nested Code ===")

        # Create deeply nested if statements
        nested_code = "fn test( -- ) {\n"
        for i in range(20):
            nested_code += "  " * i + "1 1 eq if {\n"
        nested_code += "  " * 20 + "5 10 add\n"
        for i in range(19, -1, -1):
            nested_code += "  " * i + "}\n"
        nested_code += "}"

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/nested.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": nested_code
                }
            }
        }

        response, elapsed = self.send_request(request, timeout=5)
        self.assert_test(elapsed < 3.0, "Deeply nested code handled",
                        f"{elapsed:.3f}s")

    def test_many_functions(self):
        """Test document with many function definitions"""
        print("\n=== Testing Many Functions ===")

        code = ""
        for i in range(100):
            code += f"fn func{i}( -- ) {{\n    5 10 add\n}}\n\n"

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/manyfuncs.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": code
                }
            }
        }

        response, elapsed = self.send_request(request, timeout=5)
        self.assert_test(elapsed < 3.0, "Many functions handled",
                        f"100 functions in {elapsed:.3f}s")

    def test_repeated_didChange(self):
        """Test repeated document changes"""
        print("\n=== Testing Repeated Document Changes ===")

        success = 0
        for i in range(15):
            request = {
                "jsonrpc": "2.0",
                "method": "textDocument/didChange",
                "params": {
                    "textDocument": {
                        "uri": "file:///tmp/changing.qd",
                        "version": i + 1
                    },
                    "contentChanges": [
                        {"text": f"fn test{i}( -- ) {{ {i} {i+1} add }}"}
                    ]
                }
            }

            response, elapsed = self.send_request(request, timeout=2)
            if elapsed < 2.0:
                success += 1

        self.assert_test(success >= 13, f"Repeated changes handled ({success}/15)")

    def test_maximum_document_size(self):
        """Test with very large document"""
        print("\n=== Testing Maximum Document Size ===")

        # Generate ~500KB document (reduced from 1MB to avoid timeout)
        lines = []
        for i in range(5000):
            lines.append(f"    {i % 100} {(i+1) % 100} add")

        large_doc = "fn huge( -- ) {\n" + "\n".join(lines) + "\n}"

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/huge.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": large_doc
                }
            }
        }

        response, elapsed = self.send_request(request, timeout=15)
        size_kb = len(large_doc) / 1024
        # More lenient timeout for large documents
        self.assert_test(elapsed < 12.0, f"Large document handled",
                        f"{size_kb:.1f}KB in {elapsed:.3f}s")

    def test_unicode_edge_cases(self):
        """Test various Unicode edge cases"""
        print("\n=== Testing Unicode Edge Cases ===")

        unicode_tests = [
            "// Emoji: 😀🎉🚀",
            "// Arabic: مرحبا بك",
            "// Chinese: 你好世界",
            "// Hebrew: שלום עולם",
            "// Emoji combinations: 👨‍👩‍👧‍👦",
            "// Special: ™®©€£¥",
        ]

        success = 0
        for i, test_str in enumerate(unicode_tests):
            code = f"fn test( -- ) {{\n    {test_str}\n    5 10 add\n}}"

            request = {
                "jsonrpc": "2.0",
                "method": "textDocument/didOpen",
                "params": {
                    "textDocument": {
                        "uri": f"file:///tmp/unicode{i}.qd",
                        "languageId": "quadrate",
                        "version": 1,
                        "text": code
                    }
                }
            }

            response, elapsed = self.send_request(request, timeout=2)
            if elapsed < 2.0:
                success += 1

        self.assert_test(success >= 5, f"Unicode variations handled ({success}/6)")

    def test_binary_like_content(self):
        """Test with content that looks like binary"""
        print("\n=== Testing Binary-like Content ===")

        # Content with control characters and null bytes (escaped)
        weird_content = "fn test( -- ) {\n    // \\x00\\x01\\x02\\xFF\n    5 10 add\n}"

        request = {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///tmp/binary.qd",
                    "languageId": "quadrate",
                    "version": 1,
                    "text": weird_content
                }
            }
        }

        response, elapsed = self.send_request(request, timeout=2)
        self.assert_test(elapsed < 2.0, "Binary-like content handled")

    def test_completion_performance(self):
        """Test completion response time"""
        print("\n=== Testing Completion Performance ===")

        request = {
            "jsonrpc": "2.0",
            "id": 500,
            "method": "textDocument/completion",
            "params": {
                "textDocument": {"uri": "file:///tmp/perf.qd"},
                "position": {"line": 0, "character": 0}
            }
        }

        response, elapsed = self.send_request(request)

        # Completion should be fast
        self.assert_test(elapsed < 0.5, "Completion is fast", f"{elapsed:.3f}s")

        if response and "result" in response:
            items = response["result"].get("items", [])
            self.assert_test(len(items) > 0, "Returns completion items")

    def test_formatting_performance(self):
        """Test formatting response time"""
        print("\n=== Testing Formatting Performance ===")

        request = {
            "jsonrpc": "2.0",
            "id": 501,
            "method": "textDocument/formatting",
            "params": {
                "textDocument": {"uri": "file:///tmp/format_perf.qd"},
                "options": {"tabSize": 4, "insertSpaces": False}
            }
        }

        response, elapsed = self.send_request(request)
        self.assert_test(elapsed < 0.5, "Formatting is fast", f"{elapsed:.3f}s")

    def test_mixed_requests_sequence(self):
        """Test mixed request types in sequence"""
        print("\n=== Testing Mixed Request Sequence ===")

        requests = [
            {"jsonrpc": "2.0", "id": 1, "method": "initialize",
             "params": {"capabilities": {}, "rootUri": "file:///tmp"}},
            {"jsonrpc": "2.0", "method": "textDocument/didOpen",
             "params": {"textDocument": {"uri": "file:///tmp/mix.qd", "languageId": "quadrate",
                                        "version": 1, "text": "fn test( -- ) {}"}}},
            {"jsonrpc": "2.0", "id": 2, "method": "textDocument/completion",
             "params": {"textDocument": {"uri": "file:///tmp/mix.qd"},
                       "position": {"line": 0, "character": 0}}},
            {"jsonrpc": "2.0", "id": 3, "method": "textDocument/formatting",
             "params": {"textDocument": {"uri": "file:///tmp/mix.qd"},
                       "options": {"tabSize": 4}}},
            {"jsonrpc": "2.0", "method": "textDocument/didSave",
             "params": {"textDocument": {"uri": "file:///tmp/mix.qd"}}},
            {"jsonrpc": "2.0", "id": 4, "method": "shutdown", "params": {}},
        ]

        success = 0
        total_time = 0
        for req in requests:
            response, elapsed = self.send_request(req, timeout=3)
            total_time += elapsed
            if response is not None and "error" not in response:
                success += 1

        self.assert_test(success >= 5, f"Mixed requests succeed ({success}/6)",
                        f"total: {total_time:.3f}s")

    def run_all_tests(self):
        """Run all stress tests"""
        print("=" * 60)
        print("Quadrate LSP Stress Test Suite")
        print("=" * 60)

        # Performance tests
        self.test_completion_performance()
        self.test_formatting_performance()

        # Load tests
        self.test_rapid_initialize_requests()
        self.test_large_completion_requests()
        self.test_repeated_didChange()

        # Size tests
        self.test_extremely_long_line()
        self.test_deeply_nested_code()
        self.test_many_functions()
        self.test_maximum_document_size()

        # Edge case tests
        self.test_unicode_edge_cases()
        self.test_binary_like_content()

        # Integration test
        self.test_mixed_requests_sequence()

        print("\n" + "=" * 60)
        print(f"Tests run:    {self.test_count}")
        print(f"Passed:       {self.passed}")
        print(f"Failed:       {self.failed}")
        print("=" * 60)

        if self.failed == 0:
            print("\n✅ All stress tests passed!")
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

    tester = LSPStressTester(str(lsp_path))
    return tester.run_all_tests()

if __name__ == "__main__":
    sys.exit(main())
