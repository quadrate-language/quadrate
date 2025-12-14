# `use` sort

Sorting algorithms for arrays.
Arrays are pointers to contiguous i64 values.

## Functions

### `fn` ints

Sort an array of i64 in ascending order (insertion sort).

**Signature:** `(arr:ptr count:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements |

**Example:**

```qd
arr count sort::ints
```

---

### `fn` ints_desc

Sort an array of i64 in descending order.

**Signature:** `(arr:ptr count:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements |

**Example:**

```qd
arr count sort::ints_desc
```

---

### `fn` is_sorted

Check if array is sorted in ascending order.

**Signature:** `(arr:ptr count:i64 -- sorted:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements |

| Output | Type | Description |
|--------|------|-------------|
| `sorted` | `i64` | 1 if sorted, 0 otherwise |

**Example:**

```qd
arr count sort::is_sorted  // result
```

---

### `fn` max

Find maximum value in array.

**Signature:** `(arr:ptr count:i64 -- maxval:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements (must be > 0) |

| Output | Type | Description |
|--------|------|-------------|
| `maxval` | `i64` | Maximum value |

**Example:**

```qd
arr count sort::max  // val
```

---

### `fn` min

Find minimum value in array.

**Signature:** `(arr:ptr count:i64 -- minval:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements (must be > 0) |

| Output | Type | Description |
|--------|------|-------------|
| `minval` | `i64` | Minimum value |

**Example:**

```qd
arr count sort::min  // val
```

---

### `fn` reverse

Reverse an array in place.

**Signature:** `(arr:ptr count:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of i64 values |
| `count` | `i64` | Number of elements |

**Example:**

```qd
arr count sort::reverse
```

---

### `fn` search

Binary search for value in sorted array.

**Signature:** `(arr:ptr count:i64 needle:i64 -- idx:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Sorted array of i64 values |
| `count` | `i64` | Number of elements |
| `needle` | `i64` | Value to find |

| Output | Type | Description |
|--------|------|-------------|
| `idx` | `i64` | Index if found, -1 otherwise |

**Example:**

```qd
arr count 42 sort::search  // idx
```

