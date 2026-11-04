# Threading

Threads come from the [thread](../stdlib/thread.md) module. A thread runs a function with no
inputs and no outputs, on a stack of its own.

## Overview

| Function | Signature | Description |
|----------|-----------|-------------|
| `thread::spawn` | `(func:fn(--) -- t:Thread)!` | Start a thread running `func` |
| `join` | `(t:Thread -- )!` | Wait for the thread to finish |
| `detach` | `(t:Thread -- )!` | Let the thread run on its own; it cannot be joined after |
| `is_alive` | `(t:Thread -- alive:i64)` | 1 while the thread is running |

`spawn`, `join` and `detach` are fallible: a thread that cannot be created or joined is an
error, handled like any other.

---

## Thread management

### spawn and join

```qd
use thread

fn worker() {
	"Working..." print nl
}

fn main() {
	&worker thread::spawn! -> t
	// Do other work...
	t join!  // Block until worker finishes
}
```

### detach

```qd
use thread

fn worker() {
	"Working..." print nl
}

fn main() {
	&worker thread::spawn! -> t
	t detach!  // The thread runs on its own; main does not wait for it
	10 thread::sleep
}
```

---

## Examples

### Multiple workers

```qd
use thread

fn worker() {
	"Working" print nl
}

fn main() {
	[]thread::Thread -> threads
	3 fn { threads &worker thread::spawn! append -> threads } times
	threads fn { join! } each
	"All done" print nl
}
```

> **Note:** Spawned threads start with their own empty stack. Use channels (`thread::chan_buffered!`) to pass data between threads, and `thread::WaitGroup` to wait for a group. See [thread](../stdlib/thread.md) for details.

---

## Considerations

### Thread safety

- Each thread has its own stack
- Shared data requires synchronization (`thread::mutex_new!`, `lock!`, `unlock!`)
- Be careful with mutable state

### Best practices

1. **Join threads** - Use `join!` to ensure completion
2. **Handle errors** - Threads can fail independently
3. **Limit concurrency** - Too many threads hurts performance (`thread::cpu_count`)
4. **Avoid shared mutable state** - Pass data to threads through channels
