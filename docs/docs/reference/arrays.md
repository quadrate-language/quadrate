# Array operations

Operations for creating and manipulating arrays.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `make<T>` | `(size -- arr)` | Create array with elements of type T |
| `len` | `(arr -- len)` | Get length |
| `nth` | `(arr index -- value)` | Get element |
| `set` | `(arr index value --)` | Set element |
| `append` | `(arr value -- arr)` | Append element |
| `free` | `(arr --)` | Free memory |

---

## Creating arrays

### make<T>

Creates a typed array with the specified number of elements.

**Signature:** `(size -- arr)`

```qd
10 make<i64> -> arr
```

---

## Array access

### len

Outputs the number of elements in an array.

**Signature:** `(arr -- len)`

```qd
arr len // number of elements
```

### nth

Outputs the element at the given index.

**Signature:** `(arr index -- value)`

```qd
arr 0 nth // first element
```

### set

Sets the element at the given index.

**Signature:** `(arr index value --)`

```qd
arr 0 42 set
```

---

## Modification

### append

Appends a value to the array, returning the modified array.

**Signature:** `(arr value -- arr)`

```qd
arr 42 append -> arr
```

### free

Frees the memory used by an array or struct.

**Signature:** `(arr --)`

```qd
arr free
```

---

## Array literals

Create arrays with literal syntax:

```qd
[1 2 3 4 5] -> arr
["a" "b" "c"] -> strings
[1.0 2.0 3.0] -> floats
```
