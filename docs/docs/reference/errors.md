# Error handling

Built-in operations for error handling.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `panic` | `(msg code --)` | Signal a panic (error) |
| `err` | `(-- msg code)` | Get the message and code of the last failed call |
| `ok` | `(status -- flag)` | 1 if a fallible call's status is `Ok` |
| `fail` | `(status -- flag)` | 1 if a fallible call's status is an error code |
| `Ok` | `(-- 0)` | The status of a call that succeeded |
| `Err` | `(-- 1)` | The generic error code |

---

## Result constants

Quadrate provides two builtin constants for result handling:

- `Ok` = 0, the status of a call that succeeded
- `Err` = 1, the generic error code

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

### err

Pushes the message and code of the last failed call, and clears them. A failed call leaves zero
values where its results would be, so `err` is how an `else` branch or a `switch` arm gets at
the message.

**Signature:** `(-- msg code)`

```qd
use io

fn main() {
	"/no/such/file" io::Read io::open ok if {
		io::close
	} else {
		drop
		err "code: " print print nl     // 2
		"message: " print print nl  // io::open: No such file or directory
	}
}
```

---

## Fallible functions

Functions that can fail are marked with `!` after the signature:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		"division by zero" 2 panic
	}
	/
}
```

## Handling errors

!!! note "The status"
    A bare fallible call leaves its results and then a status: `Ok` (0) on success, the error
    code on failure. On failure the results are zero values (`0`, `0.0`, `""`, `null`). The shape
    is the same for functions written in Quadrate and for those imported from a native library,
    and the status is an ordinary value: it can be bound, kept and tested later.

    A `panic` may carry any code but `0`, which is `Ok`. Prefer codes >= 2 for your own modules,
    matching the stdlib convention, so each code stays distinguishable in a `switch`.

!!! warning "Drop the results in the arm that does not use them"
    Every arm starts with the call's results. The failure arms usually do not want the zero
    placeholders, and drop them; an arm that forgets leaves the stack deeper than the others,
    which the compiler reports:

    ```qd
    // doccheck: skip -- the shape of a switch on a status
    path io::Read io::open switch {
        Ok { io::close }
        io::ErrNotFound { drop  "not found" print nl }   // drop the null handle
    }
    ```

### With switch (recommended)

Use `switch` to match on specific error codes:

```qd
"/tmp/config.txt" io::Read io::open switch {
	Ok {
		"File opened!" print nl
		io::close
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
10 2 divide ok if {
	// Success: the result is on the stack
	print nl
} else {
	// Failure: the result is a zero placeholder
	drop
	"Division failed" print nl
}
```

## Calling fallible functions

### With switch (matching error codes)

Handle specific errors:

<!-- doccheck: skip excerpt: the body is elided and io is used without an import, to keep the switch arms in view -->
```qd
fn read_config( -- data:str)! {
	"/etc/app/config.txt" io::Read io::open switch {
		Ok {
			// Read file contents...
			io::close
		}
		io::ErrNotFound {
			"config not found" io::ErrNotFound panic
		}
		_ {
			err panic
		}
	}
}
```

### With if-else

Handle errors without matching specific codes:

<!-- doccheck: continues -->
```qd
fn compute(x:i64 -- result:i64)! {
	2 divide fail if {
		"compute failed" 2 panic
	}
	// Success: the result is on the stack
}
```

### With ! (abort on error)

Abort the program if an error occurs:

<!-- doccheck: continues -->
```qd
fn compute(x:i64 -- result:i64)! {
	2 divide!  // Aborts program if divide fails
	10 +
}
```

**Warning**: Using `!` terminates the entire program if the function fails. Only use when crashing is acceptable (e.g., during initialization or in scripts).

### With ? (propagate to caller)

Automatically propagate errors to the calling function:

<!-- doccheck: continues -->
```qd
fn compute(x:i64 -- result:i64)! {
	2 divide?  // Propagates error to caller if divide fails
	10 +
}
```

The `?` operator requires the enclosing function to be fallible (marked with `!`). On error, it immediately returns from the current function, preserving the error state.

This is equivalent to the more verbose pattern:

<!-- doccheck: continues -->
```qd
fn compute(x:i64 -- result:i64)! {
	2 divide switch {
		Ok { }
		_ { err panic }
	}
	10 +
}
```

---

## Error codes

By convention:

- `Ok` (0) = Success
- `Err` (1) = Generic error
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
4. **Clean up with defer** - Resources released on every exit from the arm that acquired them

```qd
use io

fn process(path:str -- result:i64)! {
	io::Read io::open switch {
		Ok {
			-> file
			defer {
				file io::close
			}
			// Process file...
			42
		}
		io::ErrNotFound {
			"file not found" io::ErrNotFound panic
		}
		io::ErrPermission {
			"permission denied" io::ErrPermission panic
		}
		_ {
			err panic
		}
	}
}
```
