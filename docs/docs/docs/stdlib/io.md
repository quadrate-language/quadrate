# `use` io

File and stream I/O operations.
Error codes: Ok=1 (success), specific errors start at 2

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `Append` | `"a"` | Open mode: append. |
| `AppendBinary` | `"ab"` | Open mode: append binary. |
| `AppendRead` | `"a+"` | Open mode: append and read. |
| `AppendReadBinary` | `"ab+"` | Open mode: append and read binary. |
| `ErrEof` | `8` | Error: End of file reached. |
| `ErrInvalidArg` | `9` | Error: Invalid argument. |
| `ErrInvalidHandle` | `4` | Error: Invalid file handle. |
| `ErrNotFound` | `2` | Error: File not found. |
| `ErrPermission` | `3` | Error: Permission denied. |
| `ErrRead` | `5` | Error: Read operation failed. |
| `ErrSeek` | `7` | Error: Seek operation failed. |
| `ErrWrite` | `6` | Error: Write operation failed. |
| `ReadBinary` | `"rb"` | Open mode: read binary. |
| `Read` | `"r"` | Open mode: read only. |
| `ReadWriteBinary` | `"rb+"` | Open mode: read and write binary. |
| `ReadWrite` | `"r+"` | Open mode: read and write. |
| `SeekCur` | `1` | Seek from current position. |
| `SeekEnd` | `2` | Seek from end of file. |
| `SeekSet` | `0` | Seek from beginning of file. |
| `WriteBinary` | `"wb"` | Open mode: write binary. |
| `WriteReadBinary` | `"wb+"` | Open mode: write and read binary. |
| `WriteRead` | `"w+"` | Open mode: write and read (truncate). |
| `Write` | `"w"` | Open mode: write (truncate). |

## Functions

### `fn` close

Close a file.

**Signature:** `(handle:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle to close |

**Example:**

```qd
f io::close
```

---

### `fn` eof

Check if at end of file.

**Signature:** `(handle:ptr -- handle:ptr is_eof:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle |

| Output | Type | Description |
|--------|------|-------------|
| `handle` | `ptr` | File handle (unchanged) |
| `is_eof` | `i64` | 1 if at EOF, 0 otherwise |

**Example:**

```qd
f io::eof -> f  // at_end
```

---

### `fn` open

Open a file.

**Signature:** `(path:str mode:str -- handle:ptr)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |
| `mode` | `str` | Open mode (use io::Read, io::Write, etc.) |

| Output | Type | Description |
|--------|------|-------------|
| `handle` | `ptr` | File handle |

| Error | Description |
|-------|-------------|
| `io::ErrNotFound` | File not found |

**Example:**

```qd
"data.txt" io::Read io::open!  // f
```

---

### `fn` read_file

Read entire file contents.

**Signature:** `(path:str -- contents:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |

| Output | Type | Description |
|--------|------|-------------|
| `contents` | `str` | File contents as string |

| Error | Description |
|-------|-------------|
| `io::ErrNotFound` | File not found |
| `io::ErrRead` | Read operation failed |

**Example:**

```qd
"/etc/hostname" io::read_file!  // contents
```

---

### `fn` read

Read bytes into buffer.

**Signature:** `(handle:ptr buffer:ptr count:i64 -- bytes_read:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle |
| `buffer` | `ptr` | Pre-allocated buffer |
| `count` | `i64` | Maximum bytes to read |

| Output | Type | Description |
|--------|------|-------------|
| `bytes_read` | `i64` | Actual bytes read |

| Error | Description |
|-------|-------------|
| `io::ErrInvalidArg` | Invalid file handle, buffer, or count |
| `io::ErrRead` | Read operation failed |

**Example:**

```qd
f buf 1024 io::read!  // n
```

---

### `fn` readline

Read a line from stdin.

**Signature:** `( -- line:str)!`

| Output | Type | Description |
|--------|------|-------------|
| `line` | `str` | Line without trailing newline |

| Error | Description |
|-------|-------------|
| `io::ErrEof` | End of file or read error |

**Example:**

```qd
io::readline!  // input
```

---

### `fn` seek

Seek to position in file.

**Signature:** `(handle:ptr offset:i64 whence:i64 -- position:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle |
| `offset` | `i64` | Offset in bytes |
| `whence` | `i64` | Reference point (SeekSet, SeekCur, SeekEnd) |

| Output | Type | Description |
|--------|------|-------------|
| `position` | `i64` | New position |

| Error | Description |
|-------|-------------|
| `io::ErrInvalidHandle` | Invalid file handle |
| `io::ErrInvalidArg` | Invalid whence value |
| `io::ErrSeek` | Seek operation failed |

**Example:**

```qd
f 0 io::SeekSet io::seek! drop
```

---

### `fn` tell

Get current position in file.

**Signature:** `(handle:ptr -- position:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle |

| Output | Type | Description |
|--------|------|-------------|
| `position` | `i64` | Current position |

| Error | Description |
|-------|-------------|
| `io::ErrInvalidHandle` | Invalid file handle |
| `io::ErrSeek` | Tell operation failed |

**Example:**

```qd
f io::tell!  // pos
```

---

### `fn` write

Write bytes from buffer.

**Signature:** `(handle:ptr buffer:ptr count:i64 -- bytes_written:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle |
| `buffer` | `ptr` | Buffer containing data |
| `count` | `i64` | Number of bytes to write |

| Output | Type | Description |
|--------|------|-------------|
| `bytes_written` | `i64` | Actual bytes written |

| Error | Description |
|-------|-------------|
| `io::ErrInvalidArg` | Invalid file handle, buffer, or count |
| `io::ErrWrite` | Write operation failed |

**Example:**

```qd
f buf len io::write! drop
```

---

### `fn` write_file

Write string contents to file. Creates or truncates the file at the given path.

**Signature:** `(path:str contents:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |
| `contents` | `str` | String to write |

| Error | Description |
|-------|-------------|
| `io::ErrPermission` | Permission denied or cannot create file |
| `io::ErrWrite` | Write operation failed |

**Example:**

```qd
"/tmp/test.txt" "Hello, world!\n" io::write_file!
```

