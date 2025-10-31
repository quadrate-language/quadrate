# Quadrate Language Test Suite

This directory contains the test suite for the Quadrate programming language (.qd files).

## Directory Structure

```
tests/
├── qd/                    # Quadrate test files (.qd)
│   ├── basic/            # Basic language features
│   │   ├── arithmetic.qd
│   │   ├── stack_operations.qd
│   │   ├── advanced_stack_ops.qd
│   │   └── comparison_ops.qd
│   ├── control_flow/     # Control flow constructs
│   │   ├── for_loop_simple.qd
│   │   ├── for_loop_nested.qd
│   │   ├── for_loop_break.qd
│   │   ├── if_statement.qd
│   │   └── complex_control_flow.qd
│   ├── math/             # Mathematical functions
│   │   ├── math_functions.qd
│   │   └── trigonometry.qd
│   ├── strings/          # String operations
│   │   ├── string_operations.qd
│   │   └── utf8_strings.qd
│   ├── stack/            # Stack manipulation
│   │   └── new_stack_ops.qd
│   └── documentation/    # Documentation examples
│       └── stack_notation.qd
├── expected/             # Expected output files (.out)
│   ├── arithmetic.out
│   ├── stack_operations.out
│   └── ...
└── run_qd_tests.sh       # Test runner script
```

## Running Tests

### Run all tests:
```bash
./tests/run_qd_tests.sh
```

### Run with custom quadc binary:
```bash
QUADC=path/to/quadc ./tests/run_qd_tests.sh
```

## Test Output

The test runner will:
1. Compile each .qd file in `tests/qd/`
2. Execute the compiled binary
3. Compare output with the corresponding file in `tests/expected/`
4. Report PASS/FAIL for each test

Example output:
```
================================================
  Quadrate Language Test Suite
================================================

PASS  advanced_stack_ops
PASS  arithmetic
PASS  comparison_ops
PASS  stack_operations
PASS  complex_control_flow
PASS  for_loop_break
PASS  for_loop_nested
PASS  for_loop_simple
PASS  if_statement
PASS  stack_notation
PASS  math_functions
PASS  trigonometry
PASS  new_stack_ops
PASS  string_operations
PASS  utf8_strings

================================================
  Test Summary
================================================
Tests run:    15
Tests passed: 15
Tests failed: 0

All tests passed!
```

## Adding New Tests

1. Create a `.qd` file in the appropriate subdirectory under `tests/qd/`
2. Run the test manually to verify it works:
   ```bash
   build/debug/bin/quadc/quadc tests/qd/category/test_name.qd -r
   ```
3. Save the expected output to `tests/expected/test_name.out`:
   ```bash
   build/debug/bin/quadc/quadc tests/qd/category/test_name.qd -r > tests/expected/test_name.out
   ```
4. Run the test suite to verify the new test passes

## Test Categories

### basic/
Tests for fundamental language features:
- Arithmetic operations (add, sub, mul, div)
- Stack operations (dup, swap, over, etc.)
- Advanced stack operations (rot, nip, tuck, etc.)
- Comparison operations (eq, lt, gt, etc.)
- Literal values (integers, floats, strings)

### control_flow/
Tests for control flow constructs:
- `if` statements
- `for` loops (including $ iterator variable)
- Nested loops
- `break` and `continue`
- Complex control flow combinations

### math/
Tests for mathematical functions:
- Basic math operations (abs, min, max, etc.)
- Trigonometric functions (sin, cos, tan, etc.)

### strings/
Tests for string operations:
- Basic string operations (concat, length, etc.)
- UTF-8 string handling

### stack/
Tests for advanced stack manipulation operations

### documentation/
Tests for documentation features:
- Stack notation examples and validation

## Current Test Coverage

**Basic Features:**
- ✓ Arithmetic operations
- ✓ Stack operations (dup, swap, over)
- ✓ Advanced stack operations (rot, nip, tuck)
- ✓ Comparison operations (eq, lt, gt)

**Control Flow:**
- ✓ Simple for loops
- ✓ Nested for loops
- ✓ $ iterator variable in loops
- ✓ if statements
- ✓ break/continue
- ✓ Complex control flow combinations

**Mathematical Functions:**
- ✓ Basic math operations (abs, min, max)
- ✓ Trigonometric functions (sin, cos, tan)

**String Operations:**
- ✓ Basic string operations
- ✓ UTF-8 string handling

**Stack Operations:**
- ✓ Advanced stack manipulation

**Documentation:**
- ✓ Stack notation validation

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

- Test names should match between `.qd` files and `.out` files (basename only)
- Tests should be deterministic (same output every run)
- Keep tests small and focused on a single feature
- Use comments in .qd files to document what is being tested
