# `use` hof

<!-- doccheck: page-context use hof -->

<!-- doccheck: page-setup [1 2 3] -> arr -->

Higher-Order Function combinators.

The combinators every program uses -- `dip`, `keep`, `bi`, `tri`, `bi_star`, `tri_star`,
`both`, `times`, `each`, `map`, `filter`, `fold`, `find`, `any` and `all` -- are builtins, and
need no `use`. This module has the rest: `when` and `unless`, which run a function value on a
condition, and `fold_right`.

Every one is generic, and `when` and `unless` take their function value with a stack
variable, `..a`, so it may have any arity.

## Functions

### `fn` fold_right

Fold an array right to left: `f` runs on each element and the accumulator, last element first, as `fold` runs on the accumulator and each element first to last.

**Signature:** `(arr:[]T acc:A f:fn(T A -- A) -- result:A)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `[]T` | Array |
| `acc` | `A` | Initial accumulator |
| `f` | `fn(T A -- A)` | Function value (elem acc -- acc) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `A` | Final accumulator |

**Example:**

```qd
arr 0 fn { + } hof::fold_right  // sum of array
```
---

### `fn` unless

Run a function value only if the condition is false, as `when` runs it when it is true.

**Signature:** `(..a cond:i64 q:fn(..a -- ..a) -- ..a)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `cond` | `i64` | Condition (0 = false, non-zero = true) |
| `q` | `fn` | Function value, run when the condition is false |

**Example:**

```qd
5 0 fn { 2 * } hof::unless print nl  // 10
5 1 fn { 2 * } hof::unless print nl  // 5
```
---

### `fn` when

Run a function value only if the condition is true. It must leave the stack the shape it found it, since when the condition is false nothing runs.

**Signature:** `(..a cond:i64 q:fn(..a -- ..a) -- ..a)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `cond` | `i64` | Condition (0 = false, non-zero = true) |
| `q` | `fn` | Function value, run when the condition is true |

**Example:**

```qd
5 1 fn { 2 * } hof::when print nl  // 10
5 0 fn { 2 * } hof::when print nl  // 5
```
