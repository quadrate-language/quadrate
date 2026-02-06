# Example: writing tests

Quadrate has built-in testing support. Write tests alongside your code and run them with `quad test`.

## Defining tests

Use the `test` keyword with a name and a block:

```qd
use testing

test "addition works" {
	2 3 + 5 testing::assert_eq
}

test "string comparison" {
	"hello" "hello" testing::assert_eq
}
```

Tests don't need a `main` function. Each `test` block runs independently.

## Assertion functions

The `testing` module provides these assertions:

| Function | Description |
|----------|-------------|
| `testing::assert_eq` | Assert two values are equal |
| `testing::assert_ne` | Assert two values are not equal |
| `testing::assert_true` | Assert a value is truthy (non-zero, non-empty) |
| `testing::assert_false` | Assert a value is falsy (zero, empty) |
| `testing::assert_approx_eq` | Assert two floats are approximately equal |
| `testing::fail` | Unconditionally fail with a message |

### Examples

```qd
use testing

test "assert_eq checks equality" {
	5 5 testing::assert_eq
	"hello" "hello" testing::assert_eq
}

test "assert_ne checks inequality" {
	5 6 testing::assert_ne
}

test "assert_true checks truthiness" {
	1 testing::assert_true
	42 testing::assert_true
	"non-empty" testing::assert_true
}

test "assert_false checks falsiness" {
	0 testing::assert_false
	"" testing::assert_false
}

test "assert_approx_eq for floats" {
	3.14159 3.14160 0.001 testing::assert_approx_eq
}
```

## Running tests

### Run all tests in a project

```bash
quad test
```

This discovers and runs all `*_test.qd` files and any files containing `test` blocks.

### Run tests in a directory

```bash
quad test ./...
```

### Run a specific test file

```bash
quad test math_test.qd
```

### Using the compiler directly

```bash
quadc --test -r my_test.qd
```

## Test file naming

By convention, test files are named `*_test.qd` and placed alongside the code they test:

```
mymodule/
  mymodule.qd
  mymodule_test.qd
```

## Testing your own functions

```qd
use testing

fn factorial(n:i64 -- result:i64) {
	-> n
	n 1 <= if {
		1
	} else {
		n n 1 - factorial *
	}
}

test "factorial of 0 is 1" {
	0 factorial 1 testing::assert_eq
}

test "factorial of 1 is 1" {
	1 factorial 1 testing::assert_eq
}

test "factorial of 5 is 120" {
	5 factorial 120 testing::assert_eq
}
```

## Testing error handling

Test fallible functions by checking both success and failure cases:

```qd
use testing

fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop2
		"division by zero" -1 panic
	}
	/
}

test "divide succeeds with valid input" {
	10 2 divide if {
		5 testing::assert_eq
	} else {
		"should not fail" testing::fail
	}
}

test "divide fails on zero" {
	10 0 divide if {
		"should not succeed" testing::fail
	} else {
		// Error case: division by zero was caught
		1 testing::assert_true
	}
}
```

## Testing with modules

Import the module you're testing:

```qd
use testing
use str

test "str::len counts characters" {
	"hello" str::len 5 testing::assert_eq
	"" str::len 0 testing::assert_eq
}

test "str::contains finds substrings" {
	"hello world" "world" str::contains testing::assert_true
	"hello world" "xyz" str::contains testing::assert_false
}
```

## Key concepts

1. **`test "name" { }`** - Defines a test block with a descriptive name
2. **`use testing`** - Import the testing module for assertions
3. **`quad test`** - Run all tests in the project
4. **`*_test.qd`** - Naming convention for test files
5. **No `main` needed** - Test files don't require a main function
