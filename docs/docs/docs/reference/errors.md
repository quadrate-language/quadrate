# Error Handling

Built-in operations for error handling.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `panic` | `( code msg -- )` | Signal a panic (error) |

---

## Signaling Panics

### panic

Signals a panic with a code and message. Used in fallible functions.

**Signature:** `( code msg -- )`

```qd
1 "invalid input" panic
```

---

## Fallible Functions

Functions that can fail are marked with `!` after the signature:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop drop
		0  // Output value (required)
		-1 "division by zero" panic
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
	// Error: error value on stack
	drop
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
		drop 0
		1 "compute failed" panic
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

ErrNotFound "file not found" panic
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
		defer { file close_file }

		file read_data if {
			// Process...
		} else {
			drop 0
			1 "read failed" panic
		}
	} else {
		drop 0
		2 "open failed" panic
	}
}
```
