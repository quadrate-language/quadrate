# `use` term

Terminal colors and formatting using ANSI escape codes.
Use these constants to colorize terminal output.

## Example

```qd
use term

fn main() {
    term::Green print "PASS" print term::Reset print nl
    term::Red print "FAIL" print term::Reset print nl
    term::Bold print term::Blue print "Info" print term::Reset print nl
}
```

## Constants

### Formatting

| Name | Description |
|------|-------------|
| `Reset` | Reset all formatting |
| `Bold` | Bold text |
| `Dim` | Dim text |
| `Italic` | Italic text |
| `Underline` | Underline text |

### Foreground Colors

| Name | Description |
|------|-------------|
| `Black` | Black foreground |
| `Red` | Red foreground |
| `Green` | Green foreground |
| `Yellow` | Yellow foreground |
| `Blue` | Blue foreground |
| `Magenta` | Magenta foreground |
| `Cyan` | Cyan foreground |
| `White` | White foreground |

### Bright Foreground Colors

| Name | Description |
|------|-------------|
| `BrightBlack` | Bright black (gray) foreground |
| `BrightRed` | Bright red foreground |
| `BrightGreen` | Bright green foreground |
| `BrightYellow` | Bright yellow foreground |
| `BrightBlue` | Bright blue foreground |
| `BrightMagenta` | Bright magenta foreground |
| `BrightCyan` | Bright cyan foreground |
| `BrightWhite` | Bright white foreground |

### Background Colors

| Name | Description |
|------|-------------|
| `BgBlack` | Black background |
| `BgRed` | Red background |
| `BgGreen` | Green background |
| `BgYellow` | Yellow background |
| `BgBlue` | Blue background |
| `BgMagenta` | Magenta background |
| `BgCyan` | Cyan background |
| `BgWhite` | White background |
