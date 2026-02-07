# Standard Library

The Quadrate standard library provides modules for common programming tasks.

## Using Modules

Import a module with `use`:

```qd
use str
use math

fn main() {
	"hello" str::upper print nl  // HELLO
	16.0 math::sqrt print nl  // 4
}
```

## Fallible Functions

Functions marked with `!` can fail and require error handling:

```qd
use str

fn main() {
	"hello" 0 3 str::substring! print nl  // "hel"
}
```

## Available Modules

| Module | Description |
|--------|-------------|
| [bits](bits.md) | Bit manipulation utilities |
| [bytes](bytes.md) | Byte buffer operations |
| [flag](flag.md) | Command-line flag parsing |
| [fmt](fmt.md) | Formatting utilities |
| [io](io.md) | File I/O operations |
| [limits](limits.md) | Numeric type limits |
| [math](math.md) | Mathematical functions, vectors, matrices |
| [mem](mem.md) | Memory management |
| [os](os.md) | Operating system interface |
| [path](path.md) | File path manipulation |
| [rand](rand.md) | Random number generation |
| [sb](sb.md) | String builder |
| [signal](signal.md) | Signal handling |
| [str](str.md) | String operations |
| [strconv](strconv.md) | String conversion utilities |
| [term](term.md) | Terminal operations |
| [testing](testing.md) | Testing framework |
| [thread](thread.md) | Threading primitives |
| [time](time.md) | Time and date operations |
| [tty](tty.md) | Terminal detection and dimensions |
| [unicode](unicode.md) | Unicode utilities |
