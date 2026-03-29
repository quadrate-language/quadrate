# `use` term

Terminal colors and formatting using ANSI escape codes.
Use these constants to colorize terminal output.
@example term::Green print "PASS" print term::Reset print nl

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `AltScreen` | `"\x1b[?1049h"` | Enter alternate screen buffer. |
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
| `ClearLineToBegin` | `"\x1b[1K"` | Clear from cursor to beginning of line. |
| `ClearLineToEnd` | `"\x1b[0K"` | Clear from cursor to end of line. |
| `ClearLine` | `"\x1b[2K"` | Clear entire line. |
| `ClearToBegin` | `"\x1b[1J"` | Clear from cursor to beginning of screen. |
| `ClearToEnd` | `"\x1b[0J"` | Clear from cursor to end of screen. |
| `Clear` | `"\x1b[2J"` | Clear entire screen. |
| `CursorDown` | `"\x1b[1B"` | Move cursor down one line. |
| `CursorLeft` | `"\x1b[1D"` | Move cursor left one column. |
| `CursorRight` | `"\x1b[1C"` | Move cursor right one column. |
| `CursorUp` | `"\x1b[1A"` | Move cursor up one line. |
| `Cyan` | `"\x1b[36m"` | Cyan foreground. |
| `Dim` | `"\x1b[2m"` | Dim text. |
| `Green` | `"\x1b[32m"` | Green foreground. |
| `HideCursor` | `"\x1b[?25l"` | Hide cursor. |
| `Home` | `"\x1b[H"` | Move cursor to home position (1, 1). |
| `Italic` | `"\x1b[3m"` | Italic text. |
| `Magenta` | `"\x1b[35m"` | Magenta foreground. |
| `MainScreen` | `"\x1b[?1049l"` | Exit alternate screen buffer. |
| `Red` | `"\x1b[31m"` | Red foreground. |
| `Reset` | `"\x1b[0m"` | Reset all formatting. |
| `RestoreCursor` | `"\x1b[u"` | Restore cursor position. |
| `SaveCursor` | `"\x1b[s"` | Save cursor position. |
| `ScrollDown` | `"\x1b[1T"` | Scroll down one line. |
| `ScrollUp` | `"\x1b[1S"` | Scroll up one line. |
| `ShowCursor` | `"\x1b[?25h"` | Show cursor. |
| `Underline` | `"\x1b[4m"` | Underline text. |
| `White` | `"\x1b[37m"` | White foreground. |
| `Yellow` | `"\x1b[33m"` | Yellow foreground. |

## Functions

### `fn` bg256

Set background to 256-color palette value (0-255).

**Signature:** `(n:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Color index (0-255) |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
21 term::bg256 print "Blue BG" print term::Reset print nl
```
---

### `fn` bg_rgb

Set background to RGB color.

**Signature:** `(r:i64 g:i64 b:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
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

### `fn` down

Move cursor down by n lines.

**Signature:** `(n:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Number of lines |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
5 term::down print
```
---

### `fn` fg256

Set foreground to 256-color palette value (0-255).

**Signature:** `(n:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Color index (0-255) |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
196 term::fg256 print "Red" print term::Reset print nl
```
---

### `fn` goto

Move cursor to specific position (1-indexed).

**Signature:** `(row:i64 col:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
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

### `fn` left

Move cursor left by n columns.

**Signature:** `(n:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Number of columns |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
10 term::left print
```
---

### `fn` rgb

Set foreground to RGB color.

**Signature:** `(r:i64 g:i64 b:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
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

### `fn` right

Move cursor right by n columns.

**Signature:** `(n:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Number of columns |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
10 term::right print
```
---

### `fn` up

Move cursor up by n lines.

**Signature:** `(n:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Number of lines |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | ANSI escape sequence |

**Example:**

```qd
5 term::up print
```
