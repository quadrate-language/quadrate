# Bitwise Operations

Operations for bit manipulation.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `and` | `( a b -- result )` | Bitwise AND |
| `or` | `( a b -- result )` | Bitwise OR |
| `xor` | `( a b -- result )` | Bitwise XOR |
| `not` | `( a -- result )` | Bitwise NOT |
| `shl` | `( a n -- result )` | Shift left |
| `shr` | `( a n -- result )` | Shift right |

---

## Logical Operations

### and

Computes bitwise AND of two integers.

**Signature:** `( a b -- result )`

```qd
0b1100 0b1010 and // 0b1000 (8)
```

### or

Computes bitwise OR of two integers.

**Signature:** `( a b -- result )`

```qd
0b1100 0b1010 or // 0b1110 (14)
```

### xor

Computes bitwise XOR of two integers.

**Signature:** `( a b -- result )`

```qd
0b1100 0b1010 xor // 0b0110 (6)
```

### not

Computes bitwise NOT (ones' complement).

**Signature:** `( a -- result )`

```qd
0 not // -1 (all bits set)
```

---

## Shift Operations

### shl

Shifts a left by n bits.

**Signature:** `( a n -- result )`

```qd
1 4 shl // 16
```

### shr

Shifts a right by n bits (arithmetic shift).

**Signature:** `( a n -- result )`

```qd
16 2 shr // 4
```

---

## Common Uses

### Setting Bits

```qd
const FLAG_READ 1
const FLAG_WRITE 2
const FLAG_EXEC 4

// Set flags
FLAG_READ FLAG_WRITE or -> permissions
```

### Checking Bits

```qd
permissions FLAG_READ and 0 != if {
	"Has read permission" print nl
}
```

### Clearing Bits

```qd
permissions FLAG_WRITE not and -> permissions
```

### Toggling Bits

```qd
permissions FLAG_EXEC xor -> permissions
```
