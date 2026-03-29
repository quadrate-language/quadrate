# `use` fmt

Formatted output functions.

## Functions

### `fn` eprintln

Print a string to stderr followed by a newline.

**Signature:** `(s:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to print |

**Example:**

```qd
"error occurred" fmt::eprintln
```
---

### `fn` fprintf

Print formatted output to a file descriptor.

**Signature:** `(fd:i64 format:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `fd` | `i64` | File descriptor (1=stdout, 2=stderr) |
| `format` | `str` | Format string with % specifiers |

**Example:**

```qd
"error: %s\n" "not found" 2 fmt::fprintf
```
---

### `fn` printf

Print formatted output to stdout.

**Signature:** `(format:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `format` | `str` | Format string with % specifiers |

**Example:**

```qd
"world" "Hello %s\n" fmt::printf  // Hello world
```
---

### `fn` println

Print a string to stdout followed by a newline.

**Signature:** `(s:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to print |

**Example:**

```qd
"hello" fmt::println  // hello\n
```
---

### `fn` print

Print a string to stdout (no newline appended).

**Signature:** `(s:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to print |

**Example:**

```qd
"hello" fmt::print
```
---

### `fn` sprintf

Format a string with printf-style specifiers.

**Signature:** `(format:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `format` | `str` | Format string with % specifiers |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Formatted string |

**Example:**

```qd
"world" "Hello %s" fmt::sprintf  // "Hello world"
```
---

### `fn` sprintln

Append a newline to a string and return the result.

**Signature:** `(s:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | String with newline appended |

**Example:**

```qd
"hello" fmt::sprintln  // "hello\n"
```
