# `use` sb

StringBuilder - Efficient string building.
Avoids O(n²) cost of repeated strings::concat.

## StringBuilder

Growable string buffer for efficient string building.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `buf` | `ptr` | Internal buffer |
| `len` | `i64` | Current length |
| `cap` | `i64` | Buffer capacity |

### Constructors

#### `fn` new

Create a new StringBuilder with default capacity.

**Signature:** `( -- sb:StringBuilder)`

| Output | Type | Description |
|--------|------|-------------|
| `sb` | `StringBuilder` | New empty builder |

**Example:**

```qd
sb::new  // builder
```
---

#### `fn` with_capacity

Create a StringBuilder with specific initial capacity.

**Signature:** `(capacity:i64 -- sb:StringBuilder)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `capacity` | `i64` | Initial buffer size |

| Output | Type | Description |
|--------|------|-------------|
| `sb` | `StringBuilder` | New empty builder |

**Example:**

```qd
1024 sb::with_capacity  // builder
```

### Methods

#### `fn` append_char

Append a single character (by code point).

**Signature:** `(sb:StringBuilder) append_char(c:i64 -- sb2:StringBuilder)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code to append |

| Output | Type | Description |
|--------|------|-------------|
| `sb2` | `StringBuilder` | Updated builder |

**Example:**

```qd
builder 65 sb::append_char  // builder
```
---

#### `fn` append_int

Append an integer as string.

**Signature:** `(sb:StringBuilder) append_int(n:i64 -- sb2:StringBuilder)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Integer to append |

| Output | Type | Description |
|--------|------|-------------|
| `sb2` | `StringBuilder` | Updated builder |

**Example:**

```qd
builder 42 sb::append_int  // builder
```
---

#### `fn` append

Append a string to the builder.

**Signature:** `(sb:StringBuilder) append(s:str -- sb2:StringBuilder)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to append |

| Output | Type | Description |
|--------|------|-------------|
| `sb2` | `StringBuilder` | Updated builder |

**Example:**

```qd
builder "hello" sb::append  // builder
```
---

#### `fn` build

Build the final string (does not consume builder).

**Signature:** `(sb:StringBuilder) build( -- s:str)`

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | Built string |

**Example:**

```qd
builder sb::build  // result
```
---

#### `fn` capacity

Get the current capacity of the builder.

**Signature:** `(sb:StringBuilder) capacity( -- cap:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `cap` | `i64` | Current buffer capacity |

**Example:**

```qd
builder sb::capacity  // cap
```
---

#### `fn` finish

Build string and free builder in one call.

**Signature:** `(sb:StringBuilder) finish( -- s:str)`

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | Built string |

**Example:**

```qd
builder sb::finish  // result
```
---

#### `fn` free

Free the builder's resources.

**Signature:** `(sb:StringBuilder) free()`

**Example:**

```qd
builder sb::free
```
---

#### `fn` is_empty

Check if the builder is empty.

**Signature:** `(sb:StringBuilder) is_empty( -- empty:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `empty` | `i64` | 1 if empty, 0 otherwise |

**Example:**

```qd
builder sb::is_empty  // empty
```
---

#### `fn` len

Get current length of builder content.

**Signature:** `(sb:StringBuilder) len( -- sblen:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `sblen` | `i64` | Current content length |

**Example:**

```qd
builder sb::len  // sblen
```
---

#### `fn` newline

Append a newline character.

**Signature:** `(sb:StringBuilder) newline( -- sb2:StringBuilder)`

| Output | Type | Description |
|--------|------|-------------|
| `sb2` | `StringBuilder` | Updated builder |

**Example:**

```qd
builder sb::newline  // builder
```
---

#### `fn` space

Append a space character.

**Signature:** `(sb:StringBuilder) space( -- sb2:StringBuilder)`

| Output | Type | Description |
|--------|------|-------------|
| `sb2` | `StringBuilder` | Updated builder |

**Example:**

```qd
builder sb::space  // builder
```

