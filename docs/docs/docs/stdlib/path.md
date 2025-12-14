# `use` path

File path manipulation functions.
POSIX-style paths with forward slash separator.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `SEP` | `"/"` | Path separator constant. |

## Functions

### `fn` basename

Get the filename part of a path.

**Signature:** `( path:str -- name:str )`

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

### `fn` dirname

Get the directory part of a path.

**Signature:** `( path:str -- dir:str )`

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

**Signature:** `( path:str -- ext:str )`

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

### `fn` is_absolute

Check if path is absolute.

**Signature:** `( path:str -- result:i64 )`

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

### `fn` join

Join two path components.

**Signature:** `( p1:str p2:str -- joined:str )`

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

**Signature:** `( path:str -- norm:str )`

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

### `fn` stem

Get filename without extension.

**Signature:** `( path:str -- stem:str )`

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

