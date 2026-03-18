# `use` signal

Unix signal handling with polling-based API.

Signals are caught and stored as flags. Use pending() to check
if a signal was received, and clear() to reset the flag.

**Example:**

```qd
use signal

fn main() {
    signal::SigInt signal::trap
    "Running. Press Ctrl+C to stop." print nl

    loop {
        signal::SigInt signal::pending if {
            "Shutting down" print nl
            signal::SigInt signal::clear
            break
        }
    }
}
```

## Functions

### `fn` clear

Clear the pending flag for a signal.

**Signature:** `(signum:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number |

**Example:**

```qd
signal::SigInt signal::clear
```
---

### `fn` ignore

Ignore the specified signal completely.

**Signature:** `(signum:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number |

**Example:**

```qd
signal::SigPipe signal::ignore
```
---

### `fn` kill

Send a signal to another process.

**Signature:** `(pid:i64 signum:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `pid` | `i64` | Process ID to signal |
| `signum` | `i64` | Signal number to send |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 0 on success, -1 on error |

**Example:**

```qd
1234 signal::SigTerm signal::kill drop
```
---

### `fn` pending

Check if a signal is pending (received but not cleared). Returns 1 if pending, 0 otherwise. Does not clear the flag.

**Signature:** `(signum:i64 -- flag:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number |

| Output | Type | Description |
|--------|------|-------------|
| `flag` | `i64` | 1 if pending, 0 otherwise |

**Example:**

```qd
signal::SigInt signal::pending
```
---

### `fn` raise

Send a signal to the current process.

**Signature:** `(signum:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number to send |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 0 on success, -1 on error |

**Example:**

```qd
signal::SigUsr1 signal::raise drop
```
---

### `fn` reset

Reset signal to default behavior.

**Signature:** `(signum:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number |

**Example:**

```qd
signal::SigInt signal::reset
```
---

### `fn` SigAbrt

Abort signal.

**Signature:** `( -- val:i64)`
---

### `fn` SigAlrm

Alarm clock.

**Signature:** `( -- val:i64)`
---

### `fn` SigChld

Child process stopped or terminated.

**Signature:** `( -- val:i64)`
---

### `fn` SigCont

Continue if stopped.

**Signature:** `( -- val:i64)`
---

### `fn` SigFpe

Floating point exception.

**Signature:** `( -- val:i64)`
---

### `fn` SigHup

Hangup signal (terminal closed).

**Signature:** `( -- val:i64)`
---

### `fn` SigIll

Illegal instruction.

**Signature:** `( -- val:i64)`
---

### `fn` SigInt

Interrupt signal (Ctrl+C).

**Signature:** `( -- val:i64)`
---

### `fn` SigKill

Kill signal (cannot be caught).

**Signature:** `( -- val:i64)`
---

### `fn` SigPipe

Broken pipe.

**Signature:** `( -- val:i64)`
---

### `fn` SigQuit

Quit signal (Ctrl+\).

**Signature:** `( -- val:i64)`
---

### `fn` SigSegv

Segmentation fault.

**Signature:** `( -- val:i64)`
---

### `fn` SigStop

Stop signal (cannot be caught).

**Signature:** `( -- val:i64)`
---

### `fn` SigTerm

Termination signal.

**Signature:** `( -- val:i64)`
---

### `fn` SigTstp

Terminal stop (Ctrl+Z).

**Signature:** `( -- val:i64)`
---

### `fn` SigTtin

Background read from terminal.

**Signature:** `( -- val:i64)`
---

### `fn` SigTtou

Background write to terminal.

**Signature:** `( -- val:i64)`
---

### `fn` SigUsr1

User-defined signal 1.

**Signature:** `( -- val:i64)`
---

### `fn` SigUsr2

User-defined signal 2.

**Signature:** `( -- val:i64)`
---

### `fn` trap

Install a handler to catch the specified signal. After trapping, the signal sets a pending flag instead of causing the default action (e.g., termination).

**Signature:** `(signum:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number (use signal::SigInt, etc.) |

**Example:**

```qd
signal::SigInt signal::trap
```
---

### `fn` wait

Block until any trapped signal is received. Returns the signal number that was received.

**Signature:** `( -- signum:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `signum` | `i64` | The signal that was received |

**Example:**

```qd
signal::wait  // sig
```
