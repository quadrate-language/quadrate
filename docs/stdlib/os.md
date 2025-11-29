# os

Operating system interface.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `Success` | `0` | Error: No error. |
| `PermissionDenied` | `13` | Error: Permission denied (EACCES). |
| `FileExists` | `17` | Error: File already exists (EEXIST). |
| `NotADirectory` | `20` | Error: Path component is not a directory (ENOTDIR). |
| `IsADirectory` | `21` | Error: Is a directory when file expected (EISDIR). |
| `InvalidArgument` | `22` | Error: Invalid argument (EINVAL). |
| `FileNotFound` | `2` | Error: No such file or directory (ENOENT). |
| `NoSpaceLeft` | `28` | Error: No space left on device (ENOSPC). |
| `ReadOnlyFileSystem` | `30` | Error: Read-only file system (EROFS). |
| `NameTooLong` | `36` | Error: File name too long (ENAMETOOLONG). |
| `IoError` | `5` | Error: I/O error (EIO). |
| `OutOfMemory` | `12` | Error: Out of memory (ENOMEM). |

## Functions

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

### system

Execute a shell command.

**Signature:** `( cmd:str -- exitcode:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `cmd` | `str` | Command to execute |

| Return | Type | Description |
|--------|------|-------------|
| `exitcode` | `i64` | Command exit status |

**Example:**

```qd
"ls -la" os::system -> code
```

---

### getenv

Get environment variable value.

**Signature:** `( name:str -- value:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `str` | Variable name |

| Return | Type | Description |
|--------|------|-------------|
| `value` | `str` | Variable value (empty if not set) |

**Example:**

```qd
"HOME" os::getenv -> home
```

---

### exists

Check if path exists.

**Signature:** `( path:str -- exists:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File or directory path |

| Return | Type | Description |
|--------|------|-------------|
| `exists` | `i64` | 1 if exists, 0 otherwise |

**Example:**

```qd
"/tmp" os::exists .  // 1
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

### list

List directory contents.

**Signature:** `( path:str -- entries:ptr count:i64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Directory path |

| Return | Type | Description |
|--------|------|-------------|
| `entries` | `ptr` | Array of entry names |
| `count` | `i64` | Number of entries |

**Errors:**

- File not found
- Not a directory

**Example:**

```qd
"/tmp" os::list! -> entries -> count
```
