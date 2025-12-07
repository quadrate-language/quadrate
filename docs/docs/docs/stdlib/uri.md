# uri

URI encoding, decoding, and parsing.
Handles percent-encoding and URI component extraction.

## Structs

### Uri

Parsed URI components.

## Functions

### build

Build a URI string from components.

**Signature:** `( u:ptr -- s:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `u` | `Uri` | URI struct |

| Return | Type | Description |
|--------|------|-------------|
| `s` | `str` | URI string |

**Example:**

```qd
u uri::build  // url
```

---

### decode

Decode a percent-encoded string.

**Signature:** `( s:str -- decoded:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Percent-encoded string |

| Return | Type | Description |
|--------|------|-------------|
| `decoded` | `str` | Decoded string |

**Example:**

```qd
"hello%20world" uri::decode  // decoded
```

---

### encode

Percent-encode a string for use in URIs.

**Signature:** `( s:str -- encoded:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to encode |

| Return | Type | Description |
|--------|------|-------------|
| `encoded` | `str` | Percent-encoded string |

**Example:**

```qd
"hello world" uri::encode  // encoded
```

---

### parse

Parse a URI string into components.

**Signature:** `( s:str -- u:ptr )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | URI string to parse |

| Return | Type | Description |
|--------|------|-------------|
| `u` | `Uri` | Parsed URI struct |

**Example:**

```qd
"https://example.com:8080/path?q=1#top" uri::parse  // u
```

---

### query_get

Get a query parameter value by key.

**Signature:** `( query:str key:str -- value:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `query` | `str` | Query string (without leading '?') |
| `key` | `str` | Parameter name to find |

| Return | Type | Description |
|--------|------|-------------|
| `value` | `str` | Parameter value (empty if not found) |

**Example:**

```qd
"foo=bar&x=1" "foo" uri::query_get  // val
```

---

### query_has

Check if a query parameter exists.

**Signature:** `( query:str key:str -- exists:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `query` | `str` | Query string (without leading '?') |
| `key` | `str` | Parameter name to find |

| Return | Type | Description |
|--------|------|-------------|
| `exists` | `i64` | 1 if exists, 0 otherwise |

**Example:**

```qd
"foo=bar&x=1" "foo" uri::query_has  // exists
```
