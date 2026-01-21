# `use` thread

Thread module - threading primitives using C11 threads.

Provides:
- Thread: spawn, join, detach threads
- Mutex: mutual exclusion locks
- Channel: Go-style channels for thread communication
- WaitGroup: wait for multiple threads to complete

## Example

    use thread

    fn worker() {
        "Hello from thread!" print nl
    }

    fn main() {
        // Spawn and join a thread
        &worker thread::spawn! -> t
        t join!

        // Mutex for synchronization
        thread::mutex_new! -> m
        m lock!
        m unlock!
        m free

        // Channel for communication
        10 thread::chan_buffered! -> ch
        ch 42 send!
        ch recv! -> val
        ch free

        // WaitGroup to wait for multiple threads
        thread::wg_new! -> wg
        wg 2 add
        wg done
        wg done
        wg wait!
        wg free
    }

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrChannel` | `6` | Error: Channel operation failed. |
| `ErrClosed` | `7` | Error: Channel is closed. |
| `ErrCreate` | `2` | Error: Failed to create thread. |
| `ErrDetach` | `4` | Error: Failed to detach thread. |
| `ErrJoin` | `3` | Error: Failed to join thread. |
| `ErrMutex` | `5` | Error: Mutex operation failed. |
| `ErrTimeout` | `8` | Error: Operation timed out. |

## Functions

### `fn` sleep

Sleep the current thread for the specified duration in milliseconds. 

**Signature:** `(ms:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ms` | `i64` | Sleep duration in milliseconds |

**Example:**

```qd
1000 thread::sleep  // Sleep 1 second
```
## Channel

A channel for thread communication.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `handle` | `ptr` |  |

### Constructors

#### `fn` chan_buffered

Create a new buffered channel. 

**Signature:** `(capacity:i64 -- ch:Channel)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `capacity` | `i64` | Buffer size |

| Output | Type | Description |
|--------|------|-------------|
| `ch` | `Channel` | New channel handle |

| Error | Description |
|-------|-------------|
| `thread::ErrChannel` | Failed to create channel |

**Example:**

```qd
10 thread::chan_buffered!  // ch
```
---

#### `fn` chan_new

Create a new unbuffered (synchronous) channel. 

**Signature:** `( -- ch:Channel)!`

| Output | Type | Description |
|--------|------|-------------|
| `ch` | `Channel` | New channel handle |

| Error | Description |
|-------|-------------|
| `thread::ErrChannel` | Failed to create channel |

**Example:**

```qd
thread::chan_new!  // ch
```

### Methods

#### `fn` cap

Get channel capacity. 

**Signature:** `(ch:Channel) cap( -- c:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `cap` | `i64` | Buffer capacity (0 for unbuffered) |

**Example:**

```qd
ch cap  // c
```
---

#### `fn` close

Close the channel. 

**Signature:** `(ch:Channel) close( -- )`

**Example:**

```qd
ch close
```
---

#### `fn` free

Free the channel. 

**Signature:** `(ch:Channel) free( -- )`

**Example:**

```qd
ch free
```
---

#### `fn` is_closed

Check if the channel is closed. 

**Signature:** `(ch:Channel) is_closed( -- closed:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `closed` | `i64` | 1 if closed, 0 if open |

**Example:**

```qd
ch is_closed  // done
```
---

#### `fn` len

Get number of values in channel buffer. 

**Signature:** `(ch:Channel) len( -- n:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `n` | `i64` | Number of buffered values |

**Example:**

```qd
ch len  // n
```
---

#### `fn` recv

Receive a value from the channel. 

**Signature:** `(ch:Channel) recv( -- val:i64)!`

| Output | Type | Description |
|--------|------|-------------|
| `val` | `i64` | Received value |

| Error | Description |
|-------|-------------|
| `thread::ErrClosed` | Channel closed with no values |

**Example:**

```qd
ch recv!  // val
```
---

#### `fn` send

Send a value on the channel. 

**Signature:** `(ch:Channel) send(val:i64 -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `val` | `i64` | Value to send |

| Error | Description |
|-------|-------------|
| `thread::ErrClosed` | Cannot send on closed channel |

**Example:**

```qd
ch 42 send!
```
---

#### `fn` try_recv

Try to receive without blocking. 

**Signature:** `(ch:Channel) try_recv( -- val:i64 success:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `val` | `i64` | Received value (0 if failed) |
| `success` | `i64` | 1 if received, 0 if would block |

**Example:**

```qd
ch try_recv -> success  // val
```
---

#### `fn` try_send

Try to send without blocking. 

**Signature:** `(ch:Channel) try_send(val:i64 -- success:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `val` | `i64` | Value to send |

| Output | Type | Description |
|--------|------|-------------|
| `success` | `i64` | 1 if sent, 0 if would block |

**Example:**

```qd
ch 42 try_send  // sent
```

## Mutex

A mutual exclusion lock.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `handle` | `ptr` |  |

### Constructors

#### `fn` mutex_new

Create a new mutex. 

**Signature:** `( -- m:Mutex)!`

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mutex` | New mutex handle |

| Error | Description |
|-------|-------------|
| `thread::ErrMutex` | Failed to create mutex |

**Example:**

```qd
thread::mutex_new!  // m
```

### Methods

#### `fn` free

Free the mutex. 

**Signature:** `(m:Mutex) free( -- )`

**Example:**

```qd
m free
```
---

#### `fn` lock

Lock the mutex. 

**Signature:** `(m:Mutex) lock( -- )!`

**Example:**

```qd
m lock!
```
---

#### `fn` try_lock

Try to lock the mutex without blocking. 

**Signature:** `(m:Mutex) try_lock( -- success:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `success` | `i64` | 1 if locked, 0 if already held |

**Example:**

```qd
m try_lock  // got
```
---

#### `fn` unlock

Unlock the mutex. 

**Signature:** `(m:Mutex) unlock( -- )!`

**Example:**

```qd
m unlock!
```

## Thread

A thread handle.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `handle` | `ptr` |  |

### Constructors

#### `fn` spawn

Spawn a new thread running the given function. 

**Signature:** `(func:ptr -- t:Thread)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `func` | `ptr` | Function pointer (use &function_name) |

| Output | Type | Description |
|--------|------|-------------|
| `t` | `Thread` | Thread handle |

| Error | Description |
|-------|-------------|
| `thread::ErrCreate` | Failed to create thread |

**Example:**

```qd
&worker thread::spawn!  // t
```

### Methods

#### `fn` detach

Detach the thread. 

**Signature:** `(t:Thread) detach( -- )!`

**Example:**

```qd
t detach!
```
---

#### `fn` is_alive

Check if the thread is still alive. 

**Signature:** `(t:Thread) is_alive( -- alive:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `alive` | `i64` | 1 if running, 0 if completed |

**Example:**

```qd
t is_alive  // running
```
---

#### `fn` join

Wait for the thread to complete. 

**Signature:** `(t:Thread) join( -- )!`

**Example:**

```qd
t join!
```

## WaitGroup

A WaitGroup waits for a collection of threads to finish.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `handle` | `ptr` |  |

### Constructors

#### `fn` wg_new

Create a new WaitGroup. 

**Signature:** `( -- wg:WaitGroup)!`

| Output | Type | Description |
|--------|------|-------------|
| `wg` | `WaitGroup` | New WaitGroup handle |

| Error | Description |
|-------|-------------|
| `thread::ErrCreate` | Failed to create WaitGroup |

**Example:**

```qd
thread::wg_new!  // wg
```

### Methods

#### `fn` add

Add to the WaitGroup counter. 

**Signature:** `(wg:WaitGroup) add(n:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Number to add (usually 1 per thread) |

**Example:**

```qd
wg 2 add
```
---

#### `fn` done

Mark one unit of work as done. 

**Signature:** `(wg:WaitGroup) done( -- )`

**Example:**

```qd
wg done
```
---

#### `fn` free

Free the WaitGroup. 

**Signature:** `(wg:WaitGroup) free( -- )`

**Example:**

```qd
wg free
```
---

#### `fn` wait

Wait for all threads to complete. 

**Signature:** `(wg:WaitGroup) wait( -- )!`

| Error | Description |
|-------|-------------|
| `thread::ErrCreate` | Invalid WaitGroup |

**Example:**

```qd
wg wait!
```

