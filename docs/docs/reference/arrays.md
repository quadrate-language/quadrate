# Array operations

Operations for creating and manipulating arrays.

## Overview

Arrays are created by a literal; there is no creation instruction. The operations below act
on one once it exists.

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `len` | `(arr -- len)` | Get length |
| `nth` | `(arr index -- value)` | Get element |
| `set` | `(arr index value --)` | Set element |
| `append` | `(arr value -- arr)` | Append element |
| `free` | `(arr --)` | Free memory |

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

## Creating arrays

Every array comes from a `[…]` literal. Whether the brackets hold **elements** or a **size**
is decided by what follows the `]`: a type name immediately after it, with no whitespace,
makes the brackets a size.

### Elements

```qd
[1 2 3 4 5] -> arr
["a" "b" "c"] -> strings
[1.0 2.0 3.0] -> floats
[] -> empty          // element type decided by the first append
```

### A size and an element type

`[n]T` creates `n` elements, each the zero value of `T`.

```qd
[10]i64 -> ints      // ten 0
[10]f64 -> floats    // ten 0.0
[10]str -> strings   // ten ""
[10]ptr -> ptrs      // ten null
[]i64   -> empty     // the size left out, so none of them -- an empty []i64
```

The size may be any expression leaving one `i64`:

```qd
[src len]i64 -> dst
[n 2 *]i64   -> twice
```

| `T` | zero value |
|-----|------------|
| `i64`, `i32`, `i16`, `i8`, `u64`, `u32`, `u16`, `u8` | `0` |
| `f64`, `f32` | `0.0` |
| `str` | `""` — an empty string, not null |
| `ptr` | null |
| a struct | a distinct `T {}` per element — see below |

### Arrays of structs

`[n]T` for a struct holds `n` **distinct instances**, each built as `T {}`. It is allowed
exactly where `T {}` is: every field needs a default.

```qd
struct Point { x:f64 = 0.0  y:f64 = 0.0 }

[3]Point -> ps
ps 0 nth <<x print nl    // 0 -- a real Point, not a null slot
ps 0 nth 9.0 >>x drop    // writing one leaves the others alone
```

A struct whose fields have no defaults has no zero, so `[n]T` reports the field that is
missing one. Build those with `[]T` and `append` instead.

!!! note "`[n]T` is an expression, not a type"

    Arrays are dynamic, and `[]T` is the only array type there is. `[10]i64` and `[]i64`
    both produce a value of type `[]i64`, differing only in starting length, so a size
    never appears in a signature or a field declaration.
