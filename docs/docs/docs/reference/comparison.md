# Comparison Operations

Operations that compare values and return boolean results (0 or 1).

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `==` / `eq` | `(a b -- bool)` | Equal |
| `!=` / `neq` | `(a b -- bool)` | Not equal |
| `<` / `lt` | `(a b -- bool)` | Less than |
| `<=` / `lte` | `(a b -- bool)` | Less or equal |
| `>` / `gt` | `(a b -- bool)` | Greater than |
| `>=` / `gte` | `(a b -- bool)` | Greater or equal |
| `within` | `(val low high -- bool)` | Range check |

---

## Equality

### == (eq)

Outputs 1 if a equals b, 0 otherwise.

**Signature:** `(a b -- bool)`

```qd
5 5 == // 1
5 3 == // 0
```

### != (neq)

Outputs 1 if a does not equal b, 0 otherwise.

**Signature:** `(a b -- bool)`

```qd
5 3 != // 1
5 5 != // 0
```

---

## Ordering

### < (lt)

Outputs 1 if a is less than b, 0 otherwise.

**Signature:** `(a b -- bool)`

```qd
3 5 < // 1
5 3 < // 0
```

### <= (lte)

Outputs 1 if a is less than or equal to b, 0 otherwise.

**Signature:** `(a b -- bool)`

```qd
5 5 <= // 1
3 5 <= // 1
6 5 <= // 0
```

### > (gt)

Outputs 1 if a is greater than b, 0 otherwise.

**Signature:** `(a b -- bool)`

```qd
5 3 > // 1
3 5 > // 0
```

### >= (gte)

Outputs 1 if a is greater than or equal to b, 0 otherwise.

**Signature:** `(a b -- bool)`

```qd
5 5 >= // 1
5 3 >= // 1
3 5 >= // 0
```

---

## Range Check

### within

Outputs 1 if val is in [low, high), 0 otherwise.

**Signature:** `(val low high -- bool)`

```qd
5 0 10 within // 1 (5 is in [0,10))
10 0 10 within // 0 (10 is not in [0,10))
```

---

## Word Forms

Both symbol and word forms are available:

| Symbol | Word |
|--------|------|
| `==` | `eq` |
| `!=` | `neq` |
| `<` | `lt` |
| `<=` | `lte` |
| `>` | `gt` |
| `>=` | `gte` |

```qd
5 3 gt // Same as 5 3 >
5 5 eq // Same as 5 5 ==
```
