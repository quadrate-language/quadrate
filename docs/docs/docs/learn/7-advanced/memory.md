# Memory management

Quadrate provides manual memory management through the `mem` module.

## The mem module

```qd
use mem
```

The `mem` module provides functions for allocating, freeing, and manipulating memory.

## Allocating memory

Use `mem::alloc` to allocate bytes:

```qd
use mem

fn main() {
	1024 mem::alloc! -> buf  // Allocate 1024 bytes
	// Use buffer...
	buf mem::free  // Free when done
}
```

Always pair `alloc` with `free`.

## Using defer for cleanup

Ensure memory is freed with `defer`:

```qd
use mem

fn process() {
	4096 mem::alloc! -> buf
	defer {
		buf mem::free
	}

	// Use buffer...
	// Memory freed automatically when function exits
}
```

## Reading and writing memory

### Write byte

```qd
use mem

fn main() {
	10 mem::alloc! -> buf

	65 buf 0 mem::set_byte  // Write 'A' at offset 0
	66 buf 1 mem::set_byte  // Write 'B' at offset 1

	buf 0 mem::get_byte print nl  // 65
	buf 1 mem::get_byte print nl  // 66

	buf mem::free
}
```

### Write integers

```qd
use mem

fn main() {
	8 mem::alloc! -> buf

	42 buf 0 mem::set_i64
	buf 0 mem::get_i64 print nl  // 42

	buf mem::free
}
```

## Memory copy

Copy memory between buffers:

```qd
use mem

fn main() {
	10 mem::alloc! -> src
	10 mem::alloc! -> dst
	defer {
		src mem::free dst mem::free
	}

	// Fill source
	0 10 1 for i {
		i src i mem::set_byte
	}

	// Copy to destination
	dst src 10 mem::copy

	// Verify
	dst 5 mem::get_byte print nl  // 5
}
```

## Memory fill

Fill memory with a value:

```qd
use mem

fn main() {
	100 mem::alloc! -> buf
	defer {
		buf mem::free
	}

	0 buf 100 mem::fill  // Zero all bytes

	buf 50 mem::get_byte print nl  // 0
}
```

## Resizing memory

Use `mem::realloc` to resize:

```qd
use mem

fn main() {
	10 mem::alloc! -> buf

	// Grow buffer
	buf 100 mem::realloc! -> buf

	// Use larger buffer...

	buf mem::free
}
```

## Common patterns

### Dynamic string buffer

```qd
use mem

struct StringBuffer {
	data:ptr
	len:i64
	capacity:i64
}

fn sb_new(capacity:i64 -- sb:ptr) {
	-> capacity
	capacity mem::alloc! -> data
	StringBuffer {
		data = data
		len = 0
		capacity = capacity
	}
}

fn sb_free(sb:ptr -- ) {
	-> sb
	sb @data mem::free
}

fn sb_append_byte(sb:ptr byte:i64 -- ) {
	-> byte -> sb
	sb @len sb @capacity >= if {
		// Need to grow
		sb @data sb @capacity 2 * mem::realloc sb.data
		sb @capacity 2 * sb.capacity
	}
	byte sb @data sb @len mem::set_byte
	sb @len 1 + sb.len
}

fn main() {
	16 sb_new -> sb
	defer {
		sb sb_free
	}

	// Append "Hello"
	sb 72 sb_append_byte   // H
	sb 101 sb_append_byte  // e
	sb 108 sb_append_byte  // l
	sb 108 sb_append_byte  // l
	sb 111 sb_append_byte  // o

	// Print length and contents
	"len: " print sb @len print nl
	sb @data sb @len mem::to_string print nl  // Hello
}
```

### Memory pool

```qd
use mem

struct Pool {
	memory:ptr
	size:i64
	used:i64
}

fn pool_new(size:i64 -- pool:ptr) {
	-> size
	size mem::alloc! -> memory
	Pool {
		memory = memory
		size = size
		used = 0
	}
}

fn pool_alloc(pool:ptr bytes:i64 -- offset:i64) {
	-> bytes -> pool
	pool @used bytes + pool @size <= if {
		pool @used -> offset
		pool @used bytes + pool.used
		offset
	} else {
		-1  // Out of memory
	}
}

fn pool_reset(pool:ptr -- ) {
	-> pool
	0 pool.used
}

fn pool_free(pool:ptr -- ) {
	-> pool
	pool @memory mem::free
}

fn main() {
	1024 pool_new -> pool
	defer {
		pool pool_free
	}

	// Allocate from pool (returns offset)
	pool 100 pool_alloc -> off1
	pool 200 pool_alloc -> off2

	"off1: " print off1 print nl          // 0
	"off2: " print off2 print nl          // 100
	"pool used: " print pool @used print nl  // 300

	// Write to allocated regions
	42 pool @memory off1 mem::set_i64
	99 pool @memory off2 mem::set_i64

	// Read back
	pool @memory off1 mem::get_i64 print nl  // 42
	pool @memory off2 mem::get_i64 print nl  // 99

	// Reset pool for reuse
	pool pool_reset
	"after reset: " print pool @used print nl  // 0
}
```

## Safety guidelines

1. **Always free allocated memory** - Use defer to ensure cleanup
2. **Don't use freed memory** - Null pointers after freeing
3. **Don't double-free** - Track ownership carefully
4. **Check allocation success** - alloc can fail
5. **Don't overflow buffers** - Respect allocated sizes

```qd
use mem

fn safe_example() {
	1024 mem::alloc! -> buf
	buf 0 == if {
		"Allocation failed!" print nl
		// Handle error
	}
	defer {
		buf mem::free
	}

	// Safe: limited to allocated size
	0 1024 1 for i {
		0 buf i mem::set_byte
	}
}
```

## What's next?

Continue to [File Processing Examples](../8-examples/file-processing.md) to see these concepts in real programs.
