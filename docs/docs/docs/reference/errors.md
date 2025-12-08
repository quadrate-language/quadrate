# Error Handling

Built-in operations for error handling.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `panic` | `( msg code -- )` | Signal a panic (error) |
| `err` | `( -- msg code )` | Get error code from last fallible call |

---

## Signaling Panics

### panic

Signals a panic with a message and code. Used in fallible functions.

**Signature:** `( msg code -- )`

```qd
"invalid input" 1 panic
```

---

## Fallible Functions

Functions that can fail are marked with `!` after the signature:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop2
		"division by zero" -1 panic
	}
	/
}
```

## Handling Errors

Use `if-else` to handle errors from fallible functions:

```qd
10 2 divide if {
	// Success: result on stack
	print nl
} else {
	"Division failed" print nl
}
```

## Calling Fallible Functions

### With if-else (recommended)

Handle errors explicitly:

```qd
fn compute(x:i64 -- result:i64)! {
	-> x
	x 2 divide if {
		// Success
	} else {
		drop
		"compute failed" 1 panic
	}
}
```

### With ! (skip error check)

Skip error handling - panics if an error occurs:

```qd
fn compute(x:i64 -- result:i64)! {
	-> x
	x 2 divide!  // Aborts program if divide fails
	10 +
}
```

**Warning**: Using `!` means the program will crash if the function fails. Only use when you're certain the call won't fail, or when panicking is acceptable.

---

## Error Codes

By convention:

- `0` = Success (no error)
- Negative values = System errors
- Positive values = Application errors

```qd
const ErrInvalidInput = 1
const ErrNotFound = 2
const ErrTimeout = 3

"file not found" ErrNotFound panic
```

---

## Best Practices

1. **Always handle errors** - The compiler enforces this
2. **Use meaningful messages** - Help with debugging
3. **Use consistent error codes** - Define constants
4. **Clean up with defer** - Resources released on error

```qd
fn process(path:str -- result:i64)! {
	-> path
	path open_file if {
		-> file
		defer {
			file close_file
		}

		file read_data if {
			// Process...
		} else {
			drop
			"read failed" 1 panic
		}
	} else {
		drop
		"open failed" 2 panic
	}
}
```
