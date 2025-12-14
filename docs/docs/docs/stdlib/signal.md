# signal

Unix signal handling with polling-based API.

Signals are caught and stored as flags. Use pending() to check
if a signal was received, and clear() to reset the flag.

Example:
use signal

fn main() {
signal::SIGINT signal::trap
"Running. Press Ctrl+C to stop." . nl

1 while {
signal::SIGINT signal::pending if {
"Shutting down" . nl
signal::SIGINT signal::clear
break
}
}
}

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `SIGABRT` | `6` | Abort signal. |
| `SIGALRM` | `14` | Alarm clock. |
| `SIGCHLD` | `17` | Child process stopped or terminated. |
| `SIGCONT` | `18` | Continue if stopped. |
| `SIGFPE` | `8` | Floating point exception. |
| `SIGHUP` | `1` | Hangup signal (terminal closed). |
| `SIGILL` | `4` | Illegal instruction. |
| `SIGINT` | `2` | Interrupt signal (Ctrl+C). |
| `SIGKILL` | `9` | Kill signal (cannot be caught). |
| `SIGPIPE` | `13` | Broken pipe. |
| `SIGQUIT` | `3` | Quit signal (Ctrl+\). |
| `SIGSEGV` | `11` | Segmentation fault. |
| `SIGSTOP` | `19` | Stop signal (cannot be caught). |
| `SIGTERM` | `15` | Termination signal. |
| `SIGTSTP` | `20` | Terminal stop (Ctrl+Z). |
| `SIGTTIN` | `21` | Background read from terminal. |
| `SIGTTOU` | `22` | Background write to terminal. |
| `SIGUSR1` | `10` | User-defined signal 1. |
| `SIGUSR2` | `12` | User-defined signal 2. |

## Functions

### clear

Clear the pending flag for a signal.

**Signature:** `( signum:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number |

**Example:**

```qd
signal::SIGINT signal::clear
```

---

### ignore

Ignore the specified signal completely.

**Signature:** `( signum:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number |

**Example:**

```qd
signal::SIGPIPE signal::ignore
```

---

### pending

Check if a signal is pending (received but not cleared).
Returns 1 if pending, 0 otherwise. Does not clear the flag.

**Signature:** `( signum:i64 -- flag:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number |

| Output | Type | Description |
|--------|------|-------------|
| `flag` | `i64` | 1 if pending, 0 otherwise |

**Example:**

```qd
signal::SIGINT signal::pending
```

---

### reset

Reset signal to default behavior.

**Signature:** `( signum:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number |

**Example:**

```qd
signal::SIGINT signal::reset
```

---

### trap

Install a handler to catch the specified signal.
After trapping, the signal sets a pending flag instead of
causing the default action (e.g., termination).

**Signature:** `( signum:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `signum` | `i64` | Signal number (use signal::SIGINT, etc.) |

**Example:**

```qd
signal::SIGINT signal::trap
```

---

### wait

Block until any trapped signal is received.
Returns the signal number that was received.

**Signature:** `( -- signum:i64 )`

| Output | Type | Description |
|--------|------|-------------|
| `signum` | `i64` | The signal that was received |

**Example:**

```qd
signal::wait  // sig
```
