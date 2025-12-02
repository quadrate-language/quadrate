# bits

Bitwise operations for integer manipulation.

## Functions

### and

Bitwise AND.

**Signature:** `( a:i64 b:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `i64` | First operand |
| `b` | `i64` | Second operand |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | a AND b |

**Example:**

```qd
0b1100 0b1010 bits::and .  // 8
```

---

### clear_bit

Clear a bit to 0.

**Signature:** `( value:i64 bit_pos:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |
| `bit_pos` | `i64` | Bit position to clear |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Value with bit cleared |

**Example:**

```qd
0b1010 1 bits::clear_bit .  // 8 (0b1000)
```

---

### extract

Extract a bit field from a value.

**Signature:** `( value:i64 start_bit:i64 num_bits:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Source value |
| `start_bit` | `i64` | Start position (0 = LSB) |
| `num_bits` | `i64` | Number of bits to extract |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Extracted bits, right-aligned |

**Example:**

```qd
0b11010110 2 3 bits::extract .  // 5
```

---

### has_bit

Check if a bit is set.

**Signature:** `( value:i64 bit_pos:i64 -- flag:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to check |
| `bit_pos` | `i64` | Bit position (0 = LSB) |

| Return | Type | Description |
|--------|------|-------------|
| `flag` | `i64` | 1 if set, 0 if not |

**Example:**

```qd
0b1010 1 bits::has_bit .  // 1
```

---

### lshift

Left shift.

**Signature:** `( value:i64 shift:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to shift |
| `shift` | `i64` | Number of positions |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | value << shift |

**Example:**

```qd
1 4 bits::lshift .  // 16
```

---

### mask

Keep only the bottom N bits.

**Signature:** `( value:i64 num_bits:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |
| `num_bits` | `i64` | Number of bits to keep from LSB |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Masked value |

**Example:**

```qd
0xFF 4 bits::mask .  // 15
```

---

### not

Bitwise NOT (complement).

**Signature:** `( a:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `i64` | Value to complement |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | All bits flipped |

**Example:**

```qd
0 bits::not .  // -1
```

---

### or

Bitwise OR.

**Signature:** `( a:i64 b:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `i64` | First operand |
| `b` | `i64` | Second operand |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | a OR b |

**Example:**

```qd
0b1100 0b1010 bits::or .  // 14
```

---

### popcount

Count set bits (population count).

**Signature:** `( value:i64 -- count:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to count |

| Return | Type | Description |
|--------|------|-------------|
| `count` | `i64` | Number of 1 bits |

**Example:**

```qd
0b1010110 bits::popcount .  // 4
```

---

### reverse_bits

Reverse the bottom N bits.

**Signature:** `( value:i64 width:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to reverse |
| `width` | `i64` | Number of bits to reverse |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Reversed value |

**Example:**

```qd
0b1011 4 bits::reverse_bits .  // 13 (0b1101)
```

---

### rotate_left

Rotate bits left within a width.

**Signature:** `( value:i64 bits:i64 width:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to rotate |
| `bits` | `i64` | Positions to rotate |
| `width` | `i64` | Bit width to rotate within |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Rotated value |

**Example:**

```qd
0b0011 1 4 bits::rotate_left .  // 6 (0b0110)
```

---

### rotate_right

Rotate bits right within a width.

**Signature:** `( value:i64 bits:i64 width:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to rotate |
| `bits` | `i64` | Positions to rotate |
| `width` | `i64` | Bit width to rotate within |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Rotated value |

**Example:**

```qd
0b0110 1 4 bits::rotate_right .  // 3 (0b0011)
```

---

### rshift

Right shift.

**Signature:** `( value:i64 shift:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to shift |
| `shift` | `i64` | Number of positions |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | value >> shift |

**Example:**

```qd
16 2 bits::rshift .  // 4
```

---

### set_bit

Set a bit to 1.

**Signature:** `( value:i64 bit_pos:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |
| `bit_pos` | `i64` | Bit position to set |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Value with bit set |

**Example:**

```qd
0b1000 1 bits::set_bit .  // 10 (0b1010)
```

---

### set_bits

Set a bit field in a value.

**Signature:** `( target:i64 value:i64 start_bit:i64 num_bits:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `target` | `i64` | Target value to modify |
| `value` | `i64` | Value to insert |
| `start_bit` | `i64` | Start position (0 = LSB) |
| `num_bits` | `i64` | Number of bits to set |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Modified target |

**Example:**

```qd
0 0b101 2 3 bits::set_bits .  // 20 (0b10100)
```

---

### toggle_bit

Toggle a bit.

**Signature:** `( value:i64 bit_pos:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Input value |
| `bit_pos` | `i64` | Bit position to toggle |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Value with bit toggled |

**Example:**

```qd
0b1010 0 bits::toggle_bit .  // 11 (0b1011)
```

---

### xor

Bitwise XOR.

**Signature:** `( a:i64 b:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `i64` | First operand |
| `b` | `i64` | Second operand |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | a XOR b |

**Example:**

```qd
0b1100 0b1010 bits::xor .  // 6
```
