# Array Operations

Operations for creating and manipulating arrays.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `makei` | `( size -- arr )` | Create integer array |
| `makef` | `( size -- arr )` | Create float array |
| `makes` | `( size -- arr )` | Create string array |
| `makep` | `( size -- arr )` | Create pointer array |
| `make` | `( size -- arr )` | Create typed array |
| `len` | `( arr -- len )` | Get length |
| `nth` | `( arr index -- value )` | Get element |
| `set` | `( arr index value -- )` | Set element |
| `append` | `( arr value -- arr )` | Append element |
| `free` | `( arr -- )` | Free memory |

---

## Creating Arrays

### makei

Creates an array of size integers, initialized to 0.

**Signature:** `( size -- arr )`

```qd
10 makei -> arr
```

### makef

Creates an array of size floats, initialized to 0.0.

**Signature:** `( size -- arr )`

```qd
10 makef -> arr
```

### makes

Creates an array of size strings, initialized to empty.

**Signature:** `( size -- arr )`

```qd
10 makes -> arr
```

### makep

Creates an array of size pointers, initialized to null.

**Signature:** `( size -- arr )`

```qd
10 makep -> arr
```

### make

Creates a typed array (use with `make<Type>` syntax).

**Signature:** `( size -- arr )`

```qd
10 make<Point> -> points
```

---

## Array Access

### len

Outputs the number of elements in an array.

**Signature:** `( arr -- len )`

```qd
arr len // number of elements
```

### nth

Outputs the element at the given index.

**Signature:** `( arr index -- value )`

```qd
arr 0 nth // first element
```

### set

Sets the element at the given index.

**Signature:** `( arr index value -- )`

```qd
arr 0 42 set
```

---

## Modification

### append

Appends a value to the array, returning the modified array.

**Signature:** `( arr value -- arr )`

```qd
arr 42 append -> arr
```

### free

Frees the memory used by an array or struct.

**Signature:** `( arr -- )`

```qd
arr free
```

---

## Alternative Syntax

Arrays can also be accessed using `@[]` and `![]`:

```qd
// Read element
arr 0 @[] // Same as: arr 0 nth

// Write element
42 arr 0 ![] // Same as: arr 0 42 set
```

## Array Literals

Create arrays with literal syntax:

```qd
[1 2 3 4 5] -> arr
["a" "b" "c"] -> strings
[1.0 2.0 3.0] -> floats
```
