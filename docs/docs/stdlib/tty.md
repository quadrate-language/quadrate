# `use` tty

Terminal detection and information.
Provides functions to check if file descriptors are connected to terminals
and to get terminal dimensions.

@example
tty::is_stdout if {
    term::Green print "Success" print term::Reset print nl
} else {
    "Success" print nl
}

## Functions

### `fn` height

Get terminal height in rows. Returns 0 if not a terminal.

**Signature:** `( -- rows:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `rows` | `i64` | Terminal height |

**Example:**

```qd
tty::height  // rows
```
---

### `fn` is_stderr

Check if stderr is connected to a terminal.

**Signature:** `( -- is_tty:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `is_tty` | `i64` | 1 if terminal, 0 otherwise |

**Example:**

```qd
tty::is_stderr if { "Error output to terminal" print nl }
```
---

### `fn` is_stdin

Check if stdin is connected to a terminal. Useful for detecting if input is piped.

**Signature:** `( -- is_tty:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `is_tty` | `i64` | 1 if terminal, 0 otherwise |

**Example:**

```qd
tty::is_stdin 0 == if { "Reading from pipe" print nl }
```
---

### `fn` is_stdout

Check if stdout is connected to a terminal.

**Signature:** `( -- is_tty:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `is_tty` | `i64` | 1 if terminal, 0 otherwise |

**Example:**

```qd
tty::is_stdout if { "Colors enabled" print nl }
```
---

### `fn` size

Get terminal dimensions. Returns 0, 0 if not a terminal.

**Signature:** `( -- rows:i64 cols:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `rows` | `i64` | Terminal height in rows |
| `cols` | `i64` | Terminal width in columns |

**Example:**

```qd
tty::size -> cols  // rows
```
---

### `fn` width

Get terminal width in columns. Returns 0 if not a terminal.

**Signature:** `( -- cols:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `cols` | `i64` | Terminal width |

**Example:**

```qd
tty::width  // cols
```
