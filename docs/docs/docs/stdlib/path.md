# `use` path

File path manipulation functions.
POSIX-style paths with forward slash separator.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `Sep` | `"/"` | Path separator constant. |

## Functions

### `fn` basename

Get the filename part of a path.

**Signature:** `(path:str -- name:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `name` | `str` | Filename component |

**Example:**

```qd
"/home/user/file.txt" path::basename  // "file.txt"
```
---

### `fn` depth

Get the depth of a path (number of components).

**Signature:** `(path:str -- depth:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `depth` | `i64` | Number of path components |

**Example:**

```qd
"/home/user/file.txt" path::depth  // 3
```
---

### `fn` dirname

Get the directory part of a path.

**Signature:** `(path:str -- dir:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `dir` | `str` | Directory component |

**Example:**

```qd
"/home/user/file.txt" path::dirname  // "/home/user"
```
---

### `fn` ext

Get the file extension (including dot).

**Signature:** `(path:str -- ext:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `ext` | `str` | Extension including dot, empty if none |

**Example:**

```qd
"file.txt" path::ext  // ".txt"
```
---

### `fn` has_extension

Check if a path matches a simple extension filter.

**Signature:** `(path:str extension:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |
| `extension` | `str` | Extension to match (with or without dot) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if matches, 0 otherwise |

**Example:**

```qd
"file.txt" "txt" path::has_extension  // 1
```
---

### `fn` has_ext

Check if path has an extension.

**Signature:** `(path:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if has extension, 0 otherwise |

**Example:**

```qd
"file.txt" path::has_ext  // 1
```
---

### `fn` is_absolute

Check if path is absolute.

**Signature:** `(path:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if absolute, 0 otherwise |

**Example:**

```qd
"/home/user" path::is_absolute  // 1
```
---

### `fn` is_relative

Check if path is relative (not absolute).

**Signature:** `(path:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if relative, 0 otherwise |

**Example:**

```qd
"home/user" path::is_relative  // 1
```
---

### `fn` join

Join two path components.

**Signature:** `(p1:str p2:str -- joined:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `p1` | `str` | First path |
| `p2` | `str` | Second path |

| Output | Type | Description |
|--------|------|-------------|
| `joined` | `str` | Joined path |

**Example:**

```qd
"/home" "user" path::join  // "/home/user"
```
---

### `fn` normalize

Normalize a path (remove redundant separators).

**Signature:** `(path:str -- norm:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `norm` | `str` | Normalized path |

**Example:**

```qd
"/home//user/" path::normalize  // "/home/user"
```
---

### `fn` parent

Get parent directory (same as dirname but clearer name).

**Signature:** `(path:str -- parent:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `parent` | `str` | Parent directory |

**Example:**

```qd
"/home/user/file.txt" path::parent  // "/home/user"
```
---

### `fn` starts_with

Check if path starts with a given prefix path.

**Signature:** `(path:str prefix:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path to check |
| `prefix` | `str` | Prefix path |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if starts with prefix, 0 otherwise |

**Example:**

```qd
"/home/user/file.txt" "/home" path::starts_with  // 1
```
---

### `fn` stem

Get filename without extension.

**Signature:** `(path:str -- stem:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `stem` | `str` | Filename without extension |

**Example:**

```qd
"file.txt" path::stem  // "file"
```
---

### `fn` strip_prefix

Remove a prefix from a path if it exists.

**Signature:** `(path:str prefix:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |
| `prefix` | `str` | Prefix to remove |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Path without prefix |

**Example:**

```qd
"/home/user/file.txt" "/home" path::strip_prefix  // "user/file.txt"
```
---

### `fn` with_ext

Change or add file extension.

**Signature:** `(path:str newext:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |
| `newext` | `str` | New extension (with or without leading dot) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Path with new extension |

**Example:**

```qd
"file.txt" ".md" path::with_ext  // "file.md"
```
