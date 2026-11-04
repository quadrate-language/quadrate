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

This finds the `*_test.qd` and `test_*.qd` files in the current directory and runs the `test`
blocks in each.

### Run tests in subdirectories too

```bash
quad test ./...
```

`./...` searches the current directory and every directory below it.

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

def factorial(n:i64 -- result:i64) {
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
		drop drop
		"division by zero" -1 panic
	}
	/
}

test "divide succeeds with valid input" {
	10 2 divide ok if {
		5 testing::assert_eq
	} else {
		drop
		"should not fail" testing::fail
	}
}

test "divide fails on zero" {
	10 0 divide ok if {
		// The success arm receives the result, so it has to consume it
		drop "should not succeed" testing::fail
	} else {
		drop
		// Error case: division by zero was caught
		1 testing::assert_true
	}
}
```

## Testing with modules

Import the module you're testing:

```qd
use testing
use strings

test "len counts characters" {
	"hello" len 5 testing::assert_eq
	"" len 0 testing::assert_eq
}

test "strings::contains finds substrings" {
	"hello world" "world" strings::contains testing::assert_true
	"hello world" "xyz" strings::contains testing::assert_false
}
```

## Key concepts

1. **`test "name" { }`** - Defines a test block with a descriptive name
2. **`use testing`** - Import the testing module for assertions
3. **`quad test`** - Run all tests in the project
4. **`*_test.qd`** - Naming convention for test files
5. **No `main` needed** - Test files don't require a main function

## What's next?

Learn about [Debugging](debugging.md) techniques for finding and fixing issues.
