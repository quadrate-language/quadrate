# Arithmetic Operations

Mathematical operations on numbers.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `+` / `add` | `( a b -- sum )` | Addition |
| `-` / `sub` | `( a b -- diff )` | Subtraction |
| `*` / `mul` | `( a b -- product )` | Multiplication |
| `/` / `div` | `( a b -- quotient )` | Division |
| `%` / `mod` | `( a b -- remainder )` | Modulo |
| `neg` | `( a -- -a )` | Negation |
| `++` / `inc` | `( a -- a+1 )` | Increment |
| `--` / `dec` | `( a -- a-1 )` | Decrement |

---

## Basic Operations

### + (add)

Adds two numbers.

**Signature:** `( a b -- sum )`

```qd
3 4 + // 7
```

### - (sub)

Subtracts b from a.

**Signature:** `( a b -- diff )`

```qd
10 3 - // 7
```

### * (mul)

Multiplies two numbers.

**Signature:** `( a b -- product )`

```qd
6 7 * // 42
```

### / (div)

Divides a by b.

**Signature:** `( a b -- quotient )`

```qd
20 4 / // 5
```

### % (mod)

Computes a modulo b.

**Signature:** `( a b -- remainder )`

```qd
17 5 % // 2
```

---

## Unary Operations

### neg

Negates a number.

**Signature:** `( a -- -a )`

```qd
5 neg // -5
```

### ++ (inc)

Adds 1 to a number.

**Signature:** `( a -- a+1 )`

```qd
5 ++ // 6
5 inc // 6 (same as ++)
```

### -- (dec)

Subtracts 1 from a number.

**Signature:** `( a -- a-1 )`

```qd
5 -- // 4
5 dec // 4 (same as --)
```

---

## Word Forms

Both symbol and word forms are available:

| Symbol | Word |
|--------|------|
| `+` | `add` |
| `-` | `sub` |
| `*` | `mul` |
| `/` | `div` |
| `%` | `mod` |
| `++` | `inc` |
| `--` | `dec` |

```qd
3 4 add // Same as 3 4 +
10 3 sub // Same as 10 3 -
5 inc   // Same as 5 ++
5 dec   // Same as 5 --
```
