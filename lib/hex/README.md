# hex

Hexadecimal encoding and decoding for Quadrate.

## Installation

```bash
quadpm get https://git.sr.ht/~klahr/qdhex
```

## Usage

```qd
use hex

fn main() {
    // Encode to uppercase hex
    "Hello" hex::encode -> h
    h print nl  // "48656C6C6F"

    // Encode to lowercase hex
    "Hello" hex::encode_lower -> h2
    h2 print nl  // "48656c6c6f"

    // Decode hex string
    "48656C6C6F" hex::decode -> s
    s print nl  // "Hello"

    // Single byte operations
    65 hex::encode_byte -> b
    b print nl  // "41"

    "41" hex::decode_byte -> v
    v print nl  // 65

    // Validation
    "48656C6C6F" hex::is_valid -> valid
    valid print nl  // 1
}
```

## API

### Functions

- `encode(s:str -- hex:str)` - Encode string to uppercase hex
- `encode_lower(s:str -- hex:str)` - Encode string to lowercase hex
- `decode(hex:str -- s:str)` - Decode hex string to bytes
- `encode_byte(b:i64 -- hex:str)` - Encode single byte to 2-char hex
- `decode_byte(hex:str -- b:i64)` - Decode 2-char hex to byte (-1 if invalid)
- `is_valid(hex:str -- valid:i64)` - Check if string is valid hex
- `decoded_len(hex:str -- len:i64)` - Get decoded length (-1 if invalid)
- `encoded_len(s:str -- len:i64)` - Get encoded length

## License

Apache 2.0

## Contributing

Contributions welcome! Please open an issue or submit a patch on [SourceHut](https://git.sr.ht/~klahr/qdhex).
