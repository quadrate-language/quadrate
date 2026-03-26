# `use` hof

Higher-order function combinators for functional programming. Provides function application, conditional combinators, and array operations.

```qd
use hof

fn main(--) {
    // Double a value
    5 fn (x:i64 -- r:i64) { x 2 * } hof::apply print nl  // 10

    // Sum an array with fold
    0 make<i64> 1 append 2 append 3 append 4 append 5 append -> arr
    arr 5 0 fn (acc:i64 x:i64 -- r:i64) { acc x + } hof::fold print nl  // 15
    arr free
}
```

## Functions

### Basic Combinators

### `fn` apply

Apply a function to a value.

**Signature:** `(x:i64 f:ptr -- r:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Input value |
| `f` | `ptr` | Function pointer `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | Result of f(x) |

**Example:**

```qd
5 fn (x:i64 -- r:i64) { x 2 * } hof::apply  // 10
```
---

### `fn` bi

Apply two functions to the same value.

**Signature:** `(x:i64 f:ptr g:ptr -- a:i64 b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Input value |
| `f` | `ptr` | First function `(i64 -- i64)` |
| `g` | `ptr` | Second function `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `a` | `i64` | Result of f(x) |
| `b` | `i64` | Result of g(x) |

**Example:**

```qd
5 fn (x:i64 -- r:i64) { x 1 + } fn (x:i64 -- r:i64) { x 2 * } hof::bi
-> b -> a  // a=6, b=10
```
---

### `fn` tri

Apply three functions to the same value.

**Signature:** `(x:i64 f:ptr g:ptr h:ptr -- a:i64 b:i64 c:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Input value |
| `f` | `ptr` | First function `(i64 -- i64)` |
| `g` | `ptr` | Second function `(i64 -- i64)` |
| `h` | `ptr` | Third function `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `a` | `i64` | Result of f(x) |
| `b` | `i64` | Result of g(x) |
| `c` | `i64` | Result of h(x) |

---

### `fn` keep

Apply a function but preserve the original value on the stack.

**Signature:** `(x:i64 f:ptr -- r:i64 orig:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Input value |
| `f` | `ptr` | Function pointer `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | Result of f(x) |
| `orig` | `i64` | Original value (preserved) |

**Example:**

```qd
5 fn (x:i64 -- r:i64) { x 2 * } hof::keep  // Stack: 10 5
```
---

### `fn` dip

Apply a function to the second stack element, preserving the top.

**Signature:** `(x:i64 y:i64 f:ptr -- r:i64 top:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Second element (will have f applied) |
| `y` | `i64` | Top element (preserved) |
| `f` | `ptr` | Function pointer `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | Result of f(x) |
| `top` | `i64` | Original top value (preserved) |

---

### `fn` both

Apply the same function to two values separately.

**Signature:** `(x:i64 y:i64 f:ptr -- a:i64 b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | First value |
| `y` | `i64` | Second value |
| `f` | `ptr` | Function pointer `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `a` | `i64` | Result of f(x) |
| `b` | `i64` | Result of f(y) |

**Example:**

```qd
3 4 fn (x:i64 -- r:i64) { x x * } hof::both  // Stack: 9 16
```
---

### `fn` bi_star

Apply different functions to two values (f to x, g to y).

**Signature:** `(x:i64 y:i64 f:ptr g:ptr -- a:i64 b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | First value |
| `y` | `i64` | Second value |
| `f` | `ptr` | Function for first value `(i64 -- i64)` |
| `g` | `ptr` | Function for second value `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `a` | `i64` | Result of f(x) |
| `b` | `i64` | Result of g(y) |

---

### Conditional Combinators

### `fn` when

Apply function only if condition is true, otherwise return value unchanged.

**Signature:** `(x:i64 cond:i64 f:ptr -- r:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Input value |
| `cond` | `i64` | Condition (0 = false, non-zero = true) |
| `f` | `ptr` | Function pointer `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | f(x) if cond is true, otherwise x |

**Example:**

