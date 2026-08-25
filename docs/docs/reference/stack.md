# Stack operations

Operations for manipulating values on the stack.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `dup` | `(a -- a a)` | Duplicate top |
| `dup2` | `(a b -- a b a b)` | Duplicate top two |
| `drop` | `(a --)` | Remove top |
| `swap` | `(a b -- b a)` | Exchange top two |
| `over` | `(a b -- a b a)` | Copy second to top |
| `rot` | `(a b c -- b c a)` | Rotate three |
| `nip` | `(a b -- b)` | Remove second |
| `pick` | `(... n -- ... val)` | Copy nth value |
| `roll` | `(... n -- ...)` | Move nth to top |
| `clear` | `(... --)` | Remove all |
| `depth` | `(... -- ... n)` | Count values |

**Naming convention:** the base name operates on the top element (`dup`, `swap`,
`drop`, `over`, `nip`); a `2` suffix operates on the top pair (`dup2`).

---

## Duplication

### dup

Duplicates the top value on the stack.

**Signature:** `(a -- a a)`

```qd
5 dup + // 10
```

### dup2

Duplicates the top two values.

**Signature:** `(a b -- a b a b)`

```qd
1 2 dup2 // Stack: [1, 2, 1, 2]
```

---

## Removal

### drop

Removes the top value from the stack.

**Signature:** `(a --)`

```qd
1 2 3 drop // Stack: [1, 2]
```

---

## Swapping

### swap

Exchanges the top two values.

**Signature:** `(a b -- b a)`

```qd
1 2 swap // Stack: [2, 1]
```

---

## Copying

### over

Copies the second value to the top.

**Signature:** `(a b -- a b a)`

```qd
1 2 over // Stack: [1, 2, 1]
```

---

## Rotation

### rot

Rotates the top three values, moving third to top.

**Signature:** `(a b c -- b c a)`

```qd
1 2 3 rot // Stack: [2, 3, 1]
```

---

## Other operations

### nip

Removes the second value, keeping top.

**Signature:** `(a b -- b)`

```qd
1 2 nip // Stack: [2]
```

### pick

Copies the nth value (0-indexed from top) to the top.

**Signature:** `(... n -- ... val)`

```qd
1 2 3 4 2 pick // Copies index 2 (value 2) to top
```

### roll

Moves the nth value (0-indexed from top) to the top, shifting others down.

**Signature:** `(... n -- ...)`

```qd
1 2 3 4 2 roll // Moves index 2 (value 2) to top -> [1, 3, 4, 2]
```

### clear

Removes all values from the stack.

**Signature:** `(... --)`

### depth

Pushes the number of values on the stack.

**Signature:** `(... -- ... n)`

```qd
1 2 3 depth // Stack: [1, 2, 3, 3]
```
