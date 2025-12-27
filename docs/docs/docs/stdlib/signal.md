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

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `SigAbrt` | `6` | Abort signal. |
| `SigAlrm` | `14` | Alarm clock. |
| `SigChld` | `17` | Child process stopped or terminated. |
| `SigCont` | `18` | Continue if stopped. |
| `SigFpe` | `8` | Floating point exception. |
| `SigHup` | `1` | Hangup signal (terminal closed). |
| `SigIll` | `4` | Illegal instruction. |
| `SigInt` | `2` | Interrupt signal (Ctrl+C). |
| `SigKill` | `9` | Kill signal (cannot be caught). |
| `SigPipe` | `13` | Broken pipe. |
| `SigQuit` | `3` | Quit signal (Ctrl+\). |
| `SigSegv` | `11` | Segmentation fault. |
| `SigStop` | `19` | Stop signal (cannot be caught). |
| `SigTerm` | `15` | Termination signal. |
| `SigTstp` | `20` | Terminal stop (Ctrl+Z). |
| `SigTtin` | `21` | Background read from terminal. |
| `SigTtou` | `22` | Background write to terminal. |
| `SigUsr1` | `10` | User-defined signal 1. |
| `SigUsr2` | `12` | User-defined signal 2. |

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
