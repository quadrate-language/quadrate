# Quadrate Language Test Suite

This directory contains the test suite for the Quadrate programming language (.qd files).

## Directory Structure

```
tests/
├── qd/                    # Quadrate test files with expected outputs
│   ├── arithmetic/       # Arithmetic & mathematical operations
│   │   ├── arith_basic.qd
│   │   ├── arith_basic.out
│   │   ├── arith_comparison.qd
│   │   ├── arith_comparison.out
│   │   ├── arith_functions.qd
│   │   ├── arith_functions.out
│   │   └── arith_trigonometry.qd
│   │   └── arith_trigonometry.out
│   ├── stack/            # Stack manipulation operations
│   │   ├── stack_basic.qd
│   │   ├── stack_basic.out
│   │   ├── stack_pairs.qd
│   │   ├── stack_pairs.out
│   │   ├── stack_advanced.qd
│   │   ├── stack_advanced.out
│   │   ├── stack_utility.qd
│   │   └── stack_utility.out
│   ├── logic/            # Logical & bitwise operations
│   │   ├── logic_bitwise.qd
│   │   ├── logic_bitwise.out
│   │   ├── logic_minmax.qd
│   │   └── logic_minmax.out
│   ├── control_flow/     # Control flow constructs
│   │   ├── for_loop_simple.qd
│   │   ├── for_loop_simple.out
│   │   ├── for_loop_nested.qd
│   │   ├── for_loop_nested.out
│   │   └── ...
│   ├── strings/          # String operations
│   │   ├── string_operations.qd
│   │   ├── string_operations.out
│   │   ├── utf8_strings.qd
│   │   └── utf8_strings.out
│   └── documentation/    # Documentation examples
│       ├── stack_notation.qd
│       └── stack_notation.out
├── run_qd_tests_parallel.sh          # Parallel test runner
├── run_qd_tests_valgrind_parallel.sh # Parallel valgrind test runner
└── run_formatter_tests.sh            # Formatter test runner
```

## Running Tests

### Run all tests (parallel):
```bash
./tests/run_qd_tests_parallel.sh
```

### Run with custom quadc binary:
```bash
QUADC=path/to/quadc ./tests/run_qd_tests_parallel.sh
```

### Run with valgrind memory leak detection (parallel):
```bash
QUADC=path/to/quadc ./tests/run_qd_tests_valgrind_parallel.sh
```

## Test Output

The test runner will:
1. Compile each .qd file in `tests/qd/`
2. Execute the compiled binary
3. Compare output with the corresponding .out file in the same directory
4. Report PASS/FAIL for each test

Example output:
```
================================================
  Quadrate Language Test Suite
================================================

PASS  arith_basic
PASS  arith_comparison
PASS  arith_functions
PASS  arith_trigonometry
PASS  stack_basic
PASS  stack_pairs
PASS  stack_advanced
PASS  stack_utility
PASS  logic_bitwise
PASS  logic_minmax
PASS  complex_control_flow
PASS  for_loop_break
PASS  for_loop_nested
PASS  for_loop_simple
PASS  if_statement
PASS  stack_notation
PASS  string_operations
PASS  utf8_strings

================================================
  Test Summary
================================================
Tests run:    18
Tests passed: 18
Tests failed: 0

All tests passed!
```

## Adding New Tests

1. Create a `.qd` file in the appropriate subdirectory under `tests/qd/`
2. Run the test manually to verify it works:
   ```bash
   build/debug/bin/quadc/quadc tests/qd/category/test_name.qd -r
   ```
3. Save the expected output next to the test file:
   ```bash
   build/debug/bin/quadc/quadc tests/qd/category/test_name.qd -r > tests/qd/category/test_name.out
   ```
4. Run the test suite to verify the new test passes

## Test Categories

### arithmetic/
Tests for arithmetic and mathematical operations:
- **arith_basic.qd** - Basic arithmetic (add, sub, mul, div)
- **arith_comparison.qd** - Comparison operations (eq, neq, lt, gt, lte, gte, within)
- **arith_functions.qd** - Math functions (sq, abs, inc, dec, inv, fac, sqrt, cb, cbrt, ceil, floor)
- **arith_trigonometry.qd** - Trigonometric functions (sin, cos, tan, asin, acos, atan)

### stack/
Tests for stack manipulation operations:
- **stack_basic.qd** - Basic operations (dup, swap, over, drop, rot, nip)
- **stack_pairs.qd** - Pair operations (dup2, drop2, swap2, over2)
- **stack_advanced.qd** - Advanced operations (tuck, pick, roll)
- **stack_utility.qd** - Utility operations (clear, depth)

### logic/
Tests for logical and bitwise operations:
- **logic_bitwise.qd** - Bitwise operations (and, or, not)
- **logic_minmax.qd** - Logical functions (min, max, neg, mod)

### control_flow/
Tests for control flow constructs:
- `if` statements with conditions
- `for` loops (including $ iterator variable)
- Nested loops
- `break` and `continue` statements
- Complex control flow combinations

### strings/
Tests for string operations:
- Basic string printing and manipulation
- UTF-8 string handling with various character sets

### documentation/
Tests for documentation features:
- Stack notation examples and validation
- Function signature demonstrations

## Current Test Coverage

**Arithmetic Operations (4 tests):**
- ✓ Basic operations (add, sub, mul, div)
- ✓ Comparison operations (eq, neq, lt, gt, lte, gte, within)
- ✓ Math functions (sq, abs, inc, dec, inv, fac, sqrt, cb, cbrt, ceil, floor)
- ✓ Trigonometric functions (sin, cos, tan, asin, acos, atan)

**Stack Manipulation (4 tests):**
- ✓ Basic operations (dup, swap, over, drop, rot, nip)
- ✓ Pair operations (dup2, drop2, swap2, over2)
- ✓ Advanced operations (tuck, pick, roll)
- ✓ Utility operations (clear, depth)

**Logic Operations (2 tests):**
- ✓ Bitwise operations (and, or, not)
- ✓ Min/max and modulo (min, max, neg, mod)

**Control Flow (5 tests):**
- ✓ Simple for loops
- ✓ Nested for loops
- ✓ $ iterator variable in loops
- ✓ if/else statements
- ✓ break/continue
- ✓ Complex control flow combinations

**String Operations (2 tests):**
- ✓ Basic string operations
- ✓ UTF-8 string handling

**Documentation (1 test):**
- ✓ Stack notation validation

**Total: 18 language tests**

## Integration with Meson

The tests are integrated with the meson build system. Run tests with:

```bash
# Run all tests including .qd tests
meson test -C build/debug --print-errorlogs

# Run only .qd tests
meson test -C build/debug qd_tests --print-errorlogs

# Or use the Makefile shortcut
make tests
```

The integration is defined in `tests/meson.build` and automatically passes the correct `quadc` binary path to the test runner.

## Notes

- Each `.qd` test file should have a corresponding `.out` file in the same directory with expected output
- Tests should be deterministic (same output every run)
- Keep tests small and focused on a single feature
- Use comments in .qd files to document what is being tested
