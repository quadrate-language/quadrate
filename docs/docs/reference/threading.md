# Threading

Built-in operations for concurrent execution.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `spawn` | `(fn -- thread)` | Spawn a new thread |
| `wait` | `(thread --)` | Wait for thread completion |
| `detach` | `(thread --)` | Detach a thread |

---

## Thread management

### spawn

Spawns a new thread to execute a function.

**Signature:** `(fn -- thread)`

```qd
fn worker() {
	"Working..." print nl
}

fn main() {
	&worker spawn -> t
	t wait
}
```

### wait

Waits for a thread to complete.

**Signature:** `(thread --)`

```qd
&worker spawn -> t
// Do other work...
t wait  // Block until worker finishes
```

### detach

Detaches a thread, allowing it to run independently.

**Signature:** `(thread --)`

```qd
&worker spawn -> t
t detach  // Thread runs on its own
// Main program continues
```

---

## Examples

### Multiple workers

```qd
fn worker() {
	"Working" print nl
}

fn main() {
	3 make<i64> -> threads

	0 3 1 for i {
		threads i &worker spawn set
	}

	// Wait for all
	0 3 1 for i {
		threads i nth wait
	}

	"All done" print nl
}
```

> **Note:** Spawned threads start with their own empty stack. Use channels (`thread::chan_buffered!`) to pass data between threads. See [thread](../stdlib/thread.md) for details.

### Fire and forget

```qd
fn background_task() {
	// Long running work...
}

fn main() {
	&background_task spawn detach
	"Started background task" print nl
	// Main continues without waiting
}
```

---

## Considerations

### Thread safety

- Each thread has its own stack
- Shared data requires synchronization
- Be careful with mutable state

### Best practices

1. **Join threads** - Use `wait` to ensure completion
2. **Handle errors** - Threads can fail independently
3. **Limit concurrency** - Too many threads hurts performance
4. **Avoid shared mutable state** - Pass data to threads at creation
