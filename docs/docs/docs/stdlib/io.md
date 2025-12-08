# io

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `Append` | `"a"` | Open mode: append. |
| `AppendBinary` | `"ab"` | Open mode: append binary. |
| `AppendRead` | `"a+"` | Open mode: append and read. |
| `AppendReadBinary` | `"ab+"` | Open mode: append and read binary. |
| `Read` | `"r"` | File and stream I/O operations. Open mode: read only. |
| `ReadBinary` | `"rb"` | Open mode: read binary. |
| `ReadWrite` | `"r+"` | Open mode: read and write. |
| `ReadWriteBinary` | `"rb+"` | Open mode: read and write binary. |
| `SeekCur` | `1` | Seek from current position. |
| `SeekEnd` | `2` | Seek from end of file. |
| `SeekSet` | `0` | Seek from beginning of file. |
| `Write` | `"w"` | Open mode: write (truncate). |
| `WriteBinary` | `"wb"` | Open mode: write binary. |
| `WriteRead` | `"w+"` | Open mode: write and read (truncate). |
| `WriteReadBinary` | `"wb+"` | Open mode: write and read binary. |

## Functions

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

### eof

Check if at end of file.

**Signature:** `( handle:ptr -- handle:ptr is_eof:i64 )`

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

### open

Open a file.

**Signature:** `( path:str mode:str -- handle:ptr )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File path |
| `mode` | `str` | Open mode (use io::Read, io::Write, etc.) |

| Output | Type | Description |
|--------|------|-------------|
| `handle` | `ptr` | File handle |

**Errors:**

- File not found
- Permission denied

**Example:**

```qd
"data.txt" io::Read io::open!  // f
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

| Output | Type | Description |
|--------|------|-------------|
| `bytes_read` | `i64` | Actual bytes read |

**Errors:**

- Read failed

**Example:**

```qd
f buf 1024 io::read!  // n
```

---

### readline

Read a line from stdin.

**Signature:** `( -- line:str )!`

| Output | Type | Description |
|--------|------|-------------|
| `line` | `str` | Line without trailing newline |

**Errors:**

- Read failed

**Example:**

```qd
io::readline!  // input
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

| Output | Type | Description |
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

| Output | Type | Description |
|--------|------|-------------|
| `position` | `i64` | Current position |

**Errors:**

- Tell failed

**Example:**

```qd
f io::tell!  // pos
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

| Output | Type | Description |
|--------|------|-------------|
| `bytes_written` | `i64` | Actual bytes written |

**Errors:**

- Write failed

**Example:**

```qd
f buf len io::write! drop
```
