# Stack Operations

Operations for manipulating values on the stack.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `dup` | `(a -- a a)` | Duplicate top |
| `dup2` | `(a b -- a b a b)` | Duplicate top two |
| `dupd` | `(a b -- a a b)` | Duplicate second |
| `drop` | `(a --)` | Remove top |
| `drop2` | `(a b --)` | Remove top two |
| `swap` | `(a b -- b a)` | Exchange top two |
| `swap2` | `(a b c d -- c d a b)` | Exchange top pairs |
| `swapd` | `(a b c -- b a c)` | Swap under top |
| `over` | `(a b -- a b a)` | Copy second to top |
| `over2` | `(a b c d -- a b c d a b)` | Copy second pair |
| `overd` | `(a b c -- a b a c)` | Copy second under top |
| `rot` | `(a b c -- b c a)` | Rotate three |
| `nip` | `(a b -- b)` | Remove second |
| `nipd` | `(a b c -- a c)` | Remove second under top |
| `tuck` | `(a b -- b a b)` | Copy top under second |
| `pick` | `(... n -- ... val)` | Copy nth value |
| `roll` | `(... n -- ...)` | Move nth to top |
| `clear` | `(... --)` | Remove all |
| `depth` | `(... -- ... n)` | Count values |

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

### dupd

Duplicates the second value, keeping top on top.

**Signature:** `(a b -- a a b)`

---

## Removal

### drop

Removes the top value from the stack.

**Signature:** `(a --)`

```qd
1 2 3 drop // Stack: [1, 2]
```

### drop2

Removes the top two values from the stack.

**Signature:** `(a b --)`

---

## Swapping

### swap

Exchanges the top two values.

**Signature:** `(a b -- b a)`

```qd
1 2 swap // Stack: [2, 1]
```

### swap2

Exchanges the top two pairs of values.

**Signature:** `(a b c d -- c d a b)`

### swapd

Swaps the second and third values, keeping top on top.

**Signature:** `(a b c -- b a c)`

---

## Copying

### over

Copies the second value to the top.

**Signature:** `(a b -- a b a)`

```qd
1 2 over // Stack: [1, 2, 1]
```

### over2

Copies the second pair to the top.

**Signature:** `(a b c d -- a b c d a b)`

### overd

Copies the second value, keeping top on top.

**Signature:** `(a b c -- a b a c)`

---

## Rotation

### rot

Rotates the top three values, moving third to top.

**Signature:** `(a b c -- b c a)`

```qd
1 2 3 rot // Stack: [2, 3, 1]
```

---

## Other Operations

### nip

Removes the second value, keeping top.

**Signature:** `(a b -- b)`

```qd
1 2 nip // Stack: [2]
```

### nipd

Removes the second value, keeping top and third.

**Signature:** `(a b c -- a c)`

### tuck

Copies the top value under the second.

**Signature:** `(a b -- b a b)`

### pick

Copies the nth value (0-indexed from top) to the top.

**Signature:** `(... n -- ... val)`

```qd
1 2 3 4 2 pick // Copies index 2 (value 2) to top
```

### roll

Moves the nth value to the top, shifting others down.

**Signature:** `(... n -- ...)`

```qd
1 2 3 4 2 roll // Moves index 2 (value 2) to top
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
