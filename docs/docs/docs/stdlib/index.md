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
	"hello" 0 3 str::substring if {
		prints nl  // "hel"
	} else {
		drop
		"substring failed" print nl
	}
}
```

## Modules by Category

### Text & Strings

| Module | Description |
|--------|-------------|
| [str](str.md) | String manipulation functions |
| [strconv](strconv.md) | String to number conversions |
| [fmt](fmt.md) | Formatted output functions |
| [sb](sb.md) | StringBuilder for efficient string building |
| [regex](regex.md) | Regular expression matching |
| [unicode](unicode.md) | Unicode utilities |

### Data & Encoding

| Module | Description |
|--------|-------------|
| [json](json.md) | JSON parsing and querying |
| [base64](base64.md) | Base64 encoding and decoding |
| [hex](hex.md) | Hexadecimal encoding and decoding |
| [bytes](bytes.md) | Byte array operations |

### Math & Numbers

| Module | Description |
|--------|-------------|
| [math](math.md) | Mathematical functions |
| [bits](bits.md) | Bit manipulation utilities |
| [rand](rand.md) | Random number generation |

### I/O & System

| Module | Description |
|--------|-------------|
| [io](io.md) | Input/output operations |
| [os](os.md) | Operating system interaction |
| [path](path.md) | File path manipulation |
| [net](net.md) | TCP network operations |

### Time & IDs

| Module | Description |
|--------|-------------|
| [time](time.md) | Date and time functions |
| [uuid](uuid.md) | UUID generation |

### Utilities

| Module | Description |
|--------|-------------|
| [mem](mem.md) | Memory allocation and manipulation |
| [sort](sort.md) | Sorting algorithms |
| [flag](flag.md) | Command-line flag parsing |
| [testing](testing.md) | Unit testing utilities |

### Hashing

| Module | Description |
|--------|-------------|
| [sha256](sha256.md) | SHA-256 cryptographic hash |
| [crc32](crc32.md) | CRC32 checksum calculation |

### Web

| Module | Description |
|--------|-------------|
| [uri](uri.md) | URI encoding, decoding, and parsing |
