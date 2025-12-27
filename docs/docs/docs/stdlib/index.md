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
| [base64](base64.md) | Base64 encoding and decoding. |
| [bits](bits.md) | Bitwise operations for integer manipulation. |
| [bytes](bytes.md) | Byte array operations and endianness conversion. |
| [crc32](crc32.md) | CRC32 checksum calculation. |
| [flag](flag.md) | Command-line flag parsing. |
| [fmt](fmt.md) | Formatted output functions. |
| [hex](hex.md) | Hexadecimal encoding and decoding. |
| [hof](hof.md) | Higher-Order Function combinators. |
| [io](io.md) | File and stream I/O operations. |
| [json](json.md) | JSON parsing and querying without AST construction. |
| [limits](limits.md) | Numeric limits and constants. |
| [math](math.md) | Mathematical functions and constants. |
| [mem](mem.md) | Low-level memory allocation and manipulation. |
| [net](net.md) | TCP network operations. |
| [os](os.md) | Operating system interface. |
| [path](path.md) | File path manipulation functions. |
| [rand](rand.md) | Random number generation using xorshift64* algorithm. |
| [regex](regex.md) | Regular expression matching using Thompson NFA. |
| [sb](sb.md) | StringBuilder - Efficient string building. |
| [sha256](sha256.md) | SHA-256 cryptographic hash function. |
| [signal](signal.md) | Unix signal handling with polling-based API. |
| [sort](sort.md) | Sorting algorithms for arrays. |
| [str](str.md) | String manipulation functions. |
| [strconv](strconv.md) | String to number conversions. |
| [testing](testing.md) | Testing utilities for unit tests. |
| [time](time.md) | Time operations and duration constants. |
| [unicode](unicode.md) | Unicode character constants and classification. |
| [uri](uri.md) | URI encoding, decoding, and parsing. |
| [uuid](uuid.md) | UUID generation (version 4 random UUIDs). |
