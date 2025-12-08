# hex

Hexadecimal encoding and decoding.
Converts between binary data and hexadecimal string representation.

## Functions

### decode

Decode a hexadecimal string to bytes.
Accepts both uppercase and lowercase hex digits.
Invalid characters are skipped.

**Signature:** `( hex:str -- s:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `hex` | `str` | Hexadecimal string |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | Decoded string |

**Example:**

```qd
"48656C6C6F" hex::decode  // "Hello"
```

---

### decode_byte

Decode a 2-character hex string to a byte value.
Returns -1 if the input is invalid.

**Signature:** `( hex:str -- b:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `hex` | `str` | Two-character hex string |

| Output | Type | Description |
|--------|------|-------------|
| `b` | `i64` | Byte value (0-255) or -1 if invalid |

**Example:**

```qd
"41" hex::decode_byte  // 65
```

---

### decoded_len

Get the length of data that would result from decoding a hex string.
Returns -1 if the hex string has invalid length (odd).

**Signature:** `( hex:str -- len:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `hex` | `str` | Hexadecimal string |

| Output | Type | Description |
|--------|------|-------------|
| `len` | `i64` | Decoded length in bytes, or -1 if invalid |

**Example:**

```qd
"48656C6C6F" hex::decoded_len  // 5
```

---

### encode

Encode a string to uppercase hexadecimal.
Each byte becomes two hex characters (00-FF).

**Signature:** `( s:str -- hex:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to encode |

| Output | Type | Description |
|--------|------|-------------|
| `hex` | `str` | Hexadecimal string (uppercase) |

**Example:**

```qd
"Hello" hex::encode  // "48656C6C6F"
```

---

### encode_byte

Encode a single byte (0-255) to a 2-character hex string.

**Signature:** `( b:i64 -- hex:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `b` | `i64` | Byte value (0-255) |

| Output | Type | Description |
|--------|------|-------------|
| `hex` | `str` | Two-character hex string (uppercase) |

**Example:**

```qd
65 hex::encode_byte  // "41"
```

---

### encode_lower

Encode a string to lowercase hexadecimal.
Each byte becomes two hex characters (00-ff).

**Signature:** `( s:str -- hex:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to encode |

| Output | Type | Description |
|--------|------|-------------|
| `hex` | `str` | Hexadecimal string (lowercase) |

**Example:**

```qd
"Hello" hex::encode_lower  // "48656c6c6f"
```

---

### encoded_len

Get the length of hex string that would result from encoding data.

**Signature:** `( s:str -- len:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to encode |

| Output | Type | Description |
|--------|------|-------------|
| `len` | `i64` | Encoded length in characters |

**Example:**

```qd
"Hello" hex::encoded_len  // 10
```

---

### is_valid

Check if a string contains only valid hexadecimal characters.
Valid characters are 0-9, A-F, a-f.

**Signature:** `( hex:str -- valid:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `hex` | `str` | String to check |

| Output | Type | Description |
|--------|------|-------------|
| `valid` | `i64` | 1 if valid hex, 0 otherwise |

**Example:**

```qd
"48656C6C6F" hex::is_valid  // 1
"48ZZ" hex::is_valid  // 0
```