```qd
5 1 fn (x:i64 -- r:i64) { x 2 * } hof::when   // 10
5 0 fn (x:i64 -- r:i64) { x 2 * } hof::when   // 5
```
---

### `fn` unless

Apply function only if condition is false, otherwise return value unchanged.

**Signature:** `(x:i64 cond:i64 f:ptr -- r:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Input value |
| `cond` | `i64` | Condition (0 = false, non-zero = true) |
| `f` | `ptr` | Function pointer `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | f(x) if cond is false, otherwise x |

---

### `fn` times

Apply function n times to a value.

**Signature:** `(x:i64 n:i64 f:ptr -- r:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Initial value |
| `n` | `i64` | Number of applications |
| `f` | `ptr` | Function pointer `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | Result after n applications |

**Example:**

```qd
1 4 fn (x:i64 -- r:i64) { x 2 * } hof::times   // 16
```
---

### Array Operations

### `fn` fold

Fold/reduce an array left-to-right with a binary function.

**Signature:** `(arr:ptr count:i64 init:i64 f:ptr -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array pointer |
| `count` | `i64` | Array length |
| `init` | `i64` | Initial accumulator value |
| `f` | `ptr` | Binary function `(acc:i64 elem:i64 -- acc:i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Final accumulated value |

**Example:**

```qd
arr 5 0 fn (acc:i64 x:i64 -- r:i64) { acc x + } hof::fold  // sum
```
---

### `fn` fold_right

Fold/reduce an array right-to-left with a binary function.

**Signature:** `(arr:ptr count:i64 init:i64 f:ptr -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array pointer |
| `count` | `i64` | Array length |
| `init` | `i64` | Initial accumulator value |
| `f` | `ptr` | Binary function `(elem:i64 acc:i64 -- acc:i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Final accumulated value |

---

### `fn` map

Map a function over an array, returning a new array. The caller must free the result.

**Signature:** `(arr:ptr count:i64 f:ptr -- result:ptr out_count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Input array pointer |
| `count` | `i64` | Array length |
| `f` | `ptr` | Function pointer `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `ptr` | New array with mapped values |
| `out_count` | `i64` | Array length (same as input) |

**Example:**

```qd
arr 3 fn (x:i64 -- r:i64) { x 2 * } hof::map -> count -> result
// result is a new array: [2, 4, 6]
result free
```
---

### `fn` filter

Filter an array, keeping only elements that satisfy a predicate. The caller must free the result.

**Signature:** `(arr:ptr count:i64 pred:ptr -- result:ptr out_count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Input array pointer |
| `count` | `i64` | Array length |
| `pred` | `ptr` | Predicate function `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `ptr` | New array with matching elements |
| `out_count` | `i64` | Length of filtered array |

**Example:**

```qd
arr 5 fn (x:i64 -- r:i64) { x 2 % 0 == } hof::filter -> count -> result
// result contains only even numbers
result free
```
---

### `fn` any

Check if any element in the array satisfies a predicate.

**Signature:** `(arr:ptr count:i64 pred:ptr -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array pointer |
| `count` | `i64` | Array length |
| `pred` | `ptr` | Predicate function `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if any element satisfies pred, 0 otherwise |

---

### `fn` all

Check if all elements in the array satisfy a predicate.

**Signature:** `(arr:ptr count:i64 pred:ptr -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array pointer |
| `count` | `i64` | Array length |
| `pred` | `ptr` | Predicate function `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if all elements satisfy pred, 0 otherwise |

---

### `fn` find

Find the first element satisfying a predicate.

**Signature:** `(arr:ptr count:i64 pred:ptr -- elem:i64 found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array pointer |
| `count` | `i64` | Array length |
| `pred` | `ptr` | Predicate function `(i64 -- i64)` |

| Output | Type | Description |
|--------|------|-------------|
| `elem` | `i64` | Found element (or 0 if not found) |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
arr 4 fn (x:i64 -- r:i64) { x 2 % 0 == } hof::find -> found -> elem
found if {
    "first even: " print elem print nl
}
```
