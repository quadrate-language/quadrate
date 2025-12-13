# hof - Higher-Order Functions

The `hof` module provides combinators for functional programming with anonymous functions.

## Usage

```quadrate
use hof
```

## Functions

### apply

Apply a function to a value.

```quadrate
fn apply( x:i64 f:ptr -- r:i64 )
```

**Example:**
```quadrate
5 fn (x:i64 -- r:i64) { 2 * } hof::apply   // 10
```

---

### bi

Apply two functions to the same value, returning both results.

```quadrate
fn bi( x:i64 f:ptr g:ptr -- a:i64 b:i64 )
```

**Example:**
```quadrate
5 fn (x:i64 -- r:i64) { 2 * } fn (x:i64 -- r:i64) { 3 + } hof::bi
// Stack: 10 8
```

---

### tri

Apply three functions to the same value, returning all results.

```quadrate
fn tri( x:i64 f:ptr g:ptr h:ptr -- a:i64 b:i64 c:i64 )
```

**Example:**
```quadrate
5 fn (x:i64 -- r:i64) { 1 + } fn (x:i64 -- r:i64) { 2 * } fn (x:i64 -- r:i64) { dup * } hof::tri
// Stack: 6 10 25
```

---

### keep

Apply a function but preserve the original value.

```quadrate
fn keep( x:i64 f:ptr -- r:i64 orig:i64 )
```

**Example:**
```quadrate
5 fn (x:i64 -- r:i64) { 2 * } hof::keep
// Stack: 10 5
```

---

### dip

Apply a function to the second stack element, preserving the top.

```quadrate
fn dip( x:i64 y:i64 f:ptr -- r:i64 top:i64 )
```

**Example:**
```quadrate
10 20 fn (x:i64 -- r:i64) { 2 * } hof::dip
// Stack: 20 20  (doubled 10, kept 20 on top)
```

---

### both

Apply the same function to two values.

```quadrate
fn both( x:i64 y:i64 f:ptr -- a:i64 b:i64 )
```

**Example:**
```quadrate
3 4 fn (x:i64 -- r:i64) { dup * } hof::both
// Stack: 9 16
```

---

### bi_star

Apply two different functions to two values.

```quadrate
fn bi_star( x:i64 y:i64 f:ptr g:ptr -- a:i64 b:i64 )
```

**Example:**
```quadrate
3 4 fn (x:i64 -- r:i64) { 1 + } fn (x:i64 -- r:i64) { 2 * } hof::bi_star
// Stack: 4 8  (3+1=4, 4*2=8)
```

---

### when

Apply function only if condition is true (non-zero).

```quadrate
fn when( x:i64 cond:i64 f:ptr -- r:i64 )
```

**Example:**
```quadrate
5 1 fn (x:i64 -- r:i64) { 2 * } hof::when   // 10 (applied)
5 0 fn (x:i64 -- r:i64) { 2 * } hof::when   // 5  (unchanged)
```

---

### unless

Apply function only if condition is false (zero).

```quadrate
fn unless( x:i64 cond:i64 f:ptr -- r:i64 )
```

**Example:**
```quadrate
5 0 fn (x:i64 -- r:i64) { 2 * } hof::unless  // 10 (applied)
5 1 fn (x:i64 -- r:i64) { 2 * } hof::unless  // 5  (unchanged)
```

---

### times

Apply a function n times to an initial value.

```quadrate
fn times( x:i64 n:i64 f:ptr -- r:i64 )
```

**Example:**
```quadrate
1 5 fn (x:i64 -- r:i64) { 2 * } hof::times   // 32  (1*2*2*2*2*2)
2 3 fn (x:i64 -- r:i64) { dup * } hof::times // 256 (2^2^2^2 = 256)
```

## See Also

- [Anonymous Functions](../learn/7-advanced/anonymous-functions.md) - Creating function values
- [Higher-Order Functions Tutorial](../learn/7-advanced/higher-order-functions.md) - Detailed guide
