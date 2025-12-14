# Error Handling Basics

Quadrate has a robust error handling system built around **fallible functions**.

## Fallible Functions

A fallible function is one that might fail. Mark it with `!` after the signature:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	// This function can fail
}
```

The `!` tells the compiler this function can return an error.

## Signaling Panics

Use the `panic` instruction to signal an error:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop2
		"division by zero" -1 panic
	}
	/
}
```

The `panic` instruction takes (in push order):

1. An error message (string)
2. An error code (integer)

## Handling Errors

When you call a fallible function, you **must** handle the error with `if`:

```qd
fn main() {
	10 2 divide if {
		// Success: result is on stack
		"Result: " print print nl
	} else {
		// Error: stack is unchanged from before the call
		"Division failed!" print nl
	}
}
```

The compiler enforces this - you cannot ignore errors.

## How It Works

After calling a fallible function:

- **Success (if branch)**: The function's outputs are on the stack
- **Error (else branch)**: The function's outputs are NOT on the stack (the stack is as it was before the call)

## Complete Example

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop2
		"division by zero" -1 panic
	}
	/
}

fn main() {
	// This will fail
	1 0 divide if {
		"1 / 0 = " print print nl
	} else {
		"Error: Cannot divide by zero!" print nl
	}

	// This will succeed
	10 2 divide if {
		"10 / 2 = " print print nl
	} else {
		"Unexpected error" print nl
	}
}
```

Output:
```
Error: Cannot divide by zero!
10 / 2 = 5
```

## Skipping Error Checks

Call a fallible function with `!` to skip error handling:

```qd
fn divide_and_double(a:i64 b:i64 -- result:i64)! {
	divide!  // Aborts if divide fails
	2 *
}
```

**Warning**: If `divide` fails, the program will panic. Only use `!` when you're certain the call won't fail.

## Stack Behavior

When a fallible function succeeds, the `if` branch has the function's outputs on the stack.

When a fallible function fails (via `panic`), the `else` branch does NOT have the function's outputs - the stack is as it was before the call.

```qd
fn get_two( -- a:i64 b:i64)! {
	"failed" 1 panic
}

fn main() {
	get_two if {
		// Success: outputs are on stack
		-> b -> a
		a print nl
		b print nl
	} else {
		// Error: outputs are NOT on stack
		"Function failed" print nl
	}
}
```

## Standard Library Errors

Many standard library functions are fallible:

```qd
use str
use io

fn main() {
	// String operations
	"hello" 1 3 str::substring if {
		print nl  // "ell"
	} else {
		"substring failed" print nl
	}

	// File operations
	"test.txt" io::Read io::open if {
		-> file
		"File opened" print nl
		file io::close
	} else {
		"Could not open file" print nl
	}
}
```

## Retrieving Error Information

Use the `err` instruction to retrieve the error message and code:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop2
		"division by zero" 42 panic
	}
	/
}

fn main() {
	10 0 divide if {
		print nl
	} else {
		err -> code -> msg
		"Error: " print msg print " (code " print code print ")" print nl
	}
}
```

Output:
```
Error: division by zero (code 42)
```

The `err` instruction pushes `( -- msg code )`:
- `msg`: The error message string
- `code`: The error code integer (on top)

This is useful with `switch` to handle different error codes:

```qd
fn main() {
	some_operation if {
		// Success
	} else {
		err switch {
			1 {
				drop "File not found" print nl
			}
			2 {
				drop "Permission denied" print nl
			}
			_ {
				"Unknown error: " print print nl
			}
		}
	}
}
```

## Key Rules

1. Mark fallible functions with `!` after the signature
2. Use `"message" code panic` to signal panics
3. Handle errors with `if { success } else { error }`
4. Use `err` in the else branch to retrieve error details
5. The compiler enforces error handling
6. The `if` branch has the function outputs; the `else` branch does not

## What's Next?

Learn [Error Handling Patterns](patterns.md) for common scenarios.
