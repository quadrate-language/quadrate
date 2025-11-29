# io

File and stream I/O operations.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `Read` | `"r"` | Open mode: read only. |
| `ReadBinary` | `"rb"` | Open mode: read binary. |
| `Write` | `"w"` | Open mode: write (truncate). |
| `WriteBinary` | `"wb"` | Open mode: write binary. |
| `Append` | `"a"` | Open mode: append. |
| `AppendBinary` | `"ab"` | Open mode: append binary. |
| `ReadWrite` | `"r+"` | Open mode: read and write. |
| `ReadWriteBinary` | `"rb+"` | Open mode: read and write binary. |
| `WriteRead` | `"w+"` | Open mode: write and read (truncate). |
| `WriteReadBinary` | `"wb+"` | Open mode: write and read binary. |
| `AppendRead` | `"a+"` | Open mode: append and read. |
| `AppendReadBinary` | `"ab+"` | Open mode: append and read binary. |
| `SeekSet` | `0` | Seek from beginning of file. |
| `SeekCur` | `1` | Seek from current position. |
| `SeekEnd` | `2` | Seek from end of file. |

## Functions

### open

Open a file.

**Signature:** `( path:str mode:str -- handle:ptr )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |
| `mode` | `str` | Open mode (use io::Read, io::Write, etc.) |

| Return | Type | Description |
|--------|------|-------------|
| `handle` | `ptr` | File handle |

**Errors:**

- File not found
- Permission denied

**Example:**

```qd
"data.txt" io::Read io::open! -> f
```

---

### close

Close a file.

**Signature:** `( handle:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle to close |

**Example:**

```qd
f io::close
```

---

### read

Read bytes into buffer.

**Signature:** `( handle:ptr buffer:ptr count:i64 -- bytes_read:i64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle |
| `buffer` | `ptr` | Pre-allocated buffer |
| `count` | `i64` | Maximum bytes to read |

| Return | Type | Description |
|--------|------|-------------|
| `bytes_read` | `i64` | Actual bytes read |

**Errors:**

- Read failed

**Example:**

```qd
f buf 1024 io::read! -> n
```

---

### write

Write bytes from buffer.

**Signature:** `( handle:ptr buffer:ptr count:i64 -- bytes_written:i64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle |
| `buffer` | `ptr` | Buffer containing data |
| `count` | `i64` | Number of bytes to write |

| Return | Type | Description |
|--------|------|-------------|
| `bytes_written` | `i64` | Actual bytes written |

**Errors:**

- Write failed

**Example:**

```qd
f buf len io::write! drop
```

---

### seek

Seek to position in file.

**Signature:** `( handle:ptr offset:i64 whence:i64 -- position:i64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle |
| `offset` | `i64` | Offset in bytes |
| `whence` | `i64` | Reference point (SeekSet, SeekCur, SeekEnd) |

| Return | Type | Description |
|--------|------|-------------|
| `position` | `i64` | New position |

**Errors:**

- Seek failed

**Example:**

```qd
f 0 io::SeekSet io::seek! drop
```

---

### tell

Get current position in file.

**Signature:** `( handle:ptr -- position:i64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle |

| Return | Type | Description |
|--------|------|-------------|
| `position` | `i64` | Current position |

**Errors:**

- Tell failed

**Example:**

```qd
f io::tell! -> pos
```

---

### eof

Check if at end of file.

**Signature:** `( handle:ptr -- handle:ptr is_eof:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `handle` | `ptr` | File handle |

| Return | Type | Description |
|--------|------|-------------|
| `handle` | `ptr` | File handle (unchanged) |
| `is_eof` | `i64` | 1 if at EOF, 0 otherwise |

**Example:**

```qd
f io::eof -> f -> at_end
```

---

### readline

Read a line from stdin.

**Signature:** `( -- line:str )!`

| Return | Type | Description |
|--------|------|-------------|
| `line` | `str` | Line without trailing newline |

**Errors:**

- Read failed

**Example:**

```qd
io::readline! -> input
```
