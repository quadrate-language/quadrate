# Error Handling Patterns

Common patterns for handling errors effectively.

## Pattern 1: Default Value

Return a default value when an error occurs:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		drop drop 0
		"division by zero" -1 error
	}
	/
}

fn safe_divide(a:i64 b:i64 default:i64 -- result:i64) {
	-> default
	divide if {
		// Success - return result
	} else {
		drop
		default  // Return default on error
	}
}

fn main( -- ) {
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
		drop
		fallback try_parse if {
			// Fallback succeeded
		} else {
			drop
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
		drop drop 0
		"division by zero" -1 error
	}
	/
}

fn try_all( -- success_count:i64) {
	0 -> count

	10 2 divide if {
		drop  // Discard result
		count 1 + -> count
	} else {
		drop
	}

	10 5 divide if {
		drop
		count 1 + -> count
	} else {
		drop
	}

	10 0 divide if {
		drop
		count 1 + -> count
	} else {
		drop
	}

	count
}

fn main( -- ) {
	try_all print " operations succeeded" print nl  // 2
}
```

## Pattern 4: Error Chain

Propagate errors through multiple operations:

```qd
fn step1(x:i64 -- y:i64)! {
	x 0 < if {
		0
		"negative input" 1 error
	}
	x 2 *
}

fn step2(x:i64 -- y:i64)! {
	x 100 > if {
		0
		"overflow" 2 error
	}
	x 10 +
}

fn pipeline(x:i64 -- result:i64)! {
	-> x
	x step1 if {
		step2 if {
			// Both succeeded
		} else {
			drop 0
			"step2 failed" 2 error
		}
	} else {
		drop 0
		"step1 failed" 1 error
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
		defer { file io::close }  // Always runs

		4096 -> size
		size mem::alloc -> buf
		defer { buf mem::free }   // Always runs

		file buf size io::read if {
			-> bytes_read
			buf bytes_read mem::to_string
		} else {
			drop
			""
			"read failed" 1 error
		}
	} else {
		drop
		""
		"open failed" 1 error
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
	path io::ReadOnly io::open if {
		-> handle
		File { handle = handle path = path }
	} else {
		drop
		0
		"open failed" 1 error
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
		defer { f file_close }
		// Use file...
	} else {
		drop
	}
}
```

## Pattern 7: Validation

Validate before processing:

```qd
fn validate_input(x:i64 -- )! {
	-> x
	x 0 < if {
		"negative not allowed" 1 error
	}
	x 1000 > if {
		"too large" 2 error
	}
}

fn process(x:i64 -- result:i64)! {
	-> x
	x validate_input if {
		x dup *  // Safe to process
	} else {
		drop 0
		"validation failed" 1 error
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

	success not attempts max_attempts < and while {
		unreliable_op if {
			-> last_result
			1 -> success
		} else {
			drop
		}
		attempts 1 + -> attempts
	}

	success if {
		last_result
	} else {
		0
		"all attempts failed" 1 error
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
			drop 0
			"invalid JSON in config" 2 error
		}
	} else {
		drop 0
		"failed to read config file" 1 error
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

	items iter for item {
		item process_item if {
			drop
			processed 1 + -> processed
		} else {
			drop
			"Failed: " print item print nl
			failed 1 + -> failed
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
