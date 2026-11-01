# Comparison operations

Operations that compare values and return a flag, 1 or 0.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `==` | `(a b -- flag)` | Equal |
| `!=` | `(a b -- flag)` | Not equal |
| `<` | `(a b -- flag)` | Less than |
| `<=` | `(a b -- flag)` | Less or equal |
| `>` | `(a b -- flag)` | Greater than |
| `>=` | `(a b -- flag)` | Greater or equal |
| `within` | `(a low high -- flag)` | Range check |

---

## Equality

### ==

Outputs 1 if a equals b, 0 otherwise.

**Signature:** `(a b -- flag)`

```qd
5 5 == // 1
5 3 == // 0
```

### !=

Outputs 1 if a does not equal b, 0 otherwise.

**Signature:** `(a b -- flag)`

```qd
5 3 != // 1
5 5 != // 0
```

---

## Ordering

### <

Outputs 1 if a is less than b, 0 otherwise.

**Signature:** `(a b -- flag)`

```qd
3 5 < // 1
5 3 < // 0
```

### <=

Outputs 1 if a is less than or equal to b, 0 otherwise.

**Signature:** `(a b -- flag)`

```qd
5 5 <= // 1
3 5 <= // 1
6 5 <= // 0
```

### >

Outputs 1 if a is greater than b, 0 otherwise.

**Signature:** `(a b -- flag)`

```qd
5 3 > // 1
3 5 > // 0
```

### >=

Outputs 1 if a is greater than or equal to b, 0 otherwise.

**Signature:** `(a b -- flag)`

```qd
5 5 >= // 1
5 3 >= // 1
3 5 >= // 0
```

---

## Range check

### within

Outputs 1 if val is in [low, high], 0 otherwise.

**Signature:** `(a low high -- flag)`

```qd
5 0 10 within  // 1 (5 is in [0,10])
10 0 10 within // 1 (10 is in [0,10])
11 0 10 within // 0 (11 is not in [0,10])
```
