# crypto

Cryptographic hash functions for Quadrate.

## Installation

```bash
quadpm get https://git.sr.ht/~klahr/qdcrypto
```

## Usage

### SHA-256

```qd
use crypto

fn main() {
    "Hello" crypto::sha256 print nl
    // prints: 185f8db32271fe25f561a6fc938b2e264306ec304eda518007d1764826381969
}
```

### SHA-512

```qd
use crypto

fn main() {
    "Hello" crypto::sha512 print nl
}
```

### MD5

```qd
use crypto

fn main() {
    "Hello" crypto::md5 print nl
    // prints: 8b1a9953c4611296a827abf8c47804d7
}
```

### CRC32

```qd
use crypto

fn main() {
    "Hello" crypto::crc32 print nl
    // prints: 4157704578
}
```

## Functions

- `sha256(data:str -- hash:str)` - Compute SHA-256 hash (64 hex chars)
- `sha512(data:str -- hash:str)` - Compute SHA-512 hash (128 hex chars)
- `md5(data:str -- hash:str)` - Compute MD5 hash (32 hex chars)
- `crc32(data:str -- checksum:int)` - Compute CRC32 checksum

## Streaming API

For large data, use the streaming API:

```qd
use crypto

fn main() {
    crypto::sha256_init -> ctx
    ctx "Hello " crypto::sha256_update
    ctx "World" crypto::sha256_update
    ctx crypto::sha256_final -> hash
    hash print nl
}
```

## License

Apache 2.0

## Contributing

Contributions welcome! Please open an issue or submit a patch on [SourceHut](https://git.sr.ht/~klahr/qdcrypto).
