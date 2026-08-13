# Bitwise operations

Operations for bit manipulation.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `and` | `(a b -- result)` | Bitwise AND |
| `or` | `(a b -- result)` | Bitwise OR |
| `xor` | `(a b -- result)` | Bitwise XOR |
| `not` | `(a -- result)` | Bitwise NOT (ones' complement) |
| `lnot` | `(a -- result)` | Logical NOT (boolean negation) |
| `shl` | `(a n -- result)` | Shift left |
| `shr` | `(a n -- result)` | Shift right |

!!! warning "`not` is bitwise -- use `lnot` for boolean negation"
    `not` computes the ones' complement, so `0 not` is `-1`, not `1`. Writing
    `flag not other and` to mean "NOT flag AND other" silently gives the wrong answer,
    because `-1` is all bits set and `-1 and X` is `X`.

    Use `lnot` when you want boolean negation:

    ```qd
    0 not print nl      // -1   (all bits set)
    0 lnot print nl     // 1
    5 lnot print nl     // 0
    ```

---

## Logical operations

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

## Shift operations

### shl

Shifts a left by n bits.

**Signature:** `(a n -- result)`

```qd
1 4 shl // 16
```

### shr

Shifts a right by n bits (arithmetic shift).

**Signature:** `(a n -- result)`

```qd
16 2 shr // 4
```

---

## Common uses

### Setting bits

```qd
const FlagRead = 1
const FlagWrite = 2
const FlagExec = 4

// Set flags
FlagRead FlagWrite or -> permissions
```

### Checking bits

```qd
permissions FlagRead and 0 != if {
	"Has read permission" print nl
}
```

### Clearing bits

```qd
permissions FlagWrite not and -> permissions
```

### Toggling bits

```qd
permissions FlagExec xor -> permissions
```
