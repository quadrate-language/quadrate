# Arithmetic operations

Mathematical operations on numbers.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `+` | `(a b -- sum)` | Addition |
| `-` | `(a b -- diff)` | Subtraction |
| `*` | `(a b -- product)` | Multiplication |
| `/` | `(a b -- quotient)` | Division |
| `%` | `(a b -- remainder)` | Modulo |
| `neg` | `(a -- -a)` | Negation |
| `++` | `(a -- a+1)` | Increment |
| `--` | `(a -- a-1)` | Decrement |

---

## Basic operations

### +

Adds two numbers.

**Signature:** `(a b -- sum)`

```qd
3 4 + // 7
```

### -

Subtracts b from a.

**Signature:** `(a b -- diff)`

```qd
10 3 - // 7
```

### *

Multiplies two numbers.

**Signature:** `(a b -- product)`

```qd
6 7 * // 42
```

### /

Divides a by b.

**Signature:** `(a b -- quotient)`

```qd
20 4 / // 5
```

### %

Computes a modulo b. Integers only.

**Signature:** `(a b -- remainder)`

```qd
17 5 % // 2
```

---

## Unary operations

### neg

Negates a number.

**Signature:** `(a -- -a)`

```qd
5 neg // -5
```

### ++

Adds 1 to a number.

**Signature:** `(a -- a+1)`

```qd
5 ++ // 6
```

### --

Subtracts 1 from a number.

**Signature:** `(a -- a-1)`

```qd
5 -- // 4
```
