# Error Handling

Built-in operations for error handling.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `error` | `( msg code -- )` | Signal an error |

---

## Signaling Errors

### error

Signals an error with a message and code. Used in fallible functions.

**Signature:** `( msg code -- )`

```qd
"invalid input" 1 error
```

---

## Fallible Functions

Functions that can fail are marked with `!` after the signature:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop drop
		0  // Output value (required)
		"division by zero" -1 error
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

## Error Propagation

### With if-else

Re-raise the error in your own fallible function:

```qd
fn compute(x:i64 -- result:i64)! {
	-> x
	x 2 divide if {
		// Success
	} else {
		drop 0
		"compute failed" 1 error
	}
}
```

### With !

Propagate errors directly:

```qd
fn compute(x:i64 -- result:i64)! {
	-> x
	x 2 divide!  // Propagates error if divide fails
	10 +
}
```

### With abort

Crash the program on error:

```qd
fn main( -- ) {
	10 0 divide abort
	// Never reached if divide fails
}
```

---

## Error Codes

By convention:

- `0` = Success (no error)
- Negative values = System errors
- Positive values = Application errors

```qd
const ERR_INVALID_INPUT 1
const ERR_NOT_FOUND 2
const ERR_TIMEOUT 3

"file not found" ERR_NOT_FOUND error
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
			"read failed" 1 error
		}
	} else {
		drop 0
		"open failed" 2 error
	}
}
```
