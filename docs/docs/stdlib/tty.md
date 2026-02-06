# `use` tty

Terminal detection and information. Check if standard streams are connected to a terminal (TTY) and get terminal dimensions. Useful for CLI tools that behave differently when piped vs interactive.

**Example:**

```qd
use tty
use term

fn main() {
	tty::is_stdout if {
		term::Green print
	}
	"PASS" print
	tty::is_stdout if {
		term::Reset print
	}
	nl
}
```

## Functions

### `fn` is_stdin

Check if stdin is connected to a terminal (TTY). Useful for detecting if input is from keyboard vs pipe.

**Signature:** `( -- is_tty:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `is_tty` | `i64` | 1 if TTY, 0 otherwise |

**Example:**

```qd
tty::is_stdin if { "Enter input: " print }
```
---

### `fn` is_stdout

Check if stdout is connected to a terminal (TTY). Useful for disabling colors when output is piped.

**Signature:** `( -- is_tty:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `is_tty` | `i64` | 1 if TTY, 0 otherwise |

**Example:**

```qd
tty::is_stdout if { term::Green print }
```
---

### `fn` is_stderr

Check if stderr is connected to a terminal (TTY).

**Signature:** `( -- is_tty:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `is_tty` | `i64` | 1 if TTY, 0 otherwise |

**Example:**

```qd
tty::is_stderr if { "interactive stderr" print nl }
```
---

### `fn` width

Get terminal width in columns. Returns 0 if not a terminal or size cannot be determined.

**Signature:** `( -- cols:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `cols` | `i64` | Terminal width in columns, or 0 |

**Example:**

```qd
tty::width -> cols
```
---

### `fn` height

Get terminal height in rows. Returns 0 if not a terminal or size cannot be determined.

**Signature:** `( -- rows:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `rows` | `i64` | Terminal height in rows, or 0 |

**Example:**

```qd
tty::height -> rows
```
---

### `fn` size

Get terminal dimensions (rows and columns). Returns 0 for both values if not a terminal or size cannot be determined.

**Signature:** `( -- rows:i64 cols:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `rows` | `i64` | Terminal height in rows, or 0 |
| `cols` | `i64` | Terminal width in columns, or 0 |

**Example:**

```qd
tty::size -> cols -> rows
```
