# Standard Library

The Quadrate standard library provides modules for common programming tasks.

## Using Modules

Import a module with `use`:

```qd
use strings
use math

fn main() {
	"hello" strings::upper print nl  // HELLO
	16.0 math::sqrt print nl  // 4
}
```

## Fallible Functions

Functions marked with `!` can fail and require error handling:

```qd
use strings

fn main() {
	"hello" 0 3 strings::substring! print nl  // "hel"
}
```

## Available Modules

| Module | Description |
|--------|-------------|
| [base64](base64.md) | Base64 encoding and decoding. |
| [bits](bits.md) | Bitwise operations for integer manipulation. |
| [bytes](bytes.md) | Byte array operations and endianness conversion. |
| [crypto](crypto.md) | Cryptographic hash functions, checksums, and HMAC. |
| [ct](ct.md) | Generic container types. |
| [flag](flag.md) | Command-line flag parsing. |
| [fmt](fmt.md) | Formatted output functions. |
| [fuzzy](fuzzy.md) | Minimum of two integers. |
| [hex](hex.md) | Hexadecimal encoding and decoding. |
| [hof](hof.md) | Higher-Order Function combinators. |
| [http](http.md) | HTTP module. |
| [io](io.md) | File and stream I/O operations. |
| [json](json.md) | JSON parsing and querying without AST construction. |
| [limits](limits.md) | Numeric limits and constants. |
| [log](log.md) | Logging module - structured logging with levels and rotation... |
| [math](math.md) | Mathematical functions and constants. |
| [mem](mem.md) | Low-level memory allocation and manipulation. |
| [net](net.md) | TCP network operations. |
| [os](os.md) | Operating system interface. |
| [path](path.md) | File path manipulation functions. |
| [rand](rand.md) | Random number generation using xorshift64* algorithm. |
| [regex](regex.md) | Regular expression matching using Thompson NFA. |
| [sb](sb.md) | StringBuilder - Efficient string building. |
| [signal](signal.md) | Unix signal handling with polling-based API. |
| [sort](sort.md) | Sorting algorithms for arrays. |
| [strconv](strconv.md) | String to number conversions. |
| [strings](strings.md) | String manipulation functions. |
| [term](term.md) | Terminal colors and formatting using ANSI escape codes. |
| [testing](testing.md) | Testing utilities for unit tests. |
| [thread](thread.md) | Thread module - threading primitives using C11 threads. |
| [time](time.md) | Time operations and duration constants. |
| [tls](tls.md) | TLS/SSL secure socket operations. |
| [tty](tty.md) | Terminal detection and information. |
| [unicode](unicode.md) | Unicode character constants and classification. |
| [uri](uri.md) | URI encoding, decoding, and parsing. |
| [uuid](uuid.md) | UUID generation (version 4 random UUIDs). |
