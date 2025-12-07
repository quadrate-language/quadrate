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

## Signaling Errors

Use the `error` instruction to signal an error:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop drop
		0  // Output value (required even on error)
		"division by zero" -1 error
	}
	/
}
```

The `error` instruction takes:

1. An error message (string)
2. An error code (integer)

## Handling Errors

When you call a fallible function, you **must** handle the error with `if`:

```qd
fn main( -- ) {
	10 2 divide if {
		// Success: result is on stack
		"Result: " print print nl
	} else {
		// Error: error value is on stack
		drop
		"Division failed!" print nl
	}
}
```

The compiler enforces this - you cannot ignore errors.

## How It Works

After calling a fallible function:

- **Success**: The `if` branch executes, result is on stack
- **Error**: The `else` branch executes, error value is on stack

## Complete Example

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop drop
		0
		"division by zero" -1 error
	}
	/
}

fn main( -- ) {
	// This will fail
	1 0 divide if {
		"1 / 0 = " print print nl
	} else {
		drop
		"Error: Cannot divide by zero!" print nl
	}

	// This will succeed
	10 2 divide if {
		"10 / 2 = " print print nl
	} else {
		drop
		"Unexpected error" print nl
	}
}
```

Output:
```
Error: Cannot divide by zero!
10 / 2 = 5
```

## Propagating Errors

Call a fallible function with `!` to propagate errors:

```qd
fn divide_and_double(a:i64 b:i64 -- result:i64)! {
	divide!  // Propagates error if divide fails
	2 *
}
```

If `divide` fails, `divide_and_double` will also fail with the same error.

## Using abort

Use `abort` to crash the program on error:

```qd
fn main( -- ) {
	// Crash program if divide fails
	10 0 divide abort
	print nl  // Never reached if divide fails
}
```

## Error Values

When an error occurs, the stack contains:

1. The output value(s) declared in the signature
2. An error value (which you should `drop`)

```qd
fn get_two( -- a:i64 b:i64)! {
	0 0
	"failed" 1 error
}

fn main( -- ) {
	get_two if {
		-> b -> a
		a print nl
		b print nl
	} else {
		drop  // The error value
		// The two output values (0, 0) are also on stack
		drop drop
	}
}
```

## Standard Library Errors

Many standard library functions are fallible:

```qd
use str
use io

fn main( -- ) {
	// String operations
	"hello" 1 3 str::substring if {
		print nl  // "ell"
	} else {
		drop
		"substring failed" print nl
	}

	// File operations
	"test.txt" io::ReadOnly io::open if {
		-> file
		"File opened" print nl
		file io::close
	} else {
		drop
		"Could not open file" print nl
	}
}
```

## Key Rules

1. Mark fallible functions with `!` after the signature
2. Use `"message" code error` to signal errors
3. Handle errors with `if { success } else { error }`
4. The compiler enforces error handling
5. Use `drop` to discard the error value in the else branch

## What's Next?

Learn [Error Handling Patterns](patterns.md) for common scenarios.
