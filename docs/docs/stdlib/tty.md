# Terminal

Terminal detection, colors, and formatting.

## `use` tty

Check if standard streams are connected to a terminal (TTY) and get terminal dimensions. Useful for CLI tools that behave differently when piped vs interactive.

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

### Functions

#### `fn` is_stdin

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

#### `fn` is_stdout

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

#### `fn` is_stderr

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

#### `fn` width

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

#### `fn` height

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

#### `fn` size

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

## `use` term

ANSI escape codes for terminal colors and formatting.

**Example:**

```qd
term::Green print "PASS" print term::Reset print nl
```

### Constants

| Name | Value | Description |
|------|-------|-------------|
| `BgBlack` | `"\x1b[40m"` | Black background. |
| `BgBlue` | `"\x1b[44m"` | Blue background. |
| `BgCyan` | `"\x1b[46m"` | Cyan background. |
| `BgGreen` | `"\x1b[42m"` | Green background. |
| `BgMagenta` | `"\x1b[45m"` | Magenta background. |
| `BgRed` | `"\x1b[41m"` | Red background. |
| `BgWhite` | `"\x1b[47m"` | White background. |
| `BgYellow` | `"\x1b[43m"` | Yellow background. |
| `Black` | `"\x1b[30m"` | Black foreground. |
| `Blue` | `"\x1b[34m"` | Blue foreground. |
| `Bold` | `"\x1b[1m"` | Bold text. |
| `BrightBlack` | `"\x1b[90m"` | Bright black (gray) foreground. |
| `BrightBlue` | `"\x1b[94m"` | Bright blue foreground. |
| `BrightCyan` | `"\x1b[96m"` | Bright cyan foreground. |
| `BrightGreen` | `"\x1b[92m"` | Bright green foreground. |
| `BrightMagenta` | `"\x1b[95m"` | Bright magenta foreground. |
| `BrightRed` | `"\x1b[91m"` | Bright red foreground. |
| `BrightWhite` | `"\x1b[97m"` | Bright white foreground. |
| `BrightYellow` | `"\x1b[93m"` | Bright yellow foreground. |
| `Cyan` | `"\x1b[36m"` | Cyan foreground. |
| `Dim` | `"\x1b[2m"` | Dim text. |
| `Green` | `"\x1b[32m"` | Green foreground. |
| `Italic` | `"\x1b[3m"` | Italic text. |
| `Magenta` | `"\x1b[35m"` | Magenta foreground. |
| `Red` | `"\x1b[31m"` | Red foreground. |
| `Reset` | `"\x1b[0m"` | Reset all formatting. |
| `Underline` | `"\x1b[4m"` | Underline text. |
| `White` | `"\x1b[37m"` | White foreground. |
| `Yellow` | `"\x1b[33m"` | Yellow foreground. |
