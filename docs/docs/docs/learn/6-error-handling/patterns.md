# Error Handling Patterns

Common patterns for handling errors effectively.

## Pattern 1: Default Value

Return a default value when an error occurs:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop2
		"division by zero" -1 panic
	}
	/
}

fn safe_divide(a:i64 b:i64 default:i64 -- result:i64) {
	-> default
	divide if {
		// Success - return result
	} else {
		default  // Return default on error
	}
}

fn main() {
	10 0 -1 safe_divide print nl  // -1
	10 2 -1 safe_divide print nl  // 5
}
```

## Pattern 2: Try Multiple Options

Try alternatives when first option fails:

```qd
fn try_parse(s:str -- value:i64)! {
	// ... parsing logic
}

fn parse_with_fallback(primary:str fallback:str -- value:i64) {
	-> fallback -> primary
	primary try_parse if {
		// Primary succeeded
	} else {
		fallback try_parse if {
			// Fallback succeeded
		} else {
			0  // Both failed, use default
		}
	}
}
```

## Pattern 3: Collect Results

Continue despite errors, collect successes:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop2
		"division by zero" -1 panic
	}
	/
}

fn try_all( -- success_count:i64) {
	0 -> count

	10 2 divide if {
		drop  // Discard result
		count inc -> count
	}

	10 5 divide if {
		drop
		count inc -> count
	}

	10 0 divide if {
		drop
		count inc -> count
	}

	count
}

fn main() {
	try_all print " operations succeeded" print nl  // 2
}
```

## Pattern 4: Error Chain

Propagate errors through multiple operations:

```qd
fn step1(x:i64 -- y:i64)! {
	x 0 < if {
		"negative input" 1 panic
	}
	x 2 *
}

fn step2(x:i64 -- y:i64)! {
	x 100 > if {
		"overflow" 2 panic
	}
	x 10 +
}

fn pipeline(x:i64 -- result:i64)! {
	-> x
	x step1 if {
		step2 if {
			// Both succeeded
		} else {
			"step2 failed" 2 panic
		}
	} else {
		"step1 failed" 1 panic
	}
}
```

## Pattern 5: Cleanup with defer

Use `defer` for guaranteed cleanup:

```qd
use io
use mem

fn read_file(path:str -- content:str)! {
	-> path

	path io::ReadOnly io::open if {
		-> file
		defer {
			// Always runs
			file io::close
		}

		4096 -> size
		size mem::alloc -> buf
		defer {
			// Always runs
			buf mem::free
		}

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

Defers execute in LIFO order (last in, first out).

## Pattern 6: Resource Wrapper

Wrap resource operations:

```qd
struct File {
	handle:i64
	path:str
}

fn file_open(path:str -- f:ptr)! {
	-> path
	path io::Read io::open if {
		-> handle
		File {
			handle = handle
			path = path
		}
	} else {
		"open failed" 1 panic
	}
}

fn file_close(f:ptr -- ) {
	-> f
	f @handle io::close
}

fn with_file(path:str -- ) {
	-> path
	path file_open if {
		-> f
		defer {
			f file_close
		}
		// Use file...
	}
}
```

## Pattern 7: Validation

Validate before processing:

```qd
fn validate_input(x:i64 -- )! {
	-> x
	x 0 < if {
		"negative not allowed" 1 panic
	}
	x 1000 > if {
		"too large" 2 panic
	}
}

fn process(x:i64 -- result:i64)! {
	-> x
	x validate_input if {
		x dup *  // Safe to process
	} else {
		"validation failed" 1 panic
	}
}
```

## Pattern 8: Retry Logic

Retry on transient failures:

```qd
fn unreliable_op( -- result:i64)! {
	// Might fail sometimes
}

fn retry(max_attempts:i64 -- result:i64)! {
	-> max_attempts
	0 -> attempts
	0 -> success
	0 -> last_result

	success 0 == attempts max_attempts < and while {
		unreliable_op if {
			-> last_result
			1 -> success
		}
		attempts inc -> attempts
		success 0 == attempts max_attempts < and
	}

	success if {
		last_result
	} else {
		"all attempts failed" 1 panic
	}
}
```

## Pattern 9: Error Context

Add context to errors:

```qd
fn parse_config(path:str -- cfg:ptr)! {
	-> path
	path read_file if {
		-> content
		content parse_json if {
			// Success
		} else {
			"invalid JSON in config" 2 panic
		}
	} else {
		"failed to read config file" 1 panic
	}
}
```

## Pattern 10: Batch Processing

Process items, log failures:

```qd
fn process_batch(items:ptr -- processed:i64 failed:i64) {
	-> items
	0 -> processed
	0 -> failed

	0 items len 1 for i {
		items i nth process_item if {
			drop
			processed inc -> processed
		} else {
			"Failed: " print items i nth print nl
			failed inc -> failed
		}
	}

	processed failed
}
```

## Best Practices

1. **Always handle errors** - Don't just `drop` without logging
2. **Use meaningful messages** - Help debugging
3. **Clean up resources** - Use `defer` for guaranteed cleanup
4. **Propagate when appropriate** - Use `!` to bubble up errors
5. **Fail fast** - Validate early, fail early
6. **Consider recovery** - Not every error should crash

## What's Next?

Learn about [Advanced Topics](../7-advanced/defer.md) like defer, context, and memory management.
