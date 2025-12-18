# Testing

Quadrate has built-in support for writing and running tests.

## Quick Start

Create a test file `my_test.qd`:

```qd
use testing

test "addition works" {
	2 3 + 5 testing::assert_eq
}

test "strings can be compared" {
	"hello" "hello" testing::assert_eq
	"foo" "bar" testing::assert_ne
}
```

Run it:

```bash
quad test my_test.qd
```

Output:

```
Running 2 tests...
  ✓ my_test.qd::addition works
  ✓ my_test.qd::strings can be compared

2 passed, 0 failed
```

## How It Works

The `quad test` command runs the compiler in test mode. Under the hood, it uses the `--test` flag:

```bash
quad test my_tests.qd      # Preferred
quadc --test my_tests.qd   # Equivalent (low-level)
```

In test mode, the compiler:

1. Finds all `test` blocks in the file
2. Generates a test runner that executes each one
3. Reports pass/fail results
4. Does **not** call `main` (even if present)

Without test mode, the compiler expects a `main` function and ignores test blocks. The two modes are mutually exclusive.

## Writing Tests

### The `test` Keyword

Define tests using the `test` keyword followed by a description string and a block:

```qd
use testing

test "descriptive name" {
	// test code here
}
```

Multiple tests can be in the same file:

```qd
use testing

test "first test" {
	1 1 testing::assert_eq
}

test "second test" {
	2 2 testing::assert_eq
}
```

### Assertion Functions

The `testing` module provides these assertions:

| Function | Description |
|----------|-------------|
| `assert_eq` | Assert two values are equal |
| `assert_ne` | Assert two values are not equal |
| `assert_true` | Assert value is truthy (non-zero, non-empty) |
| `assert_false` | Assert value is falsy (zero, empty, null) |
| `fail` | Unconditionally fail with a message |

#### assert_eq

Compare two values for equality. Works with integers, floats, and strings:

```qd
use testing

test "assert_eq examples" {
	// integers
	5 5 testing::assert_eq

	// floats
	3.14 3.14 testing::assert_eq

	// strings
	"hello" "hello" testing::assert_eq
}
```

#### assert_ne

Assert two values are different:

```qd
use testing

test "assert_ne examples" {
	5 10 testing::assert_ne
	"foo" "bar" testing::assert_ne
}
```

#### assert_true / assert_false

Check truthiness:

```qd
use testing

test "truthiness" {
	1 testing::assert_true
	42 testing::assert_true
	"hello" testing::assert_true

	0 testing::assert_false
	"" testing::assert_false
}
```

#### fail

Force a test failure:

```qd
use testing

test "incomplete test" {
	"Not implemented yet" testing::fail
}
```

## Test File Conventions

When adding tests to the Quadrate project, follow these conventions:

### Output Tests

For tests that verify program output, create two files:

1. `test_name.qd` - The test program
2. `test_name.out` - The expected output

Example `tests/qd/arithmetic/add_numbers.qd`:

```qd
fn main() {
	5 3 add print nl
	10 20 add print nl
}
```

Example `tests/qd/arithmetic/add_numbers.out`:

```
8
30
```

The test runner compiles and runs the `.qd` file, then compares stdout to the `.out` file.

### Compile Error Tests

For tests that should fail at compile time, create:

1. `test_name.qd` - Code that should not compile
2. `test_name.err` - Expected error message substring(s)

Example `tests/qd/compile_errors/undefined_var.qd`:

```qd
fn main() {
	undefined_variable print nl
}
```

Example `tests/qd/compile_errors/undefined_var.err`:

```
Undefined identifier 'undefined_variable'
```

The test passes if compilation fails and the error output contains the expected message.

### Runtime Error Tests

For tests that compile but should fail at runtime:

1. `test_name.qd` - Code that fails at runtime
2. `test_name.runtime_err` - Expected runtime error message

### Unit Tests

For tests using the `testing` module:

1. `test_name.qd` - Test file with `test` blocks
2. `test_name.out` - Expected test runner output

Example `tests/qd/testing/my_tests.qd`:

```qd
use testing

test "math works" {
	2 2 + 4 testing::assert_eq
}
```

Example `tests/qd/testing/my_tests.out`:

```
Running 1 tests...
  ✓ my_tests.qd::math works

1 passed, 0 failed
```

## Running Tests

### Run a Single Test

```bash
quad test my_test.qd
```

### Run All Tests in Directory

If no file is specified, `quad test` looks for files matching `*_test.qd` or `test_*.qd`:

```bash
quad test
```

### Run All Language Tests

```bash
make tests
```

Or directly:

```bash
QUADC=build/debug/cmd/quadc/quadc bash tests/run_tests.sh qd
```

### Run Tests with Valgrind

Check for memory leaks:

```bash
QUADC=build/debug/cmd/quadc/quadc bash tests/run_tests.sh valgrind
```

## Test Organization

Tests in the Quadrate project are organized by category in `tests/qd/`:

```
tests/qd/
├── arithmetic/      # Math operations
├── arrays/          # Array operations
├── closures/        # Closure tests
├── compile_errors/  # Should-fail-to-compile tests
├── control_flow/    # if/else, loops, switch
├── errors/          # Error handling
├── fmt/             # Formatting module
├── io/              # I/O operations
├── strings/         # String operations
├── structs/         # Struct tests
├── testing/         # Testing module tests
└── ...
```

## Adding a New Test

1. Choose the appropriate category directory (or create one)
2. Create `test_name.qd` with your test code
3. Create `test_name.out` with expected output (or `.err` for compile errors)
4. Run `make tests` to verify

Example workflow:

```bash
# Create test file
cat > tests/qd/arithmetic/my_new_test.qd << 'EOF'
fn main() {
	100 50 sub print nl
}
EOF

# Create expected output
echo "50" > tests/qd/arithmetic/my_new_test.out

# Verify it works
make tests
```
