# Standard Library

The Quadrate standard library provides modules for common programming tasks.

## Using Modules

Import a module with `use`:

```qd
use str
use math

fn main( -- ) {
	"hello" str::upper prints nl  // HELLO
	16.0 math::sqrt print nl  // 4
}
```

## Fallible Functions

Functions marked with `!` can fail and require error handling:

```qd
use str

fn main( -- ) {
	"hello" 0 3 str::substring! prints nl  // "hel"
}
```

## Available Modules

| Module | Description |
|--------|-------------|
| [base64](base64.md) | Base64 encoding and decoding. Optimized with lookup tables a... |
| [bits](bits.md) |  |
| [bytes](bytes.md) | Byte array operations and endianness conversion. Provides fu... |
| [crc32](crc32.md) | CRC32 checksum calculation. Implements the standard CRC-32 a... |
| [flag](flag.md) | Command-line flag parsing. |
| [fmt](fmt.md) | Formatted output functions. |
| [hex](hex.md) | Hexadecimal encoding and decoding. Converts between binary d... |
| [io](io.md) |  |
| [json](json.md) | JSON parsing and querying without AST construction. |
| [math](math.md) |  |
| [mem](mem.md) | Low-level memory allocation and manipulation.  SAFETY: These... |
| [net](net.md) | TCP network operations. |
| [os](os.md) |  |
| [path](path.md) | File path manipulation functions. POSIX-style paths with for... |
| [rand](rand.md) | Random number generation using xorshift64* algorithm. Fast, ... |
| [regex](regex.md) | Regular expression matching using Thompson NFA. Supports = .... |
| [sb](sb.md) | StringBuilder - Efficient string building. Avoids O(n²) cost... |
| [sha256](sha256.md) | SHA-256 cryptographic hash function. Produces a 256-bit (32-... |
| [sort](sort.md) | Sorting algorithms for arrays. Arrays are pointers to contig... |
| [str](str.md) | String manipulation functions. |
| [strconv](strconv.md) | String to number conversions. |
| [testing](testing.md) | Testing utilities for unit tests.  Provides assertion functi... |
| [time](time.md) |  |
| [unicode](unicode.md) |  |
| [uri](uri.md) | URI encoding, decoding, and parsing. Handles percent-encodin... |
| [uuid](uuid.md) | UUID generation (version 4 random UUIDs). Format: xxxxxxxx-x... |
