# hof

Higher-Order Function combinators.
Combinators are functions that combine or apply other functions in useful patterns.
They enable functional programming without explicit temporary variables.

## Functions

### `fn` apply

Apply a function to a value.

**Signature:** `( x:i64 f:ptr -- r:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | The input value |
| `f` | `ptr` | Function pointer (i64 -- i64) |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | Result of f(x) |

**Example:**

```qd
5 fn (x:i64 -- r:i64) { 2 * } hof::apply print nl  // 10
```

---

### `fn` bi

Apply two functions to the same value.

**Signature:** `( x:i64 f:ptr g:ptr -- a:i64 b:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | The input value |
| `f` | `ptr` | First function pointer (i64 -- i64) |
| `g` | `ptr` | Second function pointer (i64 -- i64) |

| Output | Type | Description |
|--------|------|-------------|
| `a` | `i64` | Result of f(x) |
| `b` | `i64` | Result of g(x) |

**Example:**

```qd
5 fn (x:i64 -- r:i64) { 2 * } fn (x:i64 -- r:i64) { 3 + } hof::bi  // Stack: 10 8
```

---

### `fn` bi_star

Apply two functions to two values (first to first, second to second).

**Signature:** `( x:i64 y:i64 f:ptr g:ptr -- a:i64 b:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | First value |
| `y` | `i64` | Second value |
| `f` | `ptr` | Function for first value (i64 -- i64) |
| `g` | `ptr` | Function for second value (i64 -- i64) |

| Output | Type | Description |
|--------|------|-------------|
| `a` | `i64` | Result of f(x) |
| `b` | `i64` | Result of g(y) |

**Example:**

```qd
3 4 fn (x:i64 -- r:i64) { 1 + } fn (x:i64 -- r:i64) { 2 * } hof::bi_star  // Stack: 4 8
```

---

### `fn` both

Apply a function to two values separately.

**Signature:** `( x:i64 y:i64 f:ptr -- a:i64 b:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | First value |
| `y` | `i64` | Second value |
| `f` | `ptr` | Function pointer (i64 -- i64) |

| Output | Type | Description |
|--------|------|-------------|
| `a` | `i64` | Result of f(x) |
| `b` | `i64` | Result of f(y) |

**Example:**

```qd
3 4 fn (x:i64 -- r:i64) { dup * } hof::both  // Stack: 9 16
```

---

### `fn` dip

Apply a function to the second stack element, preserving the top.

**Signature:** `( x:i64 y:i64 f:ptr -- r:i64 top:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Second element (will have f applied) |
| `y` | `i64` | Top element (preserved) |
| `f` | `ptr` | Function pointer (i64 -- i64) |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | Result of f(x) |
| `y` | `i64` | Original top value (preserved) |

**Example:**

```qd
10 20 fn (x:i64 -- r:i64) { 2 * } hof::dip  // Stack: 20 20
```

---

### `fn` keep

Apply a function but keep the original value.

**Signature:** `( x:i64 f:ptr -- r:i64 orig:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | The input value |
| `f` | `ptr` | Function pointer (i64 -- i64) |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | Result of f(x) |
| `x` | `i64` | Original value (preserved) |

**Example:**

```qd
5 fn (x:i64 -- r:i64) { 2 * } hof::keep  // Stack: 10 5
```

---

### `fn` times

Apply function n times to an initial value.

**Signature:** `( x:i64 n:i64 f:ptr -- r:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Initial value |
| `n` | `i64` | Number of times to apply |
| `f` | `ptr` | Function pointer (i64 -- i64) |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | Result after n applications |

**Example:**

```qd
2 4 fn (x:i64 -- r:i64) { dup * } hof::times print nl  // 65536 (2^16)
```

---

### `fn` tri

Apply three functions to the same value.

**Signature:** `( x:i64 f:ptr g:ptr h:ptr -- a:i64 b:i64 c:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | The input value |
| `f` | `ptr` | First function pointer (i64 -- i64) |
| `g` | `ptr` | Second function pointer (i64 -- i64) |
| `h` | `ptr` | Third function pointer (i64 -- i64) |

| Output | Type | Description |
|--------|------|-------------|
| `a` | `i64` | Result of f(x) |
| `b` | `i64` | Result of g(x) |
| `c` | `i64` | Result of h(x) |

**Example:**

```qd
5 fn (x:i64 -- r:i64) { 1 + } fn (x:i64 -- r:i64) { 2 * } fn (x:i64 -- r:i64) { dup * } hof::tri  // Stack: 6 10 25
```

---

### `fn` unless

Apply function only if condition is false, otherwise return value unchanged.

**Signature:** `( x:i64 cond:i64 f:ptr -- r:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | The input value |
| `cond` | `i64` | Condition (0 = false, non-zero = true) |
| `f` | `ptr` | Function pointer (i64 -- i64) |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | Result of f(x) if cond is false, otherwise x |

**Example:**

```qd
5 0 fn (x:i64 -- r:i64) { 2 * } hof::unless print nl  // 10
5 1 fn (x:i64 -- r:i64) { 2 * } hof::unless print nl  // 5
```

---

### `fn` when

Apply function only if condition is true, otherwise return value unchanged.

**Signature:** `( x:i64 cond:i64 f:ptr -- r:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | The input value |
| `cond` | `i64` | Condition (0 = false, non-zero = true) |
| `f` | `ptr` | Function pointer (i64 -- i64) |

| Output | Type | Description |
|--------|------|-------------|
| `r` | `i64` | Result of f(x) if cond is true, otherwise x |

**Example:**

```qd
5 1 fn (x:i64 -- r:i64) { 2 * } hof::when print nl  // 10
5 0 fn (x:i64 -- r:i64) { 2 * } hof::when print nl  // 5
```
