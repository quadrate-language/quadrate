# base64

Base64 encoding and decoding.

## Functions

### decode

Decode base64 string to binary data.

**Signature:** `( encoded:str -- data:ptr data_len:i64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `encoded` | `str` | Base64-encoded string |

| Return | Type | Description |
|--------|------|-------------|
| `data` | `ptr` | Decoded buffer (caller must free) |
| `data_len` | `i64` | Length of decoded data |

**Errors:**

- Invalid base64 encoding

**Example:**

```qd
"SGVsbG8=" base64::decode! -> buf -> len
```

---

### encode

Encode binary data to base64 string.

**Signature:** `( data:ptr len:i64 -- encoded:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `data` | `ptr` | Buffer to encode |
| `len` | `i64` | Length of data |

| Return | Type | Description |
|--------|------|-------------|
| `encoded` | `str` | Base64-encoded string |

**Example:**

```qd
buf 5 base64::encode -> b64
```
