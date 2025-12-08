# Threading

Built-in operations for concurrent execution.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `spawn` | `( fn -- thread )` | Spawn a new thread |
| `wait` | `( thread -- )` | Wait for thread completion |
| `detach` | `( thread -- )` | Detach a thread |

---

## Thread Management

### spawn

Spawns a new thread to execute a function.

**Signature:** `( fn -- thread )`

```qd
fn worker( -- ) {
	"Working..." print nl
}

fn main( -- ) {
	&worker spawn -> t
	t wait
}
```

### wait

Waits for a thread to complete.

**Signature:** `( thread -- )`

```qd
&worker spawn -> t
// Do other work...
t wait  // Block until worker finishes
```

### detach

Detaches a thread, allowing it to run independently.

**Signature:** `( thread -- )`

```qd
&worker spawn -> t
t detach  // Thread runs on its own
// Main program continues
```

---

## Examples

### Multiple Workers

```qd
fn task(id:i64 -- ) {
	-> id
	"Task " print id print " running" print nl
}

fn main( -- ) {
	3 make<ptr> -> threads

	0 3 1 for i {
		&task spawn threads i set
	}

	// Wait for all
	0 3 1 for i {
		threads i nth wait
	}

	"All done" print nl
}
```

### Fire and Forget

```qd
fn background_task( -- ) {
	// Long running work...
}

fn main( -- ) {
	&background_task spawn detach
	"Started background task" print nl
	// Main continues without waiting
}
```

---

## Considerations

### Thread Safety

- Each thread has its own stack
- Shared data requires synchronization
- Be careful with mutable state

### Best Practices

1. **Join threads** - Use `wait` to ensure completion
2. **Handle errors** - Threads can fail independently
3. **Limit concurrency** - Too many threads hurts performance
4. **Avoid shared mutable state** - Pass data to threads at creation
