# Error Handling

Built-in operations for error handling.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `panic` | `(msg code --)` | Signal a panic (error) |
| `err` | `(-- msg code)` | Get error code from last fallible call |
| `Ok` | `(-- 1)` | Builtin constant for success |
| `Err` | `(-- 0)` | Builtin constant for generic error |

---

## Result Constants

Quadrate provides two builtin constants for result handling:

- `Ok` = 1 (success)
- `Err` = 0 (generic error)

Each module defines specific error codes starting at 2. For example:
- `io::ErrNotFound` = 2
- `io::ErrPermission` = 3
- `os::ErrNotFound` = 2
- `str::ErrOutOfBounds` = 2

---

## Signaling Panics

### panic

Signals a panic with a message and code. Used in fallible functions.

**Signature:** `(msg code --)`

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
		"division by zero" Err panic
	}
	/
}
```

## Handling Errors

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

## Calling Fallible Functions

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
	-> x
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
	-> x
	x 2 divide!  // Aborts program if divide fails
	10 +
}
```

**Warning**: Using `!` terminates the entire program if the function fails. Only use when crashing is acceptable (e.g., during initialization or in scripts).

### Propagating errors

To propagate errors to the caller, handle with `if/else` and call `panic`:

```qd
fn compute(x:i64 -- result:i64)! {
	-> x
	x 2 divide if {
		10 +
	} else {
		"compute failed" 1 panic
	}
}
```

---

## Error Codes

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

// str module
str::ErrOutOfBounds  // 2 - Index out of bounds
str::ErrAlloc        // 3 - Memory allocation failed
```

---

## Best Practices

1. **Use switch for specific errors** - Match module error codes
2. **Use if-else for simple cases** - When you don't need specific codes
3. **Use meaningful messages** - Help with debugging
4. **Clean up with defer** - Resources released on error

```qd
fn process(path:str -- result:i64)! {
	-> path
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
