# `use` fmt

Formatted output functions using printf-style format specifiers.

## Format specifiers

| Specifier | Type | Description |
|-----------|------|-------------|
| `%s` | `str` | String |
| `%d` | `i64` | Signed decimal integer |
| `%i` | `i64` | Signed decimal integer (same as `%d`) |
| `%u` | `i64` | Unsigned decimal integer |
| `%x` | `i64` | Hexadecimal lowercase |
| `%X` | `i64` | Hexadecimal uppercase |
| `%o` | `i64` | Octal |
| `%f` | `f64` | Floating-point decimal |
| `%e` | `f64` | Scientific notation lowercase |
| `%E` | `f64` | Scientific notation uppercase |
| `%g` | `f64` | Shortest of `%f` or `%e` |
| `%G` | `f64` | Shortest of `%f` or `%E` |
| `%c` | `i64` | Character (ASCII value) |
| `%p` | `ptr` | Pointer address |
| `%%` | — | Literal percent sign |

### Modifiers

- **Width**: `"%5d"` pads to 5 characters
- **Precision**: `"%.2f"` limits to 2 decimal places
- **Zero-pad**: `"%05d"` pads with zeros
- **Left-align**: `"%-5d"` left-aligns within width
- **Sign**: `"%+d"` always shows sign

```qd
use fmt

fn main() {
	42 "%d" fmt::printf nl             // 42
	255 "%x" fmt::printf nl            // ff
	3.14159 "%.2f" fmt::printf nl      // 3.14
	42 "%05d" fmt::printf nl           // 00042
	65 "%c" fmt::printf nl             // A
	"test" 42 "%s = %d" fmt::printf nl // test = 42
}
```

Arguments are pushed onto the stack before the format string, in the order they appear in the format string.

## Functions

### `fn` printf

Print formatted output to stdout.

**Signature:** `(format:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `format` | `str` | Format string with % specifiers |

**Example:**

```qd
"world" "Hello %s\n" fmt::printf  // Hello world
```
---

### `fn` sprintf

Format a string with printf-style specifiers.

**Signature:** `(format:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `format` | `str` | Format string with % specifiers |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Formatted string |

**Example:**

```qd
"world" "Hello %s\n" fmt::sprintf  // "Hello world\n"
```
