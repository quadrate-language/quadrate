# Error handling patterns

Common patterns for handling errors effectively.

## Pattern 1: default value

Return a default value when an error occurs:

```qd
def divide(a:i64 b:i64 -- result:i64)! {
	b 0 == if {
		"division by zero" -1 panic
	}
	a b /
}

def safe_divide(a:i64 b:i64 default:i64 -- result:i64) {
	a b divide ok if {
		// Success - return result
	} else {
		drop
		default  // Return default on error
	}
}

fn main() {
	10 0 -1 safe_divide print nl  // -1
	10 2 -1 safe_divide print nl  // 5
}
```

## Pattern 2: try multiple options

Try alternatives when first option fails:

<!-- doccheck: skip pattern sketch: try_parse stands for a program's own fallible word -->
```qd
def try_parse(s:str -- value:i64)! {
	// ... parsing logic
}

def parse_with_fallback(primary:str fallback:str -- value:i64) {
	primary try_parse ok if {
		// Primary succeeded
	} else {
		drop
		fallback try_parse ok if {
			// Fallback succeeded
		} else {
			drop
			0  // Both failed, use default
		}
	}
}
```

## Pattern 3: collect results

Continue despite errors, collect successes:

```qd
def divide(a:i64 b:i64 -- result:i64)! {
	b 0 == if {
		"division by zero" -1 panic
	}
	a b /
}

fn try_all( -- success_count:i64) {
	0 -> count

	// The result is on the stack whichever way the call went, so it is dropped after the `if`
	10 2 divide ok if {
		count ++ -> count
	} drop

	10 5 divide ok if {
		count ++ -> count
	} drop

	10 0 divide ok if {
		count ++ -> count
	} drop

	count
}

fn main() {
	try_all print " operations succeeded" print nl  // 2
}
```

## Pattern 4: error chain

Propagate errors through multiple operations:

```qd
def step1(x:i64 -- y:i64)! {
	x 0 < if {
		"negative input" 1 panic
	}
	x 2 *
}

def step2(x:i64 -- y:i64)! {
	x 100 > if {
		"overflow" 2 panic
	}
	x 10 +
}

def pipeline(x:i64 -- result:i64)! {
	x step1 ok if {
		step2 ok if {
			// Both succeeded
		} else {
			"step2 failed" 2 panic
		}
	} else {
		"step1 failed" 1 panic
	}
}
```

## Pattern 5: cleanup with defer

Use `defer` for guaranteed cleanup:

```qd
use io
use mem

def read_file(path:str -- content:str)! {
	path io::Read io::open? -> file
	defer {
		// Always runs
		file io::close
	}

	4096 -> size
	size mem::alloc? -> buf
	defer {
		// Always runs
		buf mem::free
	}

	file buf size io::read? -> bytes_read
	buf bytes_read mem::to_string
}
```

Defers execute in LIFO order (last in, first out).

## Pattern 6: resource wrapper

Wrap resource operations:

```qd
use io

struct File {
	handle:ptr
	path:str
}

def file_open(path:str -- f:File)! {
	path io::Read io::open switch {
		Ok {
			-> handle  // bind handle
			File {
				handle = handle
				path = path
			}
		}
		_ {
			"open failed" 1 panic
		}
	}
}

def file_close(f:File -- ) {
	f <<handle io::close
}

def with_file(path:str -- ) {
	path file_open switch {
		Ok {
			-> f  // bind file
			defer {
				f file_close
			}
			// Use file...
		}
		_ {
			drop
			"could not open" print nl
		}
	}
}
```

!!! note "`defer` runs only if it is reached"
    A `defer` is armed when control reaches the statement, so one written inside a `switch` arm
    runs only when that arm is taken — the cleanup above never fires on the failure path, where
    no file was opened. The same holds for a `?` that propagates out before the `defer`.

## Pattern 7: validation

Validate before processing:

```qd
def validate_input(x:i64 -- )! {
	x 0 < if {
		"negative not allowed" 1 panic
	}
	x 1000 > if {
		"too large" 2 panic
	}
}

def process(x:i64 -- result:i64)! {
	x validate_input ok if {
		x x *  // Safe to process
	} else {
		"validation failed" 1 panic
	}
}
```

## Pattern 8: retry logic

Retry on transient failures:

<!-- doccheck: skip pattern sketch: unreliable_op stands for a program's own fallible word -->
```qd
fn unreliable_op( -- result:i64)! {
	// Might fail sometimes
}

def retry(max_attempts:i64 -- result:i64)! {
	0 -> attempts
	0 -> success
	0 -> last_result

	loop {
		success 0 == attempts max_attempts < and 0 == if { break }
		unreliable_op ok if {
			-> last_result
			1 -> success
		} else {
			drop // the placeholder result of the failed attempt
		}
		attempts ++ -> attempts
	}

	success if {
		last_result
	} else {
		"all attempts failed" 1 panic
	}
}
```

## Pattern 9: error context

Add context to errors:

<!-- doccheck: skip pattern sketch: read_file and parse_json stand for a program's own fallible words -->
```qd
def parse_config(path:str -- cfg:Config)! {
	path read_file ok if {
		-> content  // bind content
		content parse_json ok if {
			// Success: the Config is on the stack
		} else {
			drop
			"invalid JSON in config" 2 panic
		}
	} else {
		drop
		"failed to read config file" 1 panic
	}
}
```

## Pattern 10: batch processing

Process items, log failures:

<!-- doccheck: skip pattern sketch: process_item stands for a program's own fallible word -->
```qd
def process_batch(items:[]i64 -- processed:i64 failed:i64) {
	0 -> processed
	0 -> failed

	0 items len 1 for i {
		items i nth process_item ok if {
			drop
			processed ++ -> processed
		} else {
			drop // the placeholder result
			"Failed: " print items i nth print nl
			failed ++ -> failed
		}
	}

	processed failed
}
```

## Best practices

1. **Always handle errors** - Don't just `drop` without logging
2. **Use meaningful messages** - Help debugging
3. **Clean up resources** - Use `defer` for guaranteed cleanup
4. **Propagate when appropriate** - Use `?` to pass an error to the caller; `!` aborts the program
5. **Fail fast** - Validate early, fail early
6. **Consider recovery** - Not every error should crash

## What's next?

Learn about [Advanced Topics](../7-advanced/defer.md) like defer and memory management.
