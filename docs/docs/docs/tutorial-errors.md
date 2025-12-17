# Tutorial: Error Handling

Quadrate has a robust error handling system built around **fallible functions**. This tutorial covers how to define, call, and handle errors in your code.

## Fallible Functions

A fallible function is one that might fail. Mark it with `!` after the signature:

```qd
fn division(a:i64 b:i64 -- result:i64)! {
	// This function can fail
}
```

The `!` tells the compiler this function can return an error.

## Signaling Errors

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

## Calling Fallible Functions

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

## How It Works

After calling a fallible function:

- **Success**: The `if` branch executes, function outputs are on stack
- **Error**: The `else` branch executes, function outputs are NOT on stack

## Using switch Instead of if

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

## Complete Example

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

## Propagating Errors

Sometimes you want to propagate an error up to the caller. Call the fallible function with `!` to crash on error:

```qd
fn divide_and_double(a:i64 b:i64 -- result:i64)! {
	division!  // Propagates error if division fails
	2 *
}
```

If `division` fails, `divide_and_double` will also fail with the same error.

## Standard Library Errors

Many standard library functions are fallible. Common examples:

### String Operations

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
		buf_size mem::alloc -> buf

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

## The `defer` Statement

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
		size mem::alloc -> buf
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

## Error Handling Patterns

### Pattern 1: Handle and Continue

```qd
fn safe_divide(a:i64 b:i64 -- result:i64) {
	division if {
		// Success - return result
	} else {
		0  // Return default value on error
	}
}
```

### Pattern 2: Early Return on Error

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

### Pattern 3: Collect Multiple Results

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
2. **Signal panics** with `"message" code panic`
3. **Handle errors** with `if { success } else { error }`
4. **Skip error checks** by calling with `function!`
5. **Use `defer`** for cleanup that runs regardless of errors
6. **The compiler enforces** error handling - you can't ignore errors

## Next Steps

- [Standard Library](stdlib/index.md) - See which functions are fallible
- [Examples](https://git.sr.ht/~klahr/quadrate/tree/master/item/examples) - Real-world error handling
