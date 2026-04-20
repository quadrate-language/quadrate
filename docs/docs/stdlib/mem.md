# `use` mem

Low-level memory allocation and manipulation.

SAFETY: These are unsafe operations with no bounds checking.
Improper use can cause segmentation faults and memory corruption.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrAlloc` | `2` | Allocation failure error code |
| `ErrInvalidArg` | `3` | Invalid argument error code |

## Functions

### `fn` alloc_aligned

Allocate aligned memory. The returned pointer is aligned to the specified boundary. Use standard mem::free to deallocate.

**Signature:** `(alignment:i64 bytes:i64 -- address:ptr)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `alignment` | `i64` | Alignment boundary (must be power of 2, e.g., 16, 32, 64) |
| `bytes` | `i64` | Number of bytes to allocate |

| Output | Type | Description |
|--------|------|-------------|
| `address` | `ptr` | Aligned memory address |

| Error | Description |
|-------|-------------|
| `mem::ErrAlloc` | Allocation failed |
| `mem::ErrInvalidArg` | Invalid alignment or size |

**Example:**

```qd
64 1024 mem::alloc_aligned!  // buf  // 64-byte aligned
```
---

### `fn` alloc

Allocate memory.

**Signature:** `(bytes:i64 -- address:ptr)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `bytes` | `i64` | Number of bytes to allocate |

| Output | Type | Description |
|--------|------|-------------|
| `address` | `ptr` | Allocated memory address |

| Error | Description |
|-------|-------------|
| `mem::ErrAlloc` | Allocation failed |
| `mem::ErrInvalidArg` | Invalid size (negative) |

**Example:**

```qd
1024 mem::alloc!  // buf
```
---

### `fn` copy

Copy bytes between memory regions.

**Signature:** `(dst:ptr src:ptr bytes:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `dst` | `ptr` | Destination address |
| `src` | `ptr` | Source address |
| `bytes` | `i64` | Number of bytes to copy |

**Example:**

```qd
dst src 100 mem::copy
```
---

### `fn` fill

Fill memory region with byte value.

**Signature:** `(value:i64 address:ptr bytes:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Fill value (0-255) |
| `address` | `ptr` | Base address |
| `bytes` | `i64` | Number of bytes to fill |

**Example:**

```qd
0xFF buf 1024 mem::fill
```
---

### `fn` free

Free allocated memory.

**Signature:** `(address:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Memory to free (null is safe) |

**Example:**

```qd
buf mem::free
```
---

### `fn` from_string

Convert string to buffer.

**Signature:** `(text:str -- buffer:ptr length:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `text` | `str` | Source string |

| Output | Type | Description |
|--------|------|-------------|
| `buffer` | `ptr` | Allocated buffer |
| `length` | `i64` | String length |

**Example:**

```qd
"hello" mem::from_string -> len  // buf
```
---

### `fn` get_byte

Get a byte at offset.

**Signature:** `(address:ptr offset:i64 -- value:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Byte value (0-255) |

**Example:**

```qd
buf 0 mem::get_byte  // b
```
---

### `fn` get_f64

Get a 64-bit float at offset.

**Signature:** `(address:ptr offset:i64 -- value:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `f64` | Float value |

**Example:**

```qd
buf 0 mem::get_f64  // x
```
---

### `fn` get_i16

Get a signed 16-bit integer at offset (sign-extended to i64).

**Signature:** `(address:ptr offset:i64 -- value:i64)`
---

### `fn` get_i32

Get a signed 32-bit integer at offset (sign-extended to i64).

**Signature:** `(address:ptr offset:i64 -- value:i64)`
---

### `fn` get_i64

Get a 64-bit integer at offset.

**Signature:** `(address:ptr offset:i64 -- value:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Integer value |

**Example:**

```qd
buf 0 mem::get_i64  // n
```
---

### `fn` get_i8

Get a signed 8-bit integer at offset (sign-extended to i64).

**Signature:** `(address:ptr offset:i64 -- value:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Sign-extended value (-128..127) |
---

### `fn` get_ptr

Get a pointer at offset.

**Signature:** `(address:ptr offset:i64 -- value:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `ptr` | Pointer value |

**Example:**

```qd
buf 0 mem::get_ptr  // p
```
---

### `fn` get_u16

Get an unsigned 16-bit integer at offset (zero-extended to i64).

**Signature:** `(address:ptr offset:i64 -- value:i64)`
---

### `fn` get_u32

Get an unsigned 32-bit integer at offset (zero-extended to i64).

**Signature:** `(address:ptr offset:i64 -- value:i64)`
---

### `fn` get_u8

Get an unsigned 8-bit integer at offset (zero-extended to i64).

**Signature:** `(address:ptr offset:i64 -- value:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Zero-extended value (0..255) |
---

### `fn` is_not_null

Check if pointer is not null.

**Signature:** `(address:ptr -- not_null:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Pointer to check |

| Output | Type | Description |
|--------|------|-------------|
| `not_null` | `i64` | 1 if not null, 0 otherwise |

**Example:**

```qd
buf mem::is_not_null print  // 1
```
---

### `fn` is_null

Check if pointer is null.

**Signature:** `(address:ptr -- is_null:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Pointer to check |

| Output | Type | Description |
|--------|------|-------------|
| `is_null` | `i64` | 1 if null, 0 otherwise |

**Example:**

```qd
buf mem::is_null print  // 0
```
---

### `fn` ptr_add

Pointer arithmetic: base + offset.

**Signature:** `(base:ptr offset:i64 -- result:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `base` | `ptr` | Base pointer |
| `offset` | `i64` | Byte offset to add |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `ptr` | Offset pointer |
---

### `fn` realloc

Reallocate memory to new size.

**Signature:** `(address:ptr new_bytes:i64 -- new_address:ptr)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Existing allocation (or null for new alloc) |
| `new_bytes` | `i64` | New size in bytes |

| Output | Type | Description |
|--------|------|-------------|
| `new_address` | `ptr` | Reallocated memory address |

| Error | Description |
|-------|-------------|
| `mem::ErrAlloc` | Reallocation failed |
| `mem::ErrInvalidArg` | Invalid size (negative) |

**Example:**

```qd
buf 2048 mem::realloc!  // buf
```
---

### `fn` set_byte

Set a byte at offset.

**Signature:** `(value:i64 address:ptr offset:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Byte value (0-255) |
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

**Example:**

```qd
65 buf 0 mem::set_byte
```
---

### `fn` set_f64

Set a 64-bit float at offset.

**Signature:** `(value:f64 address:ptr offset:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `f64` | Float value |
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

**Example:**

```qd
3.14 buf 0 mem::set_f64
```
---

### `fn` set_i16

Set a signed 16-bit integer at offset. Stored as low 2 bytes of `value`.

**Signature:** `(value:i64 address:ptr offset:i64 -- )`
---

### `fn` set_i32

Set a signed 32-bit integer at offset. Stored as low 4 bytes of `value`.

**Signature:** `(value:i64 address:ptr offset:i64 -- )`
---

### `fn` set_i64

Set a 64-bit integer at offset.

**Signature:** `(value:i64 address:ptr offset:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Integer value |
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

**Example:**

```qd
42 buf 0 mem::set_i64
```
---

### `fn` set_i8

Set a signed 8-bit integer at offset. Stored value is the low byte of `value`.

**Signature:** `(value:i64 address:ptr offset:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Integer value (low 8 bits used) |
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |
---

### `fn` set_ptr

Set a pointer at offset.

**Signature:** `(value:ptr address:ptr offset:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `ptr` | Pointer value |
| `address` | `ptr` | Base address |
| `offset` | `i64` | Byte offset from base |

**Example:**

```qd
other_buf buf 0 mem::set_ptr
```
---

### `fn` set_u16

Set an unsigned 16-bit integer at offset. Stored as low 2 bytes of `value`.

**Signature:** `(value:i64 address:ptr offset:i64 -- )`
---

### `fn` set_u32

Set an unsigned 32-bit integer at offset. Stored as low 4 bytes of `value`.

**Signature:** `(value:i64 address:ptr offset:i64 -- )`
---

### `fn` set_u8

Set an unsigned 8-bit integer at offset. Stored value is the low byte of `value`.

**Signature:** `(value:i64 address:ptr offset:i64 -- )`
---

### `fn` to_string

Convert buffer to string.

**Signature:** `(buffer:ptr length:i64 -- text:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buffer` | `ptr` | Source buffer |
| `length` | `i64` | Number of bytes |

| Output | Type | Description |
|--------|------|-------------|
| `text` | `str` | Null-terminated string |

**Example:**

```qd
buf len mem::to_string  // s
```
---

### `fn` zero

Zero out memory region.

**Signature:** `(address:ptr bytes:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | `ptr` | Base address |
| `bytes` | `i64` | Number of bytes to zero |

**Example:**

```qd
buf 1024 mem::zero
```
