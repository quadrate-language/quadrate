# base64

Base64 encoding and decoding.
Optimized with lookup tables and direct buffer writes.

## Functions

### decode

Decode a base64 string.

**Signature:** `( encoded:str -- decoded:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `encoded` | `str` | Base64-encoded string |

| Return | Type | Description |
|--------|------|-------------|
| `decoded` | `str` | Decoded string |

**Example:**

```qd
"SGVsbG8=" base64::decode  // text
```

---

### encode

Encode a string to base64.

**Signature:** `( input:str -- encoded:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `input` | `str` | String to encode |

| Return | Type | Description |
|--------|------|-------------|
| `encoded` | `str` | Base64-encoded string |

**Example:**

```qd
"Hello" base64::encode  // b64
```
