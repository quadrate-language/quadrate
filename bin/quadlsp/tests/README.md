# Quadrate LSP Test Suite

Comprehensive test suite for the Quadrate Language Server Protocol implementation.

## Test Files

### 1. `test_lsp.py` - Basic Functionality Tests (21 assertions)

Tests core LSP functionality and basic protocol compliance:

- **Initialize/Shutdown** (7 tests)
  - JSON-RPC version compliance
  - Request ID handling
  - Server capabilities (textDocumentSync, formatting, completion)
  - Server info (name, version)

- **Completion** (10 tests)
  - Completion items list (47 built-in instructions)
  - Item structure validation
  - Specific instructions present (add, sub, mul, dup, swap)

- **Diagnostics** (1 test)
  - Server stability with invalid code

- **Protocol Validation** (3 tests)
  - Response structure
  - JSON-RPC 2.0 compliance

**Runtime**: ~0.1s | **Timeout**: 30s

### 2. `test_lsp_extended.py` - Edge Cases & Error Handling (35 tests)

Tests protocol robustness and edge cases:

- **Protocol Edge Cases** (6 tests)
  - Malformed JSON handling
  - Missing/incorrect Content-Length headers
  - Unknown methods
  - Duplicate initialize
  - Notifications (requests without ID)

- **Completion Edge Cases** (8 tests)
  - Invalid positions
  - Detailed items structure
  - Stack operations coverage
  - Arithmetic operations
  - Control flow keywords

- **Document Lifecycle** (5 tests)
  - didOpen → didChange → didSave sequence
  - Empty documents
  - Very large documents (5000 lines)
  - UTF-8 content

- **Diagnostics** (2 tests)
  - Valid Quadrate code
  - Invalid Quadrate code

- **Formatting** (3 tests)
  - Request handling
  - Response structure

- **Edge Cases** (11 tests)
  - Sequential requests
  - Special characters in URIs
  - Detailed capabilities
  - Unicode support (Russian, Japanese, French, Arabic, Hebrew, Emoji)

**Runtime**: ~0.3s | **Timeout**: 60s

### 3. `test_lsp_stress.py` - Performance & Load Tests (13 tests)

Tests server performance under stress:

- **Performance Tests** (2 tests)
  - Completion speed (< 0.5s)
  - Formatting speed (< 0.5s)

- **Load Tests** (3 tests)
  - 10 rapid initialize requests
  - 20 completion requests
  - 15 repeated document changes

- **Size Tests** (4 tests)
  - Extremely long lines (10,000 chars)
  - Deeply nested code (20 levels)
  - Many functions (100 functions)
  - Maximum document size (~67KB / 5000 lines)

- **Edge Cases** (2 tests)
  - Unicode variations (6 scripts)
  - Binary-like content

- **Integration** (1 test)
  - Mixed request sequence

**Runtime**: ~3.4s | **Timeout**: 120s

## Running Tests

### Run All LSP Tests

```bash
# Via make (recommended)
make tests

# Via meson
meson test -C build/debug test_lsp test_lsp_extended test_lsp_stress --print-errorlogs

# Individual test suites
python3 bin/quadlsp/tests/test_lsp.py
python3 bin/quadlsp/tests/test_lsp_extended.py
python3 bin/quadlsp/tests/test_lsp_stress.py
```

### Run Specific Test Suite

```bash
# Basic tests only
meson test -C build/debug test_lsp

# Extended tests only
meson test -C build/debug test_lsp_extended

# Stress tests only
meson test -C build/debug test_lsp_stress
```

## Test Coverage Summary

| Category | Tests | Coverage |
|----------|-------|----------|
| Protocol Compliance | 15 | JSON-RPC 2.0, headers, errors |
| Completion | 18 | Items, structure, performance |
| Diagnostics | 3 | Valid/invalid code, stability |
| Document Lifecycle | 8 | Open, change, save, large docs |
| Formatting | 4 | Request handling, performance |
| Edge Cases | 21 | Unicode, URIs, errors, load |
| **Total** | **69** | **Comprehensive coverage** |

## Test Results

All tests pass successfully:

```
✅ test_lsp:          21/21 assertions pass (0.1s)
✅ test_lsp_extended: 35/35 assertions pass (0.3s)
✅ test_lsp_stress:   13/13 assertions pass (3.4s)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   69/69 total assertions pass
```

## Adding New Tests

To add new tests:

1. Choose the appropriate test file:
   - `test_lsp.py` for basic functionality
   - `test_lsp_extended.py` for edge cases
   - `test_lsp_stress.py` for performance/load

2. Add a test method to the tester class
3. Add the test call to `run_all_tests()`
4. Run the test to verify it passes

Example:

```python
def test_my_feature(self):
    """Test description"""
    print("\n=== Testing My Feature ===")

    request = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "myMethod",
        "params": {}
    }

    response = self.send_request(request)
    self.assert_test(response is not None, "Feature works")
```

## Performance Benchmarks

Current performance metrics (on debug build):

- **Completion**: < 5ms average
- **Formatting**: < 5ms average
- **Initialize**: < 5ms average
- **Large document (67KB)**: ~3.1s
- **100 functions**: ~5ms

## CI/CD Integration

These tests are automatically run by:

- `make tests` - Full test suite
- Meson test framework - Individual suites
- Can be integrated into CI pipelines

## Requirements

- Python 3.6+
- Built LSP executable (`quadlsp`)
- JSON support in Python (standard library)

## Troubleshooting

### Tests time out

- Check if LSP process is hanging
- Increase timeout in meson.build
- Run individual test to isolate issue

### JSON decode errors

- Check LSP output format
- Verify Content-Length headers
- Run LSP manually to inspect output

### LSP not found

- Ensure project is built: `make debug`
- Check paths in test scripts
- Verify LSP executable exists: `ls build/debug/bin/quadlsp/quadlsp`
