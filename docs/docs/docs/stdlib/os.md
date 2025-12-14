# os

Operating system interface.

Error codes: `Ok` (1) for success, specific errors start at 2.

## Constants

### Error Codes

| Name | Value | Description |
|------|-------|-------------|
| `ErrNotFound` | `2` | Error: No such file or directory. |
| `ErrPermission` | `3` | Error: Permission denied. |
| `ErrExists` | `4` | Error: File already exists. |
| `ErrNotDirectory` | `5` | Error: Path is not a directory. |
| `ErrIsDirectory` | `6` | Error: Path is a directory. |
| `ErrIo` | `7` | Error: I/O error. |
| `ErrNoSpace` | `8` | Error: No space left on device. |
| `ErrReadOnly` | `9` | Error: Read-only file system. |
| `ErrNameTooLong` | `10` | Error: File name too long. |
| `ErrOutOfMemory` | `11` | Error: Out of memory. |
| `ErrInvalidArg` | `12` | Error: Invalid argument. |

## Functions

### copy

Copy a file.

**Signature:** `( srcpath:str dstpath:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `srcpath` | `str` | Source path |
| `dstpath` | `str` | Destination path |

**Errors:**

| Code | Description |
|------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"src.txt" "dst.txt" os::copy!
```

---

### delete

Delete a file or empty directory.

**Signature:** `( path:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Path to delete |

**Errors:**

- `os::ErrNotFound` - File not found
- `os::ErrPermission` - Permission denied

**Example (with !):**

```qd
"/tmp/test.txt" os::delete!
```

**Example (with switch):**

```qd
"/tmp/test.txt" os::delete switch {
	Ok {
		"File deleted" print nl
	}
	os::ErrNotFound {
		"File not found" print nl
	}
	os::ErrPermission {
		"Permission denied" print nl
	}
	_ {
		"Delete failed" print nl
	}
}
```

---

### exists

Check if path exists.

**Signature:** `( path:str -- exists:i64 )`

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

### exit

Exit the program with status code.

**Signature:** `( code:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `code` | `i64` | Exit status (0 = success) |

**Example:**

```qd
0 os::exit
```

---

### getenv

Get environment variable value.

**Signature:** `( name:str -- value:str )`

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

### list

List directory contents.

**Signature:** `( path:str -- entries:ptr count:i64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Directory path |

| Output | Type | Description |
|--------|------|-------------|
| `entries` | `ptr` | Array of entry names |
| `count` | `i64` | Number of entries |

**Errors:**

| Code | Description |
|------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrNotDirectory` | Path is not a directory |

**Example:**

```qd
"/tmp" os::list! -> entries  // count
```

---

### mkdir

Create a directory.

**Signature:** `( path:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Directory path |

**Errors:**

| Code | Description |
|------|-------------|
| `os::ErrExists` | File already exists |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"/tmp/newdir" os::mkdir!
```

---

### rename

Rename or move a file.

**Signature:** `( oldpath:str newpath:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `oldpath` | `str` | Current path |
| `newpath` | `str` | New path |

**Errors:**

| Code | Description |
|------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"old.txt" "new.txt" os::rename!
```

---

### setenv

Set environment variable.

**Signature:** `( name:str value:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `str` | Variable name |
| `value` | `str` | Variable value |

**Example:**

```qd
"MY_VAR" "hello" os::setenv
```

---

### system

Execute a shell command.

**Signature:** `( cmd:str -- exitcode:i64 )`

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
