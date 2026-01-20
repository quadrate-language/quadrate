# Higher-order functions

Higher-order functions (HOF) are functions that take other functions as arguments. The `hof` module provides **combinators**—functions that combine or apply other functions in useful patterns.

## Why combinators?

In stack-based programming, you often need to:

- Apply multiple operations to the same value
- Keep a value while also processing it
- Apply operations conditionally

Without combinators, you need temporary variables:

```qd
fn main() {
	// Without combinators: Is n both positive AND even?
	6 -> n
	n 0 > -> is_positive
	n 2 % 0 == -> is_even
	is_positive is_even and print nl  // 1
}
```

With combinators, it's one line:

```qd
use hof

fn main() {
	6 fn (x:i64 -- r:i64) { 0 > } fn (x:i64 -- r:i64) { 2 % 0 == } hof::bi and print nl  // 1
}
```

## Available combinators

### apply

Apply a single function to a value.

```qd
use hof

fn main() {
	5 fn (x:i64 -- r:i64) { 2 * } hof::apply print nl  // 10
}
```

### bi

Apply **two** functions to the **same** value.

```qd
use hof

fn main() {
	5 fn (x:i64 -- r:i64) { 2 * } fn (x:i64 -- r:i64) { 3 + } hof::bi
	print nl print nl  // 8 then 10 (5+3=8, 5*2=10)
}
```

### tri

Apply **three** functions to the **same** value.

```qd
use hof

fn main() {
	5 fn (x:i64 -- r:i64) { 1 + } fn (x:i64 -- r:i64) { 2 * } fn (x:i64 -- r:i64) { dup * } hof::tri
	print nl print nl print nl  // 25 then 10 then 6 (5*5=25, 5*2=10, 5+1=6)
}
```

### keep

Apply a function but **preserve the original** value.

```qd
use hof

fn main() {
	5 fn (x:i64 -- r:i64) { 2 * } hof::keep
	print nl print nl  // 5 then 10 (original=5 preserved, result=10)
}
```

### dip

Apply a function to the **second** element, preserving the **top**.

```qd
use hof

fn main() {
	10 20 fn (x:i64 -- r:i64) { 2 * } hof::dip
	print nl print nl  // 20 then 20 (kept 20 on top, doubled 10 to 20)
}
```

### both

Apply the **same** function to **two** values.

```qd
use hof

fn main() {
	3 4 fn (x:i64 -- r:i64) { dup * } hof::both
	print nl print nl  // 16 then 9 (4*4=16, 3*3=9)
}
```

### bi_star

Apply **different** functions to **two** values.

```qd
use hof

fn main() {
	3 4 fn (x:i64 -- r:i64) { 1 + } fn (x:i64 -- r:i64) { 2 * } hof::bi_star
	print nl print nl  // 8 then 4 (4*2=8, 3+1=4)
}
```

### when

Apply function **only if** condition is **true**.

```qd
use hof

fn main() {
	5 1 fn (x:i64 -- r:i64) { 2 * } hof::when print nl  // 10 (condition true)
	5 0 fn (x:i64 -- r:i64) { 2 * } hof::when print nl  // 5  (condition false, unchanged)
}
```

### unless

Apply function **only if** condition is **false** (opposite of `when`).

```qd
use hof

fn main() {
	5 0 fn (x:i64 -- r:i64) { 2 * } hof::unless print nl  // 10 (condition false)
	5 1 fn (x:i64 -- r:i64) { 2 * } hof::unless print nl  // 5  (condition true, unchanged)
}
```

### times

Apply a function **n times** to an initial value.

```qd
use hof

fn main() {
	1 5 fn (x:i64 -- r:i64) { 2 * } hof::times print nl  // 32 (1*2*2*2*2*2)

	2 3 fn (x:i64 -- r:i64) { dup * } hof::times print nl  // 256 (2^2=4, 4^2=16, 16^2=256)
}
```

## Practical examples

### Data validation

```qd
use hof

fn validate_age( age:i64 -- valid:i64 ) {
	-> age  // bind parameter

	// Must be: positive AND >= 18 AND <= 120
	age fn (x:i64 -- r:i64) { 0 > } fn (x:i64 -- r:i64) { 18 >= } hof::bi and
	age 120 <= and
}

fn main() {
	25 validate_age if { "Valid" } else { "Invalid" } print nl
	-5 validate_age if { "Valid" } else { "Invalid" } print nl
	150 validate_age if { "Valid" } else { "Invalid" } print nl
}
```

### Computing multiple results

```qd
use hof

fn stats( n:i64 -- doubled:i64 squared:i64 incremented:i64 ) {
	fn (x:i64 -- r:i64) { 2 * }
	fn (x:i64 -- r:i64) { dup * }
	fn (x:i64 -- r:i64) { 1 + }
	hof::tri
}

fn main() {
	5 stats
	print nl print nl print nl  // 6 then 25 then 10
}
```

### Iterative computation

```qd
use hof

fn main() {
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

## Comparison with factor

Quadrate's combinators are inspired by [Factor](https://factorcode.org/), but with explicit type signatures:

| Factor | Quadrate |
|--------|----------|
| `[ 2 * ]` | `fn (x:i64 -- r:i64) { 2 * }` |
| `5 [ 2 * ] [ 3 + ] bi` | `5 fn (...) { 2 * } fn (...) { 3 + } hof::bi` |

The signatures are more verbose but provide compile-time type checking.

## Limitations

Quadrate's anonymous functions have some limitations:

- Each must have an explicit type signature
- Variables are captured by reference (changes are visible to the closure)

For more advanced functional patterns, consider using named helper functions. See [Anonymous Functions](anonymous-functions.md) for details on closures and variable capture.

## What's next?

Now let's learn about [Memory Management](memory.md).
