# Bitwise Operations

Operations for bit manipulation.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `and` | `(a b -- result)` | Bitwise AND |
| `or` | `(a b -- result)` | Bitwise OR |
| `xor` | `(a b -- result)` | Bitwise XOR |
| `not` | `(a -- result)` | Bitwise NOT |
| `<<` / `shl` | `(a n -- result)` | Shift left |
| `>>` / `shr` | `(a n -- result)` | Shift right |

---

## Logical Operations

### and

Computes bitwise AND of two integers.

**Signature:** `(a b -- result)`

```qd
0b1100 0b1010 and // 0b1000 (8)
```

### or

Computes bitwise OR of two integers.

**Signature:** `(a b -- result)`

```qd
0b1100 0b1010 or // 0b1110 (14)
```

### xor

Computes bitwise XOR of two integers.

**Signature:** `(a b -- result)`

```qd
0b1100 0b1010 xor // 0b0110 (6)
```

### not

Computes bitwise NOT (ones' complement).

**Signature:** `(a -- result)`

```qd
0 not // -1 (all bits set)
```

---

## Shift Operations

### << (shl)

Shifts a left by n bits.

**Signature:** `(a n -- result)`

```qd
1 4 <<  // 16
1 4 shl // 16 (same as <<)
```

### >> (shr)

Shifts a right by n bits (arithmetic shift).

**Signature:** `(a n -- result)`

```qd
16 2 >>  // 4
16 2 shr // 4 (same as >>)
```

---

## Common Uses

### Setting Bits

```qd
const FlagRead = 1
const FlagWrite = 2
const FlagExec = 4

// Set flags
FlagRead FlagWrite or -> permissions
```

### Checking Bits

```qd
permissions FlagRead and 0 != if {
	"Has read permission" print nl
}
```

### Clearing Bits

```qd
permissions FlagWrite not and -> permissions
```

### Toggling Bits

```qd
permissions FlagExec xor -> permissions
```
