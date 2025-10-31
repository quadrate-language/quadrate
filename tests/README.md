# Quadrate Language Test Suite

This directory contains the test suite for the Quadrate programming language (.qd files).

## Directory Structure

```
tests/
├── qd/                    # Quadrate test files (.qd)
│   ├── basic/            # Basic language features
│   │   ├── arithmetic.qd
│   │   └── stack_operations.qd
│   ├── control_flow/     # Control flow constructs
│   │   ├── for_loop_simple.qd
│   │   ├── for_loop_nested.qd
│   │   ├── for_loop_break.qd
│   │   └── if_statement.qd
│   ├── functions/        # Function tests
│   └── errors/           # Tests that should fail compilation
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

PASS  arithmetic
PASS  stack_operations
SKIP  for_loop_break (no expected output file)
PASS  for_loop_nested
PASS  for_loop_simple
PASS  if_statement

================================================
  Test Summary
================================================
Tests run:    6
Tests passed: 5
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
- Literal values (integers, floats, strings)

### control_flow/
Tests for control flow constructs:
- `if` statements
- `for` loops (including $ iterator variable)
- Nested loops
- `break` and `continue`

### functions/
Tests for function definitions and calls

### errors/
Tests that should fail compilation (for error message testing)

## Current Test Coverage

**Basic Features:**
- ✓ Arithmetic operations
- ✓ Stack operations (dup, swap, over)

**Control Flow:**
- ✓ Simple for loops
- ✓ Nested for loops
- ✓ $ iterator variable in loops
- ✓ if statements
- ⚠ break/continue (test exists but needs validation)

**Functions:**
- ✗ Not yet tested (type checker issues)

## Integration with Meson

To integrate with meson build system, add to `tests/meson.build`:

```python
# Quadrate language tests
test('qd_suite',
     find_program('run_qd_tests.sh'),
     workdir: meson.project_source_root(),
     timeout: 60)
```

## Notes

- Test names should match between `.qd` files and `.out` files (basename only)
- Tests should be deterministic (same output every run)
- Keep tests small and focused on a single feature
- Use comments in .qd files to document what is being tested
