# Standard library

The Quadrate standard library provides modules for common programming tasks.

## Using modules

Import a module with `use`:

```qd
use str
use math

fn main() {
	"hello" str::upper print nl  // HELLO
	16.0 math::sqrt print nl  // 4
}
```

## Fallible functions

Functions marked with `!` can fail and require error handling:

```qd
use str

fn main() {
	"hello" 0 3 str::substring! print nl  // "hel"
}
```

## Available modules

| Module | Description |
|--------|-------------|
| [bits](bits.md) | Bitwise operations for integer manipulation. |
| [bytes](bytes.md) | Byte array operations and endianness conversion. |
| [flag](flag.md) | Command-line flag parsing. |
| [fmt](fmt.md) | Formatted output functions. |
| [io](io.md) | File and stream I/O operations. |
| [limits](limits.md) | Numeric limits and constants. |
| [math](math.md) | Mathematical functions and constants. |
| [mem](mem.md) | Low-level memory allocation and manipulation. |
| [os](os.md) | Operating system interface. |
| [path](path.md) | File path manipulation functions. |
| [rand](rand.md) | Random number generation using xorshift64* algorithm. |
| [sb](sb.md) | StringBuilder - Efficient string building. |
| [signal](signal.md) | Unix signal handling with polling-based API. |
| [str](str.md) | String manipulation functions. |
| [strconv](strconv.md) | String to number conversions. |
| [term](term.md) | Terminal colors and formatting using ANSI escape codes. |
| [testing](testing.md) | Testing utilities for unit tests. |
| [thread](thread.md) | Thread module - threading primitives using C11 threads. |
| [time](time.md) | Time operations and duration constants. |
| [unicode](unicode.md) | Unicode character constants and classification. |
