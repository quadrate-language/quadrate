# mem

Low-level memory allocation and manipulation.

SAFETY: These are unsafe operations with no bounds checking.
Improper use can cause segmentation faults and memory corruption.

## Functions

### alloc

Allocate memory.

**Signature:** `( bytes:i64 -- address:ptr )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `bytes` | `i64` | Number of bytes to allocate |

| Return | Type | Description |
|--------|------|-------------|
| `address` | `ptr` | Allocated memory, or null (0) on failure |

**Example:**

```qd
1024 mem::alloc -> buf
```

---

### copy

Copy bytes between memory regions.

**Signature:** `( src:ptr dst:ptr bytes:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `src` | `ptr` | Source address |
| `dst` | `ptr` | Destination address |
| `bytes` | `i64` | Number of bytes to copy |

**Example:**

```qd
src dst 100 mem::copy
```

---

### fill

Fill memory region with byte value.

**Signature:** `( address:ptr bytes:i64 value:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `bytes` | `i64` | Number of bytes to fill |
| `value` | `i64` | Fill value (0-255) |

**Example:**

```qd
buf 1024 0xFF mem::fill
```

---

### free

Free allocated memory.

**Signature:** `( address:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Memory to free (null is safe) |

**Example:**

```qd
buf mem::free
```

---

### from_string

Convert string to buffer.

**Signature:** `( text:str -- buffer:ptr length:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `text` | `str` | Source string |

| Return | Type | Description |
|--------|------|-------------|
| `buffer` | `ptr` | Allocated buffer |
| `length` | `i64` | String length |

**Example:**

```qd
"hello" mem::from_string -> buf -> len
```

---

### get

Get a 64-bit integer at offset.

**Signature:** `( address:ptr offset:i64 -- value:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

| Return | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Integer value |

**Example:**

```qd
buf 0 mem::get -> n
```

---

### get_byte

Get a byte at offset.

**Signature:** `( address:ptr offset:i64 -- value:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

| Return | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Byte value (0-255) |

**Example:**

```qd
buf 0 mem::get_byte -> b
```

---

### get_float

Get a 64-bit float at offset.

**Signature:** `( address:ptr offset:i64 -- value:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

| Return | Type | Description |
|--------|------|-------------|
| `value` | `f64` | Float value |

**Example:**

```qd
buf 0 mem::get_float -> x
```

---

### get_ptr

Get a pointer at offset.

**Signature:** `( address:ptr offset:i64 -- value:ptr )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

| Return | Type | Description |
|--------|------|-------------|
| `value` | `ptr` | Pointer value |

**Example:**

```qd
buf 0 mem::get_ptr -> p
```

---

### is_null

Check if pointer is null.

**Signature:** `( address:ptr -- is_null:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Pointer to check |

| Return | Type | Description |
|--------|------|-------------|
| `is_null` | `i64` | 1 if null, 0 otherwise |

**Example:**

```qd
buf mem::is_null .  // 0
```

---

### realloc

Reallocate memory to new size.

**Signature:** `( address:ptr new_bytes:i64 -- new_address:ptr )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Existing allocation (or null for new alloc) |
| `new_bytes` | `i64` | New size in bytes |

| Return | Type | Description |
|--------|------|-------------|
| `new_address` | `ptr` | Reallocated memory, or null on failure |

**Example:**

```qd
buf 2048 mem::realloc -> buf
```

---

### set

Set a 64-bit integer at offset.

**Signature:** `( address:ptr offset:i64 value:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |
| `value` | `i64` | Integer value |

**Example:**

```qd
buf 0 42 mem::set
```

---

### set_byte

Set a byte at offset.

**Signature:** `( address:ptr offset:i64 value:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |
| `value` | `i64` | Byte value (0-255) |

**Example:**

```qd
buf 0 65 mem::set_byte
```

---

### set_float

Set a 64-bit float at offset.

**Signature:** `( address:ptr offset:i64 value:f64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |
| `value` | `f64` | Float value |

**Example:**

```qd
buf 0 3.14 mem::set_float
```

---

### set_ptr

Set a pointer at offset.

**Signature:** `( address:ptr offset:i64 value:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |
| `value` | `ptr` | Pointer value |

**Example:**

```qd
buf 0 other_buf mem::set_ptr
```

---

### to_string

Convert buffer to string.

**Signature:** `( buffer:ptr length:i64 -- text:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buffer` | `ptr` | Source buffer |
| `length` | `i64` | Number of bytes |

| Return | Type | Description |
|--------|------|-------------|
| `text` | `str` | Null-terminated string |

**Example:**

```qd
buf len mem::to_string -> s
```

---

### zero

Zero out memory region.

**Signature:** `( address:ptr bytes:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `bytes` | `i64` | Number of bytes to zero |

**Example:**

```qd
buf 1024 mem::zero
```
