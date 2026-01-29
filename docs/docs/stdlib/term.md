# `use` term

Terminal colors and formatting using ANSI escape codes.
Use these constants to colorize terminal output.

**Example:**

```qd
term::Green print "PASS" print term::Reset print nl
```

## Constants

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

