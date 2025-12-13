# Higher-Order Functions

Higher-order functions (HOF) are functions that take other functions as arguments. The `hof` module provides **combinators**—functions that combine or apply other functions in useful patterns.

## Why Combinators?

In stack-based programming, you often need to:

- Apply multiple operations to the same value
- Keep a value while also processing it
- Apply operations conditionally

Without combinators, you need temporary variables:

```quadrate
// Without combinators: Is n both positive AND even?
6 -> n
n 0 > -> is_positive
n 2 % 0 == -> is_even
is_positive is_even and
```

With combinators, it's one line:

```quadrate
use hof

6 fn (x:i64 -- r:i64) { 0 > } fn (x:i64 -- r:i64) { 2 % 0 == } hof::bi and
```

## Available Combinators

### apply

Apply a single function to a value.

```quadrate
use hof

5 fn (x:i64 -- r:i64) { 2 * } hof::apply
// Stack: 10
```

### bi

Apply **two** functions to the **same** value.

```quadrate
use hof

5 fn (x:i64 -- r:i64) { 2 * } fn (x:i64 -- r:i64) { 3 + } hof::bi
// Stack: 10 8
// (5*2=10, 5+3=8)
```

**Use case**: Check multiple conditions on the same value.

```quadrate
// Is age valid (positive) AND adult (>= 18)?
age fn (x:i64 -- r:i64) { 0 > } fn (x:i64 -- r:i64) { 18 >= } hof::bi and
```

### tri

Apply **three** functions to the **same** value.

```quadrate
use hof

5 fn (x:i64 -- r:i64) { 1 + } fn (x:i64 -- r:i64) { 2 * } fn (x:i64 -- r:i64) { dup * } hof::tri
// Stack: 6 10 25
// (5+1=6, 5*2=10, 5*5=25)
```

### keep

Apply a function but **preserve the original** value.

```quadrate
use hof

5 fn (x:i64 -- r:i64) { 2 * } hof::keep
// Stack: 10 5
// (result=10, original=5 preserved)
```

**Use case**: Process a value while keeping it for later.

```quadrate
// Print the value, then continue using it
value fn (x:i64 -- r:i64) { dup print nl } hof::keep drop
// Printed value, stack unchanged
```

### dip

Apply a function to the **second** element, preserving the **top**.

```quadrate
use hof

10 20 fn (x:i64 -- r:i64) { 2 * } hof::dip
// Stack: 20 20
// (doubled 10 to 20, kept 20 on top)
```

**Use case**: Process something "underneath" while holding onto a value.

### both

Apply the **same** function to **two** values.

```quadrate
use hof

3 4 fn (x:i64 -- r:i64) { dup * } hof::both
// Stack: 9 16
// (3*3=9, 4*4=16)
```

### bi_star

Apply **different** functions to **two** values.

```quadrate
use hof

3 4 fn (x:i64 -- r:i64) { 1 + } fn (x:i64 -- r:i64) { 2 * } hof::bi_star
// Stack: 4 8
// (3+1=4, 4*2=8)
```

### when

Apply function **only if** condition is **true**.

```quadrate
use hof

5 1 fn (x:i64 -- r:i64) { 2 * } hof::when   // 10 (condition true)
5 0 fn (x:i64 -- r:i64) { 2 * } hof::when   // 5  (condition false, unchanged)
```

**Use case**: Conditional transformation.

```quadrate
// Double negative numbers to make them "more negative"
value value 0 < fn (x:i64 -- r:i64) { 2 * } hof::when
```

### unless

Apply function **only if** condition is **false** (opposite of `when`).

```quadrate
use hof

5 0 fn (x:i64 -- r:i64) { 2 * } hof::unless  // 10 (condition false)
5 1 fn (x:i64 -- r:i64) { 2 * } hof::unless  // 5  (condition true, unchanged)
```

### times

Apply a function **n times** to an initial value.

```quadrate
use hof

1 5 fn (x:i64 -- r:i64) { 2 * } hof::times
// Stack: 32
// (1 * 2 * 2 * 2 * 2 * 2 = 32)

2 3 fn (x:i64 -- r:i64) { dup * } hof::times
// Stack: 256
// (2^2=4, 4^2=16, 16^2=256)
```

## Practical Examples

### Data Validation

```quadrate
use hof

fn validate_age( age:i64 -- valid:i64 ) {
    -> age

    // Must be: positive AND >= 18 AND <= 120
    age fn (x:i64 -- r:i64) { 0 > } fn (x:i64 -- r:i64) { 18 >= } hof::bi and
    age 120 <= and
}

fn main( -- ) {
    25 validate_age if { "Valid" } else { "Invalid" } print nl
    -5 validate_age if { "Valid" } else { "Invalid" } print nl
    150 validate_age if { "Valid" } else { "Invalid" } print nl
}
```

### Computing Multiple Results

```quadrate
use hof

fn stats( n:i64 -- doubled:i64 squared:i64 incremented:i64 ) {
    fn (x:i64 -- r:i64) { 2 * }
    fn (x:i64 -- r:i64) { dup * }
    fn (x:i64 -- r:i64) { 1 + }
    hof::tri
}

fn main( -- ) {
    5 stats
    // Stack: 10 25 6
}
```

### Iterative Computation

```quadrate
use hof

fn main( -- ) {
    // Compute 2^10 by doubling 10 times
    1 10 fn (x:i64 -- r:i64) { 2 * } hof::times
    print nl  // 1024

    // Compute factorial(5) iteratively
    1 -> result
    1 6 1 for i {
        result i * -> result
    }
    result print nl  // 120
}
```

## Comparison with Factor

Quadrate's combinators are inspired by [Factor](https://factorcode.org/), but with explicit type signatures:

| Factor | Quadrate |
|--------|----------|
| `[ 2 * ]` | `fn (x:i64 -- r:i64) { 2 * }` |
| `5 [ 2 * ] [ 3 + ] bi` | `5 fn (...) { 2 * } fn (...) { 3 + } hof::bi` |

The signatures are more verbose but provide compile-time type checking.

## Limitations

Quadrate's anonymous functions are **function pointers**, not full closures:

- Cannot capture local variables from enclosing scope
- Cannot be composed or manipulated as data
- Each must have an explicit type signature

For more advanced functional patterns, consider using named helper functions.
