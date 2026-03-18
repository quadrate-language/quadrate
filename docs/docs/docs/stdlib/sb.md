# `use` sb

StringBuilder - Efficient string building.
Avoids O(n²) cost of repeated strings::concat.

## Functions

### `fn` append_char

Append a single character (by code point).

**Signature:** `(sb:ptr c:i64 -- sb:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to append to |
| `c` | `i64` | Character code to append |

| Output | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | Updated builder |

**Example:**

```qd
builder 65 sb::append_char  // builder
```
---

### `fn` append_int

Append an integer as string.

**Signature:** `(sb:ptr n:i64 -- sb:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to append to |
| `n` | `i64` | Integer to append |

| Output | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | Updated builder |

**Example:**

```qd
builder 42 sb::append_int  // builder
```
---

### `fn` append

Append a string to the builder.

**Signature:** `(sb:ptr s:str -- sb:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to append to |
| `s` | `str` | String to append |

| Output | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | Updated builder |

**Example:**

```qd
builder "hello" sb::append  // builder
```
---

### `fn` build

Build the final string (does not consume builder).

**Signature:** `(sb:ptr -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to convert |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | Built string |

**Example:**

```qd
builder sb::build  // result
```
---

### `fn` capacity

Get the current capacity of the builder.

**Signature:** `(sb:ptr -- cap:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to check |

| Output | Type | Description |
|--------|------|-------------|
| `cap` | `i64` | Current buffer capacity |

**Example:**

```qd
builder sb::capacity  // cap
```
---

### `fn` finish

Build string and free builder in one call.

**Signature:** `(sb:ptr -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to finish |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | Built string |

**Example:**

```qd
builder sb::finish  // result
```
---

### `fn` free

Free the builder's resources.

**Signature:** `(sb:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to free |

**Example:**

```qd
builder sb::free
```
---

### `fn` is_empty

Check if the builder is empty.

**Signature:** `(sb:ptr -- empty:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to check |

| Output | Type | Description |
|--------|------|-------------|
| `empty` | `i64` | 1 if empty, 0 otherwise |

**Example:**

```qd
builder sb::is_empty  // empty
```
---

### `fn` len

Get current length of builder content.

**Signature:** `(sb:ptr -- sblen:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to check |

| Output | Type | Description |
|--------|------|-------------|
| `sblen` | `i64` | Current content length |

**Example:**

```qd
builder sb::len  // sblen
```
---

### `fn` newline

Append a newline character.

**Signature:** `(sb:ptr -- sb:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to append to |

| Output | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | Updated builder |

**Example:**

```qd
builder sb::newline  // builder
```
---

### `fn` new

Create a new StringBuilder with default capacity.

**Signature:** `( -- sb:ptr)`

| Output | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | New empty builder |

**Example:**

```qd
sb::new  // builder
```
---

### `fn` space

Append a space character.

**Signature:** `(sb:ptr -- sb:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sb` | `ptr` | Builder to append to |

| Output | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | Updated builder |

**Example:**

```qd
builder sb::space  // builder
```
---

### `fn` with_capacity

Create a StringBuilder with specific initial capacity.

**Signature:** `(capacity:i64 -- sb:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `capacity` | `i64` | Initial buffer size |

| Output | Type | Description |
|--------|------|-------------|
| `sb` | `ptr` | New empty builder |

**Example:**

```qd
1024 sb::with_capacity  // builder
```
## StringBuilder

Growable string buffer for efficient string building.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `buf` | `ptr` | Internal buffer |
| `len` | `i64` | Current length |
| `cap` | `i64` | Buffer capacity |

