# Error handling patterns

Common patterns for handling errors effectively.

## Pattern 1: default value

Return a default value when an error occurs:

```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		"division by zero" -1 panic
	}
	/
}

fn safe_divide(a:i64 b:i64 default:i64 -- result:i64) {
	rrot divide ok if {
		nip   // Success - return result
	} else {
		drop  // Return default on error
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
fn try_parse(s:str -- value:i64)! {
	// ... parsing logic
}

fn parse_with_fallback(primary:str fallback:str -- value:i64) {
	swap try_parse ok if {
		nip  // Primary succeeded
	} else {
		drop
		try_parse ok if {
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
fn divide(a:i64 b:i64 -- result:i64)! {
	dup 0 == if {
		"division by zero" -1 panic
	}
	/
}

fn succeeded(a:i64 b:i64 -- flag:i64) {
	// The result is on the stack whichever way the call went, so it is dropped under the status
	divide nip ok
}

fn try_all( -- success_count:i64) {
	10 2 succeeded
	10 5 succeeded +
	10 0 succeeded +
}

fn main() {
	try_all print " operations succeeded" print nl  // 2
}
```

## Pattern 4: error chain

Propagate errors through multiple operations:

```qd
fn step1(x:i64 -- y:i64)! {
	dup 0 < if {
		"negative input" 1 panic
	}
	2 *
}

fn step2(x:i64 -- y:i64)! {
	dup 100 > if {
		"overflow" 2 panic
	}
	10 +
}

fn pipeline(x:i64 -- result:i64)! {
	step1 ok if {
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

fn read_file(path:str -- content:str)! {
	io::Read io::open? -> file
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

	file buf size io::read?
	buf swap mem::to_string
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

fn file_open(path:str -- f:File)! {
	dup io::Read io::open switch {
		Ok {
			swap File  // handle first, then path
		}
		_ {
			"open failed" 1 panic
		}
	}
}

fn file_close(f:File -- ) {
	<<handle io::close
}

fn with_file(path:str -- ) {
	file_open switch {
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
fn validate_input(x:i64 -- )! {
	dup 0 < if {
		"negative not allowed" 1 panic
	}
	1000 > if {
		"too large" 2 panic
	}
}

fn process(x:i64 -- result:i64)! {
	dup validate_input ok if {
		dup *  // Safe to process
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

fn retry(max_attempts:i64 -- result:i64)! {
	0 swap 1 for attempt {
		unreliable_op ok if { return }
		drop // the placeholder result of the failed attempt
	}
	"all attempts failed" 1 panic
}
```

## Pattern 9: error context

Add context to errors:

<!-- doccheck: skip pattern sketch: read_file and parse_json stand for a program's own fallible words -->
```qd
fn parse_config(path:str -- cfg:Config)! {
	read_file ok if {
		parse_json ok if {
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
fn process_batch(items:[]i64 -- processed:i64 failed:i64) {
	-> items
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
