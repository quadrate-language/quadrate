# Error handling basics

Quadrate has a robust error handling system built around **fallible functions**.

## Fallible functions

A fallible function is one that might fail. Mark it with `!` after the signature:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	// This function can fail
}
```

The `!` tells the compiler this function can return an error.

## Signaling panics

Use the `panic` instruction to signal an error:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	b 0 == if {
		"division by zero" -1 panic
	}
	a b /
}
```

The `panic` instruction takes (in push order):

1. An error message (string)
2. An error code (integer)

## Handling errors

When you call a fallible function, you **must** handle the error with `if`:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	b 0 == if {
		"division by zero" -1 panic
	}
	a b /
}

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

The compiler enforces this - you must either handle the error with `if`/`else`, or use `!` to abort on error (see below).

## How it works

After calling a fallible function:

- **Success (if branch)**: The function's outputs are on the stack
- **Error (else branch)**: The function's outputs are NOT on the stack (the stack is as it was before the call)

## Complete example

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	b 0 == if {
		"division by zero" -1 panic
	}
	a b /
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

## Aborting on error with `!`

Call a fallible function with `!` to abort the program on error:

```qd
fn divide_and_double(a:i64 b:i64 -- result:i64)! {
	a b divide!  // Aborts if divide fails
	2 *
}
```

**Warning**: If `divide` fails, the program will panic. Only use `!` when you're certain the call won't fail.

## Propagating errors with `?`

Call a fallible function with `?` to automatically propagate errors to the caller. The enclosing function must itself be fallible:

```qd
fn divide_and_double(a:i64 b:i64 -- result:i64)! {
	a b divide?  // If divide fails, return the error to our caller
	2 *
}
```

This is equivalent to the more verbose:

```qd
fn divide_and_double(a:i64 b:i64 -- result:i64)! {
	a b divide switch {
		Ok { }
		_ { err panic }
	}
	2 *
}
```

The `?` operator is useful when you want to let the caller handle the error instead of handling it locally. Errors chain naturally:

```qd
fn pipeline(x:i64 -- result:i64)! {
	x step1?   // propagates step1 errors
	step2?     // propagates step2 errors
	step3?     // propagates step3 errors
}
```

**Note**: `?` can only be used inside a fallible function (one marked with `!`). Using `?` in a non-fallible function is a compile error.

## Stack behavior

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

## Standard library errors

Many standard library functions are fallible:

```qd
use io

fn main() {
	"test.txt" io::Read io::open switch {
		Ok {
			-> file
			"File opened" print nl
			file io::close
		}
		_ {
			"Could not open file" print nl
		}
	}
}
```

## Retrieving error information

Use the `err` instruction to retrieve the error message and code:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	b 0 == if {
		"division by zero" 42 panic
	}
	a b /
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

The `err` instruction pushes `(-- msg code)`:
- `msg`: The error message string
- `code`: The error code integer (on top)

This is useful with `switch` to handle different error codes:

```qd
fn some_operation( -- result:i64)! {
	"something went wrong" 2 panic
}

fn main() {
	some_operation if {
		// Success
		drop
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

## Using switch for error handling

Instead of `if`/`else`, you can use `switch` with `Ok` and `Err` to handle fallible calls:

```qd
use io

fn main() {
	"test.txt" io::Read io::open switch {
		Ok {
			-> file
			"File opened" print nl
			file io::close
		}
		_ {
			"Could not open file" print nl
		}
	}
}
```

`Ok` is a literal `1` (success). After a fallible call, the top of stack is `1` on success. Use `_` as the default case to catch all error values.

To inspect the error details, use `err` in the error branch:

```qd
"missing.txt" io::read_file switch {
	Ok {
		-> content
		content print nl
	}
	_ {
		err -> code -> msg
		"Error: " print msg print " (code " print code print ")" print nl
	}
}
```

For standard library functions, prefer `switch { Ok { } _ { } }` over `if`/`else`.

## Key rules

1. Mark fallible functions with `!` after the signature
2. Use `"message" code panic` to signal panics
3. Handle errors with `if { success } else { error }` for your own functions, or `switch { Ok { } _ { } }` for standard library calls
4. Use `err` in the error branch to retrieve error details
5. Use `!` to abort on error (`func!`)
6. Use `?` to propagate errors to the caller (`func?`) — the enclosing function must be fallible
7. The compiler enforces error handling

## What's next?

Learn [Error Handling Patterns](patterns.md) for common scenarios.
