# Error handling

Built-in operations for error handling.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `panic` | `(msg code --)` | Signal a panic (error) |
| `err` | `(-- msg code)` | Get error code from last fallible call |
| `Ok` | `(-- 1)` | Builtin constant for success |
| `Err` | `(-- 0)` | Builtin constant for generic error |

---

## Result constants

Quadrate provides two builtin constants for result handling:

- `Ok` = 1 (success)
- `Err` = 0 (generic error)

Each module defines specific error codes starting at 2. For example:
- `io::ErrNotFound` = 2
- `io::ErrPermission` = 3
- `os::ErrNotFound` = 2
- `strings::ErrOutOfBounds` = 2

---

## Signaling panics

### panic

Signals a panic with a message and code. Used in fallible functions.

**Signature:** `(msg code --)`

```qd
"invalid input" 1 panic
```

---

## Fallible functions

Functions that can fail are marked with `!` after the signature:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop2
		"division by zero" 1 panic
	}
	/
}
```

## Handling errors

!!! note "Error codes and `Ok`"
    A fallible call leaves a status the consuming `if` or `switch` reads. Both idioms work for
    functions written in Quadrate and for those imported from a native library.

    `Ok` means "the call succeeded", not "the value equals 1" — so a `panic` carrying code `1`
    is still recognised as a failure and will not match `Ok`. Prefer codes >= 2 for your own
    modules anyway, matching the stdlib convention, so that each code stays distinguishable in
    a `switch`.

### With switch (recommended)

Use `switch` to match on specific error codes:

```qd
"/tmp/config.txt" io::Read io::open switch {
	Ok {
		-> file
		"File opened!" print nl
		file io::close
	}
	io::ErrNotFound {
		drop
		"File not found" print nl
	}
	io::ErrPermission {
		drop
		"Permission denied" print nl
	}
	_ {
		drop
		"Unknown error" print nl
	}
}
```

### With if-else

Handle success/failure without matching specific errors:

```qd
10 2 divide if {
	// Success: result on stack
	print nl
} else {
	"Division failed" print nl
}
```

## Calling fallible functions

### With switch (matching error codes)

Handle specific errors:

```qd
fn read_config( -- data:str)! {
	"/etc/app/config.txt" io::Read io::open switch {
		Ok {
			-> file
			// Read file contents...
			file io::close
		}
		io::ErrNotFound {
			drop
			"config not found" io::ErrNotFound panic
		}
		_ {
			-> code
			"config read failed" code panic
		}
	}
}
```

### With if-else

Handle errors without matching specific codes:

```qd
fn compute(x:i64 -- result:i64)! {
	x 2 divide if {
		// Success
	} else {
		drop
		"compute failed" Err panic
	}
}
```

### With ! (abort on error)

Abort the program if an error occurs:

```qd
fn compute(x:i64 -- result:i64)! {
	x 2 divide!  // Aborts program if divide fails
	10 +
}
```

**Warning**: Using `!` terminates the entire program if the function fails. Only use when crashing is acceptable (e.g., during initialization or in scripts).

### With ? (propagate to caller)

Automatically propagate errors to the calling function:

```qd
fn compute(x:i64 -- result:i64)! {
	x 2 divide?  // Propagates error to caller if divide fails
	10 +
}
```

The `?` operator requires the enclosing function to be fallible (marked with `!`). On error, it immediately returns from the current function, preserving the error state.

This is equivalent to the more verbose pattern:

```qd
fn compute(x:i64 -- result:i64)! {
	x 2 divide switch {
		Ok { }
		_ { err panic }
	}
	10 +
}
```

---

## Error codes

By convention:

- `Ok` (1) = Success
- `Err` (0) = Generic error
- Module-specific errors start at 2

Each module defines its own error codes:

```qd
// io module
io::ErrNotFound      // 2 - File not found
io::ErrPermission    // 3 - Permission denied
io::ErrInvalidHandle // 4 - Invalid file handle

// os module
os::ErrNotFound      // 2 - No such file or directory
os::ErrPermission    // 3 - Permission denied
os::ErrExists        // 4 - File already exists

// strings module
strings::ErrOutOfBounds  // 2 - Index out of bounds
strings::ErrAlloc        // 3 - Memory allocation failed
```

---

## Best practices

1. **Use switch for specific errors** - Match module error codes
2. **Use if-else for simple cases** - When you don't need specific codes
3. **Use meaningful messages** - Help with debugging
4. **Clean up with defer** - Resources released on error

```qd
fn process(path:str -- result:i64)! {
	path io::Read io::open switch {
		Ok {
			-> file
			defer {
				file io::close
			}
			// Process file...
			42
		}
		io::ErrNotFound {
			drop
			"file not found" io::ErrNotFound panic
		}
		io::ErrPermission {
			drop
			"permission denied" io::ErrPermission panic
		}
		_ {
			-> code
			"open failed" code panic
		}
	}
}
```
