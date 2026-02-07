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
| `Clear` | `"\x1b[2J"` | Clear entire screen. |
| `ClearToEnd` | `"\x1b[0J"` | Clear from cursor to end of screen. |
| `ClearToBegin` | `"\x1b[1J"` | Clear from cursor to beginning of screen. |
| `ClearLine` | `"\x1b[2K"` | Clear entire line. |
| `ClearLineToEnd` | `"\x1b[0K"` | Clear from cursor to end of line. |
| `ClearLineToBegin` | `"\x1b[1K"` | Clear from cursor to beginning of line. |
| `Home` | `"\x1b[H"` | Move cursor to home position (1, 1). |
| `SaveCursor` | `"\x1b[s"` | Save cursor position. |
| `RestoreCursor` | `"\x1b[u"` | Restore cursor position. |
| `HideCursor` | `"\x1b[?25l"` | Hide cursor. |
| `ShowCursor` | `"\x1b[?25h"` | Show cursor. |
| `CursorUp` | `"\x1b[1A"` | Move cursor up one line. |
| `CursorDown` | `"\x1b[1B"` | Move cursor down one line. |
| `CursorRight` | `"\x1b[1C"` | Move cursor right one column. |
| `CursorLeft` | `"\x1b[1D"` | Move cursor left one column. |
| `ScrollUp` | `"\x1b[1S"` | Scroll up one line. |
| `ScrollDown` | `"\x1b[1T"` | Scroll down one line. |
| `AltScreen` | `"\x1b[?1049h"` | Enter alternate screen buffer. |
| `MainScreen` | `"\x1b[?1049l"` | Exit alternate screen buffer. |

### Functions

#### `fn` fg256

Set foreground to 256-color palette value (0-255).

**Signature:** `(n:i64 -- s:str)`

| Input | Type | Description |
|-------|------|-------------|
| `n` | `i64` | Color index (0-255) |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
196 term::fg256 print "Red" print term::Reset print nl
```
---

#### `fn` bg256

Set background to 256-color palette value (0-255).

**Signature:** `(n:i64 -- s:str)`

| Input | Type | Description |
|-------|------|-------------|
| `n` | `i64` | Color index (0-255) |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
21 term::bg256 print "Blue BG" print term::Reset print nl
```
---

#### `fn` rgb

Set foreground to RGB color.

**Signature:** `(r:i64 g:i64 b:i64 -- s:str)`

| Input | Type | Description |
|-------|------|-------------|
| `r` | `i64` | Red component (0-255) |
| `g` | `i64` | Green component (0-255) |
| `b` | `i64` | Blue component (0-255) |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
255 128 0 term::rgb print "Orange" print term::Reset print nl
```
---

#### `fn` bg_rgb

Set background to RGB color.

**Signature:** `(r:i64 g:i64 b:i64 -- s:str)`

| Input | Type | Description |
|-------|------|-------------|
| `r` | `i64` | Red component (0-255) |
| `g` | `i64` | Green component (0-255) |
| `b` | `i64` | Blue component (0-255) |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
0 0 128 term::bg_rgb print "Navy BG" print term::Reset print nl
```
---

#### `fn` goto

Move cursor to specific position (1-indexed).

**Signature:** `(row:i64 col:i64 -- s:str)`

| Input | Type | Description |
|-------|------|-------------|
| `row` | `i64` | Row number (1-based) |
| `col` | `i64` | Column number (1-based) |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
10 5 term::goto print
```
---

#### `fn` up

Move cursor up by n lines.

**Signature:** `(n:i64 -- s:str)`

| Input | Type | Description |
|-------|------|-------------|
| `n` | `i64` | Number of lines |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

---

#### `fn` down

Move cursor down by n lines.

**Signature:** `(n:i64 -- s:str)`

| Input | Type | Description |
|-------|------|-------------|
| `n` | `i64` | Number of lines |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

---

#### `fn` right

Move cursor right by n columns.

**Signature:** `(n:i64 -- s:str)`

| Input | Type | Description |
|-------|------|-------------|
| `n` | `i64` | Number of columns |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

---

#### `fn` left

Move cursor left by n columns.

**Signature:** `(n:i64 -- s:str)`

| Input | Type | Description |
|-------|------|-------------|
| `n` | `i64` | Number of columns |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |
