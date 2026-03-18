# `use` bits

## Functions

### `fn` clear_bit

Clear a bit to 0.

**Signature:** `(value:i64 bit_pos:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |
| `bit_pos` | `i64` | Bit position to clear |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Value with bit cleared |

**Example:**

```qd
0b1010 1 bits::clear_bit print  // 8 (0b1000)
```
---

### `fn` clz

Count leading zeros (from MSB).

**Signature:** `(value:i64 -- count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to count |

| Output | Type | Description |
|--------|------|-------------|
| `count` | `i64` | Number of leading zero bits (0-64) |

**Example:**

```qd
0x0F00000000000000 bits::clz print  // 4
```
---

### `fn` ctz

Count trailing zeros (from LSB).

**Signature:** `(value:i64 -- count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to count |

| Output | Type | Description |
|--------|------|-------------|
| `count` | `i64` | Number of trailing zero bits (0-64) |

**Example:**

```qd
0x100 bits::ctz print  // 8
```
---

### `fn` extract

Extract a bit field from a value.

**Signature:** `(value:i64 start_bit:i64 num_bits:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Source value |
| `start_bit` | `i64` | Start position (0 = LSB) |
| `num_bits` | `i64` | Number of bits to extract |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Extracted bits, right-aligned |

**Example:**

```qd
0b11010110 2 3 bits::extract print  // 5
```
---

### `fn` has_bit

Check if a bit is set.

**Signature:** `(value:i64 bit_pos:i64 -- flag:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to check |
| `bit_pos` | `i64` | Bit position (0 = LSB) |

| Output | Type | Description |
|--------|------|-------------|
| `flag` | `i64` | 1 if set, 0 if not |

**Example:**

```qd
0b1010 1 bits::has_bit print  // 1
```
---

### `fn` highest_bit

Get the highest set bit as a value (isolate highest bit).

**Signature:** `(value:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Highest set bit as power of two, 0 if input is 0 |

**Example:**

```qd
0b1010 bits::highest_bit print  // 8 (0b1000)
```
---

### `fn` is_power_of_two

Check if value is a power of two.

**Signature:** `(value:i64 -- flag:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to check |

| Output | Type | Description |
|--------|------|-------------|
| `flag` | `i64` | 1 if power of two, 0 otherwise |

**Example:**

```qd
8 bits::is_power_of_two print  // 1
7 bits::is_power_of_two print  // 0
```
---

### `fn` log2

Integer log base 2 (position of highest set bit).

**Signature:** `(value:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value (must be > 0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Floor of log2(value) |

**Example:**

```qd
8 bits::log2 print  // 3
15 bits::log2 print  // 3
```
---

### `fn` lowest_bit

Get the lowest set bit as a value (isolate lowest bit).

**Signature:** `(value:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Lowest set bit as power of two, 0 if input is 0 |

**Example:**

```qd
0b1010 bits::lowest_bit print  // 2 (0b0010)
```
---

### `fn` mask

Bitwise operations for integer manipulation.  Core operations (and, or, xor, not, shl, shr) are builtins. This module provides higher-level bit manipulation functions. Keep only the bottom N bits.

**Signature:** `(value:i64 num_bits:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |
| `num_bits` | `i64` | Number of bits to keep from LSB |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Masked value |

**Example:**

```qd
0xFF 4 bits::mask print  // 15
```
---

### `fn` next_power_of_two

Find the next power of two greater than or equal to value.

**Signature:** `(value:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Next power of two |

**Example:**

```qd
5 bits::next_power_of_two print  // 8
8 bits::next_power_of_two print  // 8
```
---

### `fn` popcount

Count set bits (population count).

**Signature:** `(value:i64 -- count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to count |

| Output | Type | Description |
|--------|------|-------------|
| `count` | `i64` | Number of 1 bits |

**Example:**

```qd
0b1010110 bits::popcount print  // 4
```
---

### `fn` reverse_bits

Reverse the bottom N bits.

**Signature:** `(value:i64 width:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to reverse |
| `width` | `i64` | Number of bits to reverse |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Reversed value |

**Example:**

```qd
0b1011 4 bits::reverse_bits print  // 13 (0b1101)
```
---

### `fn` rotate_left

Rotate bits left within a width.

**Signature:** `(value:i64 bits:i64 width:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to rotate |
| `bits` | `i64` | Positions to rotate |
| `width` | `i64` | Bit width to rotate within |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Rotated value |

**Example:**

```qd
0b0011 1 4 bits::rotate_left print  // 6 (0b0110)
```
---

### `fn` rotate_right

Rotate bits right within a width.

**Signature:** `(value:i64 bits:i64 width:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to rotate |
| `bits` | `i64` | Positions to rotate |
| `width` | `i64` | Bit width to rotate within |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Rotated value |

**Example:**

```qd
0b0110 1 4 bits::rotate_right print  // 3 (0b0011)
```
---

### `fn` set_bits

Set a bit field in a value.

**Signature:** `(target:i64 value:i64 start_bit:i64 num_bits:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `target` | `i64` | Target value to modify |
| `value` | `i64` | Value to insert |
| `start_bit` | `i64` | Start position (0 = LSB) |
| `num_bits` | `i64` | Number of bits to set |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Modified target |

**Example:**

```qd
0 0b101 2 3 bits::set_bits print  // 20 (0b10100)
```
---

### `fn` set_bit

Set a bit to 1.

**Signature:** `(value:i64 bit_pos:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |
| `bit_pos` | `i64` | Bit position to set |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Value with bit set |

**Example:**

```qd
0b1000 1 bits::set_bit print  // 10 (0b1010)
```
---

### `fn` toggle_bit

Toggle a bit.

**Signature:** `(value:i64 bit_pos:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |
| `bit_pos` | `i64` | Bit position to toggle |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Value with bit toggled |

**Example:**

```qd
0b1010 0 bits::toggle_bit print  // 11 (0b1011)
```
