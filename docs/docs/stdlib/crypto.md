# `use` crypto

Cryptographic hash functions, checksums, and HMAC.
Provides SHA-256, SHA-512, MD5, CRC32, and HMAC implementations.

Each hash algorithm supports both one-shot hashing and streaming (incremental) mode.
HMAC provides message authentication using SHA-256 or SHA-512.

## Functions

### `fn` crc32

One-shot CRC32 of a string.

**Signature:** `(s:str -- crc:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `crc` | `i64` | CRC32 checksum |

**Example:**

```qd
"Hello" crc32  // crc
```
---

### `fn` hmac_sha256

Compute HMAC-SHA256.

**Signature:** `(key:str message:str -- mac:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | `str` | Secret key |
| `message` | `str` | Message to authenticate |

| Output | Type | Description |
|--------|------|-------------|
| `mac` | `str` | 64-character hex HMAC |

**Example:**

```qd
"Jefe" "what do ya want for nothing?" crypto::hmac_sha256!  // mac
```
---

### `fn` hmac_sha512

Compute HMAC-SHA512.

**Signature:** `(key:str message:str -- mac:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | `str` | Secret key |
| `message` | `str` | Message to authenticate |

| Output | Type | Description |
|--------|------|-------------|
| `mac` | `str` | 128-character hex HMAC |

**Example:**

```qd
"Jefe" "what do ya want for nothing?" crypto::hmac_sha512!  // mac
```
---

### `fn` md5

One-shot MD5 of a string. Note: MD5 is cryptographically broken. Use only for checksums, not security.

**Signature:** `(s:str -- hash:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `hash` | `str` | 32-character hex string |

**Example:**

```qd
"Hello" md5!  // hash
```
---

### `fn` sha256

One-shot SHA-256 of a string.

**Signature:** `(s:str -- hash:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `hash` | `str` | 64-character hex string |

**Example:**

```qd
"Hello" sha256!  // hash
```
---

### `fn` sha512

One-shot SHA-512 of a string.

**Signature:** `(s:str -- hash:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `hash` | `str` | 128-character hex string |
## Crc32

CRC32 hasher state for streaming computation.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `state` | `Current` | CRC state |
| `tbl` | `Lookup` | table (allocated on first update) |

### Constructors

#### `fn` crc32_hex

One-shot CRC32 as hex string.

**Signature:** `(s:str -- hex:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `hex` | `str` | CRC32 as 8-character hex string |
---

#### `fn` crc32_verify

Verify data against expected CRC32.

**Signature:** `(s:str expected:i64 -- ok:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Data to verify |
| `expected` | `i64` | Expected CRC32 value |

| Output | Type | Description |
|--------|------|-------------|
| `ok` | `i64` | 1 if matches, 0 otherwise |

### Methods

#### `fn` hex

Get CRC32 as 8-character hex string.

**Signature:** `(c:Crc32) hex( -- hex:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Crc32` | The hasher state |

| Output | Type | Description |
|--------|------|-------------|
| `hex` | `str` | CRC32 as lowercase hex string |
---

#### `fn` release

Free resources used by CRC32 hasher.

**Signature:** `(c:Crc32) release()`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Crc32` | The hasher state to free |
---

#### `fn` update

Update CRC32 state with additional data.

**Signature:** `(c:Crc32) update(data:str -- c2:Crc32)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Crc32` | The hasher state |
| `data` | `str` | Data to hash |

| Output | Type | Description |
|--------|------|-------------|
| `c2` | `Crc32` | Updated hasher state |
---

#### `fn` value

Get the current CRC32 value.

**Signature:** `(c:Crc32) value( -- crc:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Crc32` | The hasher state |

| Output | Type | Description |
|--------|------|-------------|
| `crc` | `i64` | Current CRC32 checksum |

## Md5

MD5 hasher state for streaming computation.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `a` | `i64` |  |
| `b` | `i64` |  |
| `c` | `i64` |  |
| `d` | `i64` |  |
| `buf` | `ptr` |  |
| `buflen` | `i64` |  |
| `total` | `i64` |  |

### Methods

#### `fn` finish

Finalize MD5 and get result as hex string.

**Signature:** `(h:Md5) finish( -- hash:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `h` | `Md5` | The hasher state |

| Output | Type | Description |
|--------|------|-------------|
| `hash` | `str` | 32-character hex string (lowercase) |
---

#### `fn` release

Free resources used by MD5 hasher.

**Signature:** `(h:Md5) release()`

| Parameter | Type | Description |
|-----------|------|-------------|
| `h` | `Md5` | The hasher state to free |
---

#### `fn` update

Update MD5 state with additional data.

**Signature:** `(h:Md5) update(data:str -- h2:Md5)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `h` | `Md5` | The hasher state |
| `data` | `str` | Data to hash |

| Output | Type | Description |
|--------|------|-------------|
| `h2` | `Md5` | Updated hasher state |

## Sha256

SHA-256 hasher state for streaming computation.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `buf` | `Buffer` | for incomplete blocks |
| `buflen` | `Current` | buffer fill level |
| `total` | `Total` | bytes processed |

### Constructors

#### `fn` sha256_bytes

Calculate SHA-256 hash of raw bytes.

**Signature:** `(buf:ptr length:i64 -- hash:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Input buffer |
| `length` | `i64` | Length of buffer in bytes |

| Output | Type | Description |
|--------|------|-------------|
| `hash` | `str` | 64-character hex string (lowercase) |

### Methods

#### `fn` finish

Finalize SHA-256 and get result as hex string.

**Signature:** `(h:Sha256) finish( -- hash:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `h` | `Sha256` | The hasher state |

| Output | Type | Description |
|--------|------|-------------|
| `hash` | `str` | 64-character hex string (lowercase) |
---

#### `fn` release

Free resources used by SHA-256 hasher.

**Signature:** `(h:Sha256) release()`

| Parameter | Type | Description |
|-----------|------|-------------|
| `h` | `Sha256` | The hasher state to free |
---

#### `fn` update

Update SHA-256 state with additional data.

**Signature:** `(h:Sha256) update(data:str -- h2:Sha256)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `h` | `Sha256` | The hasher state |
| `data` | `str` | Data to hash |

| Output | Type | Description |
|--------|------|-------------|
| `h2` | `Sha256` | Updated hasher state |

## Sha512

SHA-512 hasher state for streaming computation.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `h0` | `i64` |  |
| `h1` | `i64` |  |
| `h2` | `i64` |  |
| `h3` | `i64` |  |
| `h4` | `i64` |  |
| `h5` | `i64` |  |
| `h6` | `i64` |  |
| `h7` | `i64` |  |
| `buf` | `ptr` |  |
| `buflen` | `i64` |  |
| `total` | `i64` |  |

### Constructors

#### `fn` sha512_bytes

Calculate SHA-512 hash of raw bytes.

**Signature:** `(buf:ptr length:i64 -- hash:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Input buffer |
| `length` | `i64` | Length of buffer in bytes |

| Output | Type | Description |
|--------|------|-------------|
| `hash` | `str` | 128-character hex string |

### Methods

#### `fn` finish

Finalize SHA-512 and get result as hex string.

**Signature:** `(h:Sha512) finish( -- hash:str)!`
---

#### `fn` release

Free resources used by SHA-512 hasher.

**Signature:** `(h:Sha512) release()`
---

#### `fn` update

Update SHA-512 state with additional data.

**Signature:** `(h:Sha512) update(data:str -- h2:Sha512)!`

