# crc32

CRC32 checksum calculation.
Implements the standard CRC-32 algorithm (ISO 3309, used in Ethernet, ZIP, PNG).
Polynomial: 0xEDB88320 (reflected form of 0x04C11DB7)

## Functions

### checksum

Calculate CRC32 checksum of a string.

**Signature:** `( s:str -- crc:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Return | Type | Description |
|--------|------|-------------|
| `crc` | `i64` | CRC32 checksum as unsigned 32-bit value |

**Example:**

```qd
"Hello" crc32::checksum  // crc
```

---

### checksum_buf

Calculate CRC32 of a byte buffer.

**Signature:** `( buf:ptr len:i64 -- crc:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to checksum |
| `len` | `i64` | Length of buffer in bytes |

| Return | Type | Description |
|--------|------|-------------|
| `crc` | `i64` | CRC32 checksum |

**Example:**

```qd
buf buflen crc32::checksum_buf  // crc
```

---

### checksum_hex

Calculate CRC32 and return as 8-character hex string.

**Signature:** `( s:str -- h:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Return | Type | Description |
|--------|------|-------------|
| `hex` | `str` | CRC32 as hex string (lowercase) |

**Example:**

```qd
"Hello" crc32::hex  // h  // "f7d18982"
```

---

### finalize

Finalize a running CRC to get the final checksum.

**Signature:** `( crc:i64 -- final:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `crc` | `i64` | Current CRC state from update() |

| Return | Type | Description |
|--------|------|-------------|
| `final` | `i64` | Final CRC32 checksum |

**Example:**

```qd
crc_state crc32::finalize  // crc
```

---

### update

Update a running CRC with more data.
Use for streaming/incremental CRC calculation.

**Signature:** `( crc:i64 s:str -- new_crc:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `crc` | `i64` | Current CRC state (use 4294967295 to start) |
| `s` | `str` | Data to add |

| Return | Type | Description |
|--------|------|-------------|
| `new_crc` | `i64` | Updated CRC state |

**Example:**

```qd
4294967295 "Hello" crc32::update " World" crc32::update crc32::finalize  // crc
```

---

### verify

Verify data against an expected CRC32.

**Signature:** `( s:str expected:i64 -- ok:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Data to verify |
| `expected` | `i64` | Expected CRC32 value |

| Return | Type | Description |
|--------|------|-------------|
| `ok` | `i64` | 1 if matches, 0 otherwise |

**Example:**

```qd
"Hello" 4157704578 crc32::verify  // ok
```
