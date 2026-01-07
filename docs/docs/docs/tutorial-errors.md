# Tutorial: error handling

Quadrate has a robust error handling system built around **fallible functions**. This tutorial covers how to define, call, and handle errors in your code.

## Fallible functions

A fallible function is one that might fail. Mark it with `!` after the signature:

```qd
fn division(a:i64 b:i64 -- result:i64)! {
	// This function can fail
}
```

The `!` tells the compiler this function can return an error.

## Signaling errors

Use the `panic` instruction to signal an error:

```qd
fn division(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop2
		"division by zero" -1 panic
	}
	div
}
```

The `panic` instruction takes:
1. An error message (string)
2. An error code (integer)

When a function panics, it does **not** produce any output values.

## Calling fallible functions

When you call a fallible function, you **must** handle the error with `if`:

```qd
fn main() {
	10 2 division if {
		// Success: result is on stack
		"Result: " print print nl
	} else {
		// Error: no outputs on stack
		"Division failed!" print nl
	}
}
```

The compiler enforces this - you cannot ignore errors.

## How it works

After calling a fallible function:

- **Success**: The `if` branch executes, function outputs are on stack
- **Error**: The `else` branch executes, function outputs are NOT on stack

## Using switch instead of if

You can also use `switch` to handle fallible function results:

```qd
fn main() {
	10 0 division switch {
		Ok {
			// Success: result is on stack
			"Result: " print print nl
		}
		Err {
			// Error occurred
			"Division failed!" print nl
		}
	}
}
```

The `switch` matches:
- `Ok` for successful execution (outputs on stack)
- `Err` for error (no outputs on stack)

To get error details in the `Err` branch, use the `err` instruction:

```qd
fn main() {
	10 0 division switch {
		Ok {
			"Result: " print print nl
		}
		Err {
			err -> code -> msg
			"Error: " print msg print " (code " print code print ")" print nl
		}
	}
}
```

## Complete example

```qd
fn division(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop2
		"division by zero" -1 panic
	}
	div
}

fn main() {
	// This will fail
	1 0 division if {
		"1 / 0 = " print print nl
	} else {
		"Error: Cannot divide by zero!" print nl
	}

	// This will succeed
	10 2 division if {
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

## Propagating errors

To propagate an error up to the caller, handle it with `if/else` and call `panic`:

```qd
fn divide_and_double(a:i64 b:i64 -- result:i64)! {
	division if {
		2 *
	} else {
		"division failed" 1 panic
	}
}
```

If `division` fails, the `else` branch runs and `divide_and_double` panics with its own error.

## Aborting on error

Use `!` when calling a fallible function to **abort the program** if it fails:

```qd
fn divide_and_double(a:i64 b:i64 -- result:i64)! {
	division!  // ABORTS program if division fails
	2 *
}
```

**Warning**: The `!` operator does NOT propagate errors - it terminates the entire program. Only use it when crashing is acceptable (e.g., during initialization or in scripts).

## Standard library errors

Many standard library functions are fallible. Common examples:

### String operations

```qd
use str

fn main() {
	"hello" 1 3 str::substring! prints nl  // "ell"

	"hello world" "l" "L" str::replace! prints nl  // "heLLo worLd"
}
```

### File I/O

```qd
use io
use mem

fn main() {
	"test.txt" io::ReadOnly io::open if {
		-> file
		defer { file io::close }

		1024 -> buf_size
		buf_size mem::alloc! -> buf

		file buf buf_size io::read if {
			-> bytes_read
			"Read " print bytes_read print " bytes" print nl
		} else {
			"Read error" print nl
		}

		buf mem::free
	} else {
		"Could not open file" print nl
	}
}
```

## The `defer` statement

Use `defer` to ensure cleanup happens even when errors occur:

```qd
use io
use mem

fn read_file(path:str -- content:str)! {
	-> path
	path io::ReadOnly io::open if {
		-> file
		defer { file io::close }  // Always closes file

		4096 -> size
		size mem::alloc! -> buf
		defer { buf mem::free }  // Always frees buffer

		file buf size io::read if {
			-> bytes_read
			buf bytes_read mem::to_string
		} else {
			"read failed" 1 panic
		}
	} else {
		"open failed" 1 panic
	}
}
```

Defers execute in LIFO order (last registered, first executed).

## Error handling patterns

### Pattern 1: handle and continue

```qd
fn safe_divide(a:i64 b:i64 -- result:i64) {
	division if {
		// Success - return result
	} else {
		0  // Return default value on error
	}
}
```

### Pattern 2: early return on error

```qd
fn process(value:i64 -- result:i64)! {
	-> value
	value 2 division if {
		-> half
		half 0 > if {
			half
		} else {
			"value too small" 1 panic
		}
	} else {
		"division failed" 1 panic
	}
}
```

### Pattern 3: collect multiple results

```qd
fn try_all( -- success_count:i64) {
	0 -> count

	10 2 division if {
		drop
		count 1 + -> count
	}

	10 5 division if {
		drop
		count 1 + -> count
	}

	10 0 division if {
		drop
		count 1 + -> count
	}

	count
}

fn main() {
	try_all print " operations succeeded" print nl  // 2
}
```

## Summary

Key concepts:

1. **Mark fallible functions** with `!` after the signature
2. **Signal errors** with `"message" code panic`
3. **Handle errors** with `if { success } else { error }` or `switch`
4. **Propagate errors** by handling with `if/else` and calling `panic` in the else branch
5. **Abort on error** by calling with `function!` (terminates program)
6. **Use `defer`** for cleanup that runs regardless of errors
7. **The compiler enforces** error handling - you can't ignore errors

## Next steps

- **Next:** [Modules Tutorial](tutorial-modules.md) - Creating reusable modules
- [Standard Library](stdlib/index.md) - See which functions are fallible
