# Memory Management

Quadrate provides manual memory management through the `mem` module.

## The mem Module

```qd
use mem
```

The `mem` module provides functions for allocating, freeing, and manipulating memory.

## Allocating Memory

Use `mem::alloc` to allocate bytes:

```qd
use mem

fn main( -- ) {
	1024 mem::alloc -> buf  // Allocate 1024 bytes
	// Use buffer...
	buf mem::free  // Free when done
}
```

Always pair `alloc` with `free`.

## Using defer for Cleanup

Ensure memory is freed with `defer`:

```qd
use mem

fn process( -- ) {
	4096 mem::alloc -> buf
	defer {
		buf mem::free
	}

	// Use buffer...
	// Memory freed automatically when function exits
}
```

## Reading and Writing Memory

### Write byte

```qd
use mem

fn main( -- ) {
	10 mem::alloc -> buf

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

fn main( -- ) {
	8 mem::alloc -> buf

	42 buf 0 mem::set
	buf 0 mem::get print nl  // 42

	buf mem::free
}
```

## Memory and Strings

### String to Buffer

```qd
use mem
use str

fn main( -- ) {
	"Hello" str::len -> len
	"Hello" -> s

	len 1 + mem::alloc -> buf
	s buf len mem::copy_from_str
	0 buf len mem::write_byte  // Null terminate

	buf mem::to_string print nl  // Hello

	buf mem::free
}
```

### Buffer to String

```qd
use mem

fn main( -- ) {
	5 mem::alloc -> buf

	72 buf 0 mem::write_byte   // H
	105 buf 1 mem::write_byte  // i
	33 buf 2 mem::write_byte   // !
	0 buf 3 mem::write_byte    // null

	buf 3 mem::to_string print nl  // Hi!

	buf mem::free
}
```

## Memory Copy

Copy memory between buffers:

```qd
use mem

fn main( -- ) {
	10 mem::alloc -> src
	10 mem::alloc -> dst
	defer { src mem::free dst mem::free }

	// Fill source
	0 10 for i {
		i src i mem::write_byte
	}

	// Copy to destination
	src dst 10 mem::copy

	// Verify
	dst 5 mem::read_byte print nl  // 5
}
```

## Memory Fill

Fill memory with a value:

```qd
use mem

fn main( -- ) {
	100 mem::alloc -> buf
	defer { buf mem::free }

	buf 0 100 mem::fill  // Zero all bytes

	buf 50 mem::read_byte print nl  // 0
}
```

## Memory Compare

Compare memory blocks:

```qd
use mem

fn main( -- ) {
	10 mem::alloc -> a
	10 mem::alloc -> b
	defer { a mem::free b mem::free }

	a 65 10 mem::fill  // Fill a with 'A'
	b 65 10 mem::fill  // Fill b with 'A'

	a b 10 mem::compare print nl  // 0 (equal)

	66 b 5 mem::write_byte  // Change one byte

	a b 10 mem::compare print nl  // Non-zero (different)
}
```

## Resizing Memory

Use `mem::realloc` to resize:

```qd
use mem

fn main( -- ) {
	10 mem::alloc -> buf

	// Grow buffer
	buf 100 mem::realloc -> buf

	// Use larger buffer...

	buf mem::free
}
```

## Common Patterns

### Dynamic String Buffer

```qd
use mem

struct StringBuffer {
	data:ptr
	len:i64
	capacity:i64
}

fn sb_new(capacity:i64 -- sb:ptr) {
	-> capacity
	capacity mem::alloc -> data
	StringBuffer { data = data len = 0 capacity = capacity }
}

fn sb_free(sb:ptr -- ) {
	-> sb
	sb @data mem::free
}

fn sb_append_byte(sb:ptr byte:i64 -- ) {
	-> byte -> sb
	sb @len sb @capacity >= if {
		// Need to grow
		sb @data sb @capacity 2 * mem::realloc sb !data
		sb @capacity 2 * sb !capacity
	}
	byte sb @data sb @len mem::write_byte
	sb @len 1 + sb !len
}
```

### Memory Pool

```qd
use mem

struct Pool {
	memory:ptr
	size:i64
	used:i64
}

fn pool_new(size:i64 -- pool:ptr) {
	-> size
	size mem::alloc -> memory
	Pool { memory = memory size = size used = 0 }
}

fn pool_alloc(pool:ptr bytes:i64 -- ptr:ptr) {
	-> bytes -> pool
	pool @used bytes + pool @size <= if {
		pool @memory pool @used + -> ptr
		pool @used bytes + pool !used
		ptr
	} else {
		0  // Out of memory
	}
}

fn pool_reset(pool:ptr -- ) {
	-> pool
	0 pool !used
}

fn pool_free(pool:ptr -- ) {
	-> pool
	pool @memory mem::free
}
```

## Safety Guidelines

1. **Always free allocated memory** - Use defer to ensure cleanup
2. **Don't use freed memory** - Null pointers after freeing
3. **Don't double-free** - Track ownership carefully
4. **Check allocation success** - alloc can fail
5. **Don't overflow buffers** - Respect allocated sizes

```qd
use mem

fn safe_example( -- ) {
	1024 mem::alloc -> buf
	buf 0 == if {
		"Allocation failed!" print nl
		// Handle error
	}
	defer { buf mem::free }

	// Safe: limited to allocated size
	0 1024 for i {
		0 buf i mem::write_byte
	}
}
```

## What's Next?

Continue to [Practical Examples](../8-examples/calculator.md) to see these concepts in real programs.
