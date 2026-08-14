# Defer

The `defer` statement schedules code to run when the function exits.

## Basic usage

```qd
fn main() {
	"start" print nl

	defer {
		"cleanup" print nl
	}

	"middle" print nl
}
// Output:
// start
// middle
// cleanup
```

The deferred code runs after the function body completes.

## Guaranteed cleanup

Defers run even when errors occur:

```qd
use io

fn process_file(path:str -- )! {
	-> path  // bind parameter
	path io::Read io::open! -> file
	defer {
		// Always runs, even if do_something fails
		file io::close
	}

	// Even if this fails, file gets closed
	file do_something!
}
```

## LIFO order

Multiple defers execute in reverse order (last in, first out):

```qd
fn main() {
	defer {
		"first" print nl
	}
	defer {
		"second" print nl
	}
	defer {
		"third" print nl
	}
}
// Output:
// third
// second
// first
```

## Resource management

Use defer to manage resources:

```qd
use mem

fn with_buffer() {
	1024 mem::alloc! -> buf
	defer {
		buf mem::free
	}

	// Use buffer...
	// It will be freed when function exits
}
```

## Multiple resources

```qd
use io
use mem

fn copy_file(src:str dst:str -- )! {
	-> dst -> src  // bind parameters

	src io::Read io::open! -> src_file
	defer { src_file io::close }

	dst io::Write io::open! -> dst_file
	defer { dst_file io::close }

	4096 mem::alloc! -> buf
	defer { buf mem::free }

	// Copy data...
}
```

Resources are released in reverse order of acquisition.

## Defer with variables

Defers capture the current value of variables:

```qd
fn main() {
	0 -> x

	defer {
		"x = " print x print nl
	}

	42 -> x  // Changes x

	// Defer uses x's value at time of execution (42)
}
// Output: x = 42
```

## Common patterns

### Transaction

```qd
fn transaction()! {
	begin_transaction
	defer {
		is_error if {
			rollback
		} else {
			commit
		}
	}

	// Perform operations...
}
```

### Logging

```qd
fn traced_operation(name:str -- ) {
	-> name  // bind parameter
	"Entering " print name print nl
	defer {
		"Exiting " print name print nl
	}

	// Do work...
}
```

## Defer vs finally

Unlike try/finally in other languages, defer is simpler:

- No special syntax for try blocks
- Just add defer where you acquire a resource
- Cleanup is guaranteed

```qd
// Other languages:
// try {
//     file = open(...)
//     ...
// } finally {
//     file.close()
// }

// Quadrate:
fn example() {
	"file.txt" open_file -> file
	defer {
		file close_file
	}
	// ... use file
}
```

## Nested functions

Defers only apply to their own function:

```qd
fn outer() {
	defer {
		"outer cleanup" print nl
	}

	inner  // inner's defers run during inner

	"outer continues" print nl
}

fn inner() {
	defer {
		"inner cleanup" print nl
	}
	"inner work" print nl
}

fn main() {
	outer
}
// Output:
// inner work
// inner cleanup
// outer continues
// outer cleanup
```

## What's next?

Learn about [Function pointers](function-pointers.md) for passing behaviour through your program.
