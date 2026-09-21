# `use` sort

<!-- doccheck: page-context use sort -->

<!-- doccheck: page-context fn by_length(a:i64 b:i64 -- order:i64) { a b - } -->

<!-- doccheck: page-context fn ascending(a:i64 b:i64 -- order:i64) { a b - } -->

<!-- doccheck: page-setup [5 2 8 1] -> arr -->

<!-- doccheck: page-setup [5.0 2.0 8.0 1.0] -> farr -->

<!-- doccheck: page-setup ["b" "a" "c" "d"] -> strs -->

Sorting algorithms for arrays.
Every entry point takes a `[]T` -- `[]i64`, `[]f64` or `[]str` -- and nothing takes a count
beside it: the array carries its length, and saying it twice is a chance to say it wrong.
The sorts reorder in place; `min`, `max`, `search` and the `is_sorted` pair only read.

## Functions

### `fn` by

Sort an array of i64 by a comparator (quicksort). The comparator returns a negative value if `a` sorts before `b`, zero if they tie, and a positive value if `a` sorts after `b` -- the same convention as C's `qsort`. Sorting by a key, by several fields, or in descending order are all just comparators. Not stable.

**Signature:** `(arr:[]i64 cmp:fn(i64 i64 -- i64) -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Array of i64 values |
| `cmp` | `fn` | Comparator (a b -- ordering) |

**Example:**

```qd
arr &by_length sort::by
```
---

### `fn` floats

Sort an array of f64 in ascending order (quicksort).

**Signature:** `(arr:[]f64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]f64` | Array of f64 values |

**Example:**

```qd
farr sort::floats
```
---

### `fn` floats_desc

Sort an array of f64 in descending order (quicksort).

**Signature:** `(arr:[]f64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]f64` | Array of f64 values |

**Example:**

```qd
farr sort::floats_desc
```
---

### `fn` ints

Sort an array of i64 in ascending order (quicksort).

**Signature:** `(arr:[]i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Array of i64 values |

**Example:**

```qd
arr sort::ints
```
---

### `fn` ints_desc

Sort an array of i64 in descending order (quicksort).

**Signature:** `(arr:[]i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Array of i64 values |

**Example:**

```qd
arr sort::ints_desc
```
---

### `fn` is_sorted

Check if array of i64 is sorted in ascending order.

**Signature:** `(arr:[]i64 -- sorted:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Array of i64 values |

| Output | Type | Description |
|--------|------|-------------|
| `sorted` | `i64` | 1 if sorted, 0 otherwise |

**Example:**

```qd
arr sort::is_sorted  // result
```
---

### `fn` is_sorted_by

Whether an array of i64 is sorted according to a comparator.

**Signature:** `(arr:[]i64 cmp:fn(i64 i64 -- i64) -- sorted:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Array of i64 values |
| `cmp` | `fn` | Comparator (a b -- ordering) |

| Output | Type | Description |
|--------|------|-------------|
| `sorted` | `i64` | 1 if sorted, 0 otherwise |

**Example:**

```qd
arr &by_length sort::is_sorted_by print nl
```
---

### `fn` is_sorted_floats

Check if array of f64 is sorted in ascending order.

**Signature:** `(arr:[]f64 -- sorted:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]f64` | Array of f64 values |

| Output | Type | Description |
|--------|------|-------------|
| `sorted` | `i64` | 1 if sorted, 0 otherwise |

**Example:**

```qd
farr sort::is_sorted_floats  // result
```
---

### `fn` lower_bound_by

Find the index of the first element not ordered before `needle` (binary search lower bound). The array must already be sorted by the same comparator. Returns `count` when every element sorts before `needle`.

**Signature:** `(arr:[]i64 needle:i64 cmp:fn(i64 i64 -- i64) -- idx:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Array of i64 values, sorted by `cmp` |
| `needle` | `i64` | Value to locate |
| `cmp` | `fn` | Comparator (a b -- ordering) |

| Output | Type | Description |
|--------|------|-------------|
| `idx` | `i64` | Index of the first element not ordered before `needle` |

**Example:**

```qd
arr 42 &ascending sort::lower_bound_by print nl
```
---

### `fn` max

Find maximum value in array.

**Signature:** `(arr:[]i64 -- maxval:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Array of i64 values (must not be empty) |

| Output | Type | Description |
|--------|------|-------------|
| `maxval` | `i64` | Maximum value |

**Example:**

```qd
arr sort::max  // val
```
---

### `fn` min

Find minimum value in array.

**Signature:** `(arr:[]i64 -- minval:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Array of i64 values (must not be empty) |

| Output | Type | Description |
|--------|------|-------------|
| `minval` | `i64` | Minimum value |

**Example:**

```qd
arr sort::min  // val
```
---

### `fn` reverse

Reverse an array in place.

**Signature:** `(arr:[]i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Array of i64 values |

**Example:**

```qd
arr sort::reverse
```
---

### `fn` search

Binary search for value in sorted array.

**Signature:** `(arr:[]i64 needle:i64 -- idx:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Sorted array of i64 values |
| `needle` | `i64` | Value to find |

| Output | Type | Description |
|--------|------|-------------|
| `idx` | `i64` | Index if found, -1 otherwise |

**Example:**

```qd
arr 42 sort::search  // idx
```
---

### `fn` strings

Sort an array of strings in ascending alphabetical order (insertion sort).

**Signature:** `(arr:[]str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]str` | Array of strings |

**Example:**

```qd
strs sort::strings
```
---

### `fn` strings_desc

Sort an array of strings in descending alphabetical order.

**Signature:** `(arr:[]str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]str` | Array of strings |

**Example:**

```qd
strs sort::strings_desc
```
---

### `fn` unique

Remove adjacent duplicate i64 values in-place (array must be sorted).

**Signature:** `(arr:[]i64 -- new_count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]i64` | Sorted array of i64 values |

| Output | Type | Description |
|--------|------|-------------|
| `new_count` | `i64` | Number of unique elements |

**Example:**

```qd
arr sort::unique  // new_count
```
