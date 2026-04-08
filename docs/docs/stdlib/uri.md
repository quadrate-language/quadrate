# `use` uri

URI encoding, decoding, and parsing.
Handles percent-encoding and URI component extraction.

## Functions

### `fn` (u:Uri) build

Build a URI string from components.

**Signature:** `(u:Uri) build( -- s:str)`

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | URI string |

**Example:**

```qd
u uri::build  // url
```
---

### `fn` decode

Decode a percent-encoded string.

**Signature:** `(s:str -- decoded:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Percent-encoded string |

| Output | Type | Description |
|--------|------|-------------|
| `decoded` | `str` | Decoded string |

**Example:**

```qd
"hello%20world" uri::decode  // decoded
```
---

### `fn` encode

Percent-encode a string for use in URIs.

**Signature:** `(s:str -- encoded:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to encode |

| Output | Type | Description |
|--------|------|-------------|
| `encoded` | `str` | Percent-encoded string |

**Example:**

```qd
"hello world" uri::encode  // encoded
```
---

### `fn` query_get

Get a query parameter value by key.

**Signature:** `(query:str key:str -- value:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `query` | `str` | Query string (without leading '?') |
| `key` | `str` | Parameter name to find |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Parameter value (empty if not found) |

**Example:**

```qd
"foo=bar&x=1" "foo" uri::query_get  // val
```
---

### `fn` query_has

Check if a query parameter exists.

**Signature:** `(query:str key:str -- exists:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `query` | `str` | Query string (without leading '?') |
| `key` | `str` | Parameter name to find |

| Output | Type | Description |
|--------|------|-------------|
| `exists` | `i64` | 1 if exists, 0 otherwise |

**Example:**

```qd
"foo=bar&x=1" "foo" uri::query_has  // exists
```
## Uri

Parsed URI components.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `scheme` | `str` | URI scheme (e.g., "https") |
| `host` | `str` | Host name |
| `port` | `i64` | Port number (0 if not specified) |
| `path` | `str` | Path component |
| `query` | `str` | Query string (without leading "?") |
| `fragment` | `str` | Fragment (without leading "#") |

### Constructors

#### `fn` parse

Parse a URI string into components.

**Signature:** `(s:str -- u:Uri)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | URI string to parse |

| Output | Type | Description |
|--------|------|-------------|
| `u` | `Uri` | Parsed URI struct |

**Example:**

```qd
"https://example.com:8080/path?q=1#top" uri::parse  // u
```

