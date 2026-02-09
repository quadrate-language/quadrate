# `use` sort

Sorting algorithms and array utilities. Provides in-place sorting for integer and string arrays, binary search, min/max, reverse, and sorted-check.

Arrays are represented as pointers to contiguous 8-byte values, allocated with `mem::alloc!`.

```qd
use sort
use mem

fn main(--) {
    5 8 * mem::alloc! -> arr
    50 arr 0 mem::set_i64
    10 arr 8 mem::set_i64
    30 arr 16 mem::set_i64
    40 arr 24 mem::set_i64
    20 arr 32 mem::set_i64

    arr 5 sort::ints                    // [10, 20, 30, 40, 50]
    arr 5 sort::min print nl            // 10
    arr 5 sort::max print nl            // 50
    arr 5 30 sort::search print nl      // 2

    arr mem::free
}
```

## Functions

### `fn` ints

Sort an array of i64 in ascending order (insertion sort). Modifies the array in place.

**Signature:** `(arr:ptr count:i64 --)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements |

**Example:**

```qd
arr 5 sort::ints
```
---

### `fn` ints_desc

Sort an array of i64 in descending order. Modifies the array in place.

**Signature:** `(arr:ptr count:i64 --)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements |

**Example:**

```qd
arr 5 sort::ints_desc
```
---

### `fn` min

Find the minimum value in an array.

**Signature:** `(arr:ptr count:i64 -- minval:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements (must be > 0) |

| Output | Type | Description |
|--------|------|-------------|
| `minval` | `i64` | Minimum value in the array |

**Example:**

```qd
arr 5 sort::min -> smallest
```
---

### `fn` max

Find the maximum value in an array.

**Signature:** `(arr:ptr count:i64 -- maxval:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements (must be > 0) |

| Output | Type | Description |
|--------|------|-------------|
| `maxval` | `i64` | Maximum value in the array |

**Example:**

```qd
arr 5 sort::max -> largest
```
---

### `fn` is_sorted

Check if an array is sorted in ascending order.

**Signature:** `(arr:ptr count:i64 -- sorted:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements |

| Output | Type | Description |
|--------|------|-------------|
| `sorted` | `i64` | 1 if sorted ascending, 0 otherwise |

**Example:**

```qd
arr 5 sort::is_sorted if {
    "sorted!" print nl
}
```
---

### `fn` reverse

Reverse an array in place.

**Signature:** `(arr:ptr count:i64 --)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements |

**Example:**

```qd
arr 5 sort::reverse
```
---

### `fn` search

Binary search for a value in a sorted array. The array **must** be sorted in ascending order. Returns the index if found, or -1 if not found.

**Signature:** `(arr:ptr count:i64 needle:i64 -- idx:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Sorted array of i64 values |
| `count` | `i64` | Number of elements |
| `needle` | `i64` | Value to search for |

| Output | Type | Description |
|--------|------|-------------|
| `idx` | `i64` | Index of element, or -1 if not found |

**Example:**

```qd
arr 5 42 sort::search -> idx
idx -1 == if {
    "not found" print nl
} else {
    "found at index " print idx print nl
}
```
---

### `fn` strings

Sort an array of strings in ascending alphabetical order (insertion sort). Modifies the array in place.

**Signature:** `(arr:ptr count:i64 --)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of string pointers |
| `count` | `i64` | Number of elements |

String arrays typically come from functions like `str::lines!` or `os::glob` which return a pointer and count.

**Example:**

```qd
// Sort lines read from a file
data str::lines! -> count -> lines
lines count sort::strings
```
---

### `fn` strings_desc

Sort an array of strings in descending alphabetical order. Modifies the array in place.

**Signature:** `(arr:ptr count:i64 --)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of string pointers |
| `count` | `i64` | Number of elements |

**Example:**

```qd
lines count sort::strings_desc
```
