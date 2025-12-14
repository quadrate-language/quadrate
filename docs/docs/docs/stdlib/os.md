# os

Operating system interface.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrExists` | `17` | Error: File already exists (EEXIST). |
| `ErrInvalidArg` | `22` | Error: Invalid argument (EINVAL). |
| `ErrIo` | `5` | Error: I/O error (EIO). |
| `ErrIsDirectory` | `21` | Error: Is a directory when file expected (EISDIR). |
| `ErrNameTooLong` | `36` | Error: File name too long (ENAMETOOLONG). |
| `ErrNoSpace` | `28` | Error: No space left on device (ENOSPC). |
| `ErrNone` | `0` | Error: No error (success). |
| `ErrNotDirectory` | `20` | Error: Path component is not a directory (ENOTDIR). |
| `ErrNotFound` | `2` | Error: No such file or directory (ENOENT). |
| `ErrOutOfMemory` | `12` | Error: Out of memory (ENOMEM). |
| `ErrPermission` | `13` | Error: Permission denied (EACCES). |
| `ErrReadOnly` | `30` | Error: Read-only file system (EROFS). |

## Functions

### copy

Copy a file.

**Signature:** `( srcpath:str dstpath:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `srcpath` | `str` | Source path |
| `dstpath` | `str` | Destination path |

**Errors:**

- File not found
- Permission denied

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

- File not found
- Permission denied

**Example:**

```qd
"/tmp/test.txt" os::delete!
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

- File not found
- Not a directory

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

- File exists
- Permission denied

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

- File not found
- Permission denied

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
