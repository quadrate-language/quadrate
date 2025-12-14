# `use` bytes

Byte array operations and endianness conversion.
Provides functions for reading and writing multi-byte integers
in big-endian and little-endian byte order.

## Functions

### `fn` compare

Compare two byte buffers.

**Signature:** `( a:ptr a_off:i64 b:ptr b_off:i64 count:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `ptr` | First buffer |
| `a_off` | `i64` | First buffer offset |
| `b` | `ptr` | Second buffer |
| `b_off` | `i64` | Second buffer offset |
| `count` | `i64` | Number of bytes to compare |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 0 if equal, negative if a<b, positive if a>b |

**Example:**

```qd
buf1 0 buf2 0 10 bytes::compare  // result
```

---

### `fn` copy

Copy bytes from one buffer to another.

**Signature:** `( dst:ptr dst_off:i64 src:ptr src_off:i64 count:i64 --  )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `dst` | `ptr` | Destination buffer |
| `dst_off` | `i64` | Destination offset |
| `src` | `ptr` | Source buffer |
| `src_off` | `i64` | Source offset |
| `count` | `i64` | Number of bytes to copy |

**Example:**

```qd
dst 0 src 0 10 bytes::copy
```

---

### `fn` fill

Fill a buffer with a byte value.

**Signature:** `( buf:ptr offset:i64 count:i64 value:i64 --  )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to fill |
| `offset` | `i64` | Starting offset |
| `count` | `i64` | Number of bytes to fill |
| `value` | `i64` | Byte value to fill with |

**Example:**

```qd
buf 0 10 0 bytes::fill
```

---

### `fn` read_u16_be

Read a 16-bit unsigned integer from memory in big-endian order.

**Signature:** `( buf:ptr offset:i64 -- value:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to read from |
| `offset` | `i64` | Byte offset in buffer |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | 16-bit value |

**Example:**

```qd
buf 0 bytes::read_u16_be  // val
```

---

### `fn` read_u16_le

Read a 16-bit unsigned integer from memory in little-endian order.

**Signature:** `( buf:ptr offset:i64 -- value:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to read from |
| `offset` | `i64` | Byte offset in buffer |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | 16-bit value |

**Example:**

```qd
buf 0 bytes::read_u16_le  // val
```

---

### `fn` read_u32_be

Read a 32-bit unsigned integer from memory in big-endian order.

**Signature:** `( buf:ptr offset:i64 -- value:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to read from |
| `offset` | `i64` | Byte offset in buffer |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | 32-bit value |

**Example:**

```qd
buf 0 bytes::read_u32_be  // val
```

---

### `fn` read_u32_le

Read a 32-bit unsigned integer from memory in little-endian order.

**Signature:** `( buf:ptr offset:i64 -- value:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to read from |
| `offset` | `i64` | Byte offset in buffer |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | 32-bit value |

**Example:**

```qd
buf 0 bytes::read_u32_le  // val
```

---

### `fn` read_u64_be

Read a 64-bit unsigned integer from memory in big-endian order.

**Signature:** `( buf:ptr offset:i64 -- value:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to read from |
| `offset` | `i64` | Byte offset in buffer |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | 64-bit value |

**Example:**

```qd
buf 0 bytes::read_u64_be  // val
```

---

### `fn` read_u64_le

Read a 64-bit unsigned integer from memory in little-endian order.

**Signature:** `( buf:ptr offset:i64 -- value:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to read from |
| `offset` | `i64` | Byte offset in buffer |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | 64-bit value |

**Example:**

```qd
buf 0 bytes::read_u64_le  // val
```

---

### `fn` swap16

Swap byte order of a 16-bit value.

**Signature:** `( value:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | 16-bit value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Byte-swapped value |

**Example:**

```qd
0x1234 bytes::swap16  // result  // 0x3412
```

---

### `fn` swap32

Swap byte order of a 32-bit value.

**Signature:** `( value:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | 32-bit value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Byte-swapped value |

**Example:**

```qd
0x12345678 bytes::swap32  // result  // 0x78563412
```

---

### `fn` swap64

Swap byte order of a 64-bit value.

**Signature:** `( value:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | 64-bit value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Byte-swapped value |

**Example:**

```qd
0x0102030405060708 bytes::swap64  // result  // 0x0807060504030201
```

---

### `fn` write_u16_be

Write a 16-bit unsigned integer to memory in big-endian order.

**Signature:** `( buf:ptr offset:i64 value:i64 --  )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to write to |
| `offset` | `i64` | Byte offset in buffer |
| `value` | `i64` | 16-bit value to write |

**Example:**

```qd
buf 0 0x1234 bytes::write_u16_be
```

---

### `fn` write_u16_le

Write a 16-bit unsigned integer to memory in little-endian order.

**Signature:** `( buf:ptr offset:i64 value:i64 --  )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to write to |
| `offset` | `i64` | Byte offset in buffer |
| `value` | `i64` | 16-bit value to write |

**Example:**

```qd
buf 0 0x1234 bytes::write_u16_le
```

---

### `fn` write_u32_be

Write a 32-bit unsigned integer to memory in big-endian order.

**Signature:** `( buf:ptr offset:i64 value:i64 --  )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to write to |
| `offset` | `i64` | Byte offset in buffer |
| `value` | `i64` | 32-bit value to write |

**Example:**

```qd
buf 0 0x12345678 bytes::write_u32_be
```

---

### `fn` write_u32_le

Write a 32-bit unsigned integer to memory in little-endian order.

**Signature:** `( buf:ptr offset:i64 value:i64 --  )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to write to |
| `offset` | `i64` | Byte offset in buffer |
| `value` | `i64` | 32-bit value to write |

**Example:**

```qd
buf 0 0x12345678 bytes::write_u32_le
```

---

### `fn` write_u64_be

Write a 64-bit unsigned integer to memory in big-endian order.

**Signature:** `( buf:ptr offset:i64 value:i64 --  )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to write to |
| `offset` | `i64` | Byte offset in buffer |
| `value` | `i64` | 64-bit value to write |

**Example:**

```qd
buf 0 0x123456789ABCDEF0 bytes::write_u64_be
```

---

### `fn` write_u64_le

Write a 64-bit unsigned integer to memory in little-endian order.

**Signature:** `( buf:ptr offset:i64 value:i64 --  )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `ptr` | Buffer to write to |
| `offset` | `i64` | Byte offset in buffer |
| `value` | `i64` | 64-bit value to write |

**Example:**

```qd
buf 0 0x123456789ABCDEF0 bytes::write_u64_le
```

