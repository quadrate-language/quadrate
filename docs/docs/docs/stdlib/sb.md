# sb

StringBuilder - Efficient string building.
Avoids O(n²) cost of repeated str::concat.

## Structs

### StringBuilder

StringBuilder struct holds buffer, length, and capacity.

## Functions

### append

Append a string to the builder.

**Signature:** `( sb:ptr s:str -- sb:ptr )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to append to |
| `s` | `str` | String to append |

| Return | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | Updated builder |

**Example:**

```qd
builder "hello" sb::append  // builder
```

---

### append_char

Append a single character (by code point).

**Signature:** `( sb:ptr c:i64 -- sb:ptr )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to append to |
| `c` | `i64` | Character code to append |

| Return | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | Updated builder |

**Example:**

```qd
builder 65 sb::append_char  // builder
```

---

### append_int

Append an integer as string.

**Signature:** `( sb:ptr n:i64 -- sb:ptr )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to append to |
| `n` | `i64` | Integer to append |

| Return | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | Updated builder |

**Example:**

```qd
builder 42 sb::append_int  // builder
```

---

### build

Build the final string (does not consume builder).

**Signature:** `( sb:ptr -- s:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to convert |

| Return | Type | Description |
|--------|------|-------------|
| `s` | `str` | Built string |

**Example:**

```qd
builder sb::build  // result
```

---

### finish

Build string and free builder in one call.

**Signature:** `( sb:ptr -- s:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to finish |

| Return | Type | Description |
|--------|------|-------------|
| `s` | `str` | Built string |

**Example:**

```qd
builder sb::finish  // result
```

---

### free

Free the builder's resources.

**Signature:** `( sb:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to free |

**Example:**

```qd
builder sb::free
```

---

### length

Get current length of builder content.

**Signature:** `( sb:ptr -- sblen:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to check |

| Return | Type | Description |
|--------|------|-------------|
| `sblen` | `i64` | Current content length |

**Example:**

```qd
builder sb::length  // sblen
```

---

### new

Create a new StringBuilder with default capacity.

**Signature:** `( -- sb:ptr )`

| Return | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | New empty builder |

**Example:**

```qd
sb::new  // builder
```

---

### with_capacity

Create a StringBuilder with specific initial capacity.

**Signature:** `( capacity:i64 -- sb:ptr )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `capacity` | `i64` | Initial buffer size |

| Return | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | New empty builder |

**Example:**

```qd
1024 sb::with_capacity  // builder
```
