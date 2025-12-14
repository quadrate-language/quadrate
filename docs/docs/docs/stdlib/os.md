# `use` os

Operating system interface.
Error codes: Ok=1 (success), specific errors start at 2

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrExists` | `4` | Error: File already exists. |
| `ErrInvalidArg` | `12` | Error: Invalid argument. |
| `ErrIo` | `7` | Error: I/O error. |
| `ErrIsDirectory` | `6` | Error: Path is a directory. |
| `ErrNameTooLong` | `10` | Error: File name too long. |
| `ErrNoSpace` | `8` | Error: No space left on device. |
| `ErrNotDirectory` | `5` | Error: Path is not a directory. |
| `ErrNotFound` | `2` | Error: No such file or directory. |
| `ErrOutOfMemory` | `11` | Error: Out of memory. |
| `ErrPermission` | `3` | Error: Permission denied. |
| `ErrReadOnly` | `9` | Error: Read-only file system. |
| `ExitFailure` | `1` | Exit code for failed termination. |
| `ExitSuccess` | `0` | Exit code for successful termination. |

## Functions

### `fn` copy

Copy a file.

**Signature:** `(srcpath:str dstpath:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `srcpath` | `str` | Source path |
| `dstpath` | `str` | Destination path |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"src.txt" "dst.txt" os::copy!
```

---

### `fn` delete

Delete a file or empty directory.

**Signature:** `(path:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Path to delete |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"/tmp/test.txt" os::delete!
```

---

### `fn` exists

Check if path exists.

**Signature:** `(path:str -- exists:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File or directory path |

| Output | Type | Description |
|--------|------|-------------|
| `exists` | `i64` | 1 if exists, 0 otherwise |

**Example:**

```qd
"/tmp" os::exists .  // 1
```

---

### `fn` exit

Exit the program with status code.

**Signature:** `(code:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `code` | `i64` | Exit status (os::ExitSuccess or os::ExitFailure) |

**Example:**

```qd
os::ExitSuccess os::exit
```

---

### `fn` getenv

Get environment variable value.

**Signature:** `(name:str -- value:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `str` | Variable name |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Variable value (empty if not set) |

**Example:**

```qd
"HOME" os::getenv  // home
```

---

### `fn` list

List directory contents.

**Signature:** `(path:str -- entries:ptr count:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Directory path |

| Output | Type | Description |
|--------|------|-------------|
| `entries` | `ptr` | Array of entry names |
| `count` | `i64` | Number of entries |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrNotDirectory` | Path is not a directory |

**Example:**

```qd
"/tmp" os::list! -> entries  // count
```

---

### `fn` mkdir

Create a directory.

**Signature:** `(path:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Directory path |

| Error | Description |
|-------|-------------|
| `os::ErrExists` | File already exists |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"/tmp/newdir" os::mkdir!
```

---

### `fn` rename

Rename or move a file.

**Signature:** `(oldpath:str newpath:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `oldpath` | `str` | Current path |
| `newpath` | `str` | New path |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"old.txt" "new.txt" os::rename!
```

---

### `fn` setenv

Set environment variable.

**Signature:** `(name:str value:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `str` | Variable name |
| `value` | `str` | Variable value |

**Example:**

```qd
"MY_VAR" "hello" os::setenv
```

---

### `fn` system

Execute a shell command.

**Signature:** `(cmd:str -- exitcode:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `cmd` | `str` | Command to execute |

| Output | Type | Description |
|--------|------|-------------|
| `exitcode` | `i64` | Command exit status |

**Example:**

```qd
"ls -la" os::system  // code
```

