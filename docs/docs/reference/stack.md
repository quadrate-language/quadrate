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
| `pick` | `(x y z -- x y z x)` | Copy the third value to the top |
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

Copies the third value from the top to the top.

**Signature:** `(x y z -- x y z x)`

```qd
1 2 3 pick // Stack: [1, 2, 3, 1]
```

It used to take the depth as a value popped at run time — `1 2 3 4 2 pick`. Nothing could say
what a body containing one leaves on the stack, so it was rejected outright in functions the
compiler evaluates on a compile-time stack. A fixed depth is checkable everywhere.

`roll`, which moved the nth value to the top, is gone for the same reason and has no replacement
word: name the values with `->` and push them back in the order you want.

### clear

Removes all values from the stack.

**Signature:** `(... --)`

### depth

Pushes the number of values on the stack.

**Signature:** `(... -- ... n)`

```qd
1 2 3 depth // Stack: [1, 2, 3, 3]
```
