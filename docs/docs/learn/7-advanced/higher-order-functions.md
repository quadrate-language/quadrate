# Higher-order functions

A **combinator** runs function values in a pattern that stack code keeps repeating: run this
under the top value, run that and keep the value, run both of these on the same value. Quadrate
has ten built in, and the `hof` module has a few more.

## Why combinators?

Stack code often needs to:

- run several operations on the same value
- keep a value while also working on it
- get a value out of the way for a moment

Without combinators, that is what named locals are for:

```qd
fn main() {
	// Is n both positive and even?
	6 -> n
	n 0 > -> is_positive
	n 2 % 0 == -> is_even
	is_positive is_even and print nl  // 1
}
```

With one, the value never needs a name:

```qd
fn main() {
	6 fn { 0 > } fn { 2 % 0 == } bi and print nl  // 1
}
```

The function values are written `fn { … }`, with no signature: each one works on the stack, and
the compiler works out what it takes and leaves (see
[Anonymous functions](anonymous-functions.md#without-a-signature)). Written right before the
combinator like this, the body is checked against the real stack and compiled inline: there is
no function value at run time at all.

## Running function values on the stack

These run a function value on the stack as it is. The function value may take and leave any
number of values.

### dip

Run a function value on what is **under** the top value, then put the top value back.

```qd
fn main() {
	10 20 fn { 2 * } dip
	print nl print nl  // 20 then 20 (20 set aside, 10 doubled)

	1 2 3 fn { + } dip
	print nl print nl  // 3 then 3 (1 2 added under the 3)
}
```

### keep

Run a function value on the top value, and **keep** that value.

```qd
fn main() {
	5 fn { 2 * } keep
	print nl print nl  // 5 then 10
}
```

### dip2 and keep2

The same for **two** values: `dip2` runs a function value on what is under the top two and puts
them back; `keep2` runs it on the top two and keeps them.

```qd
fn main() {
	1 2 3 fn { 10 * } dip2
	print nl print nl print nl  // 3 then 2 then 10
	2 3 fn { * } keep2
	print nl print nl print nl  // 3 then 2 then 6
}
```

### bi and tri

Run **two** (or three) function values on the **same** value, one after the other.

```qd
fn main() {
	5 fn { 2 * } fn { 3 + } bi
	print nl print nl  // 8 then 10

	5 fn { 1 + } fn { 2 * } fn { dup * } tri
	print nl print nl print nl  // 25 then 10 then 6
}
```

### bi_star and tri_star

Run **one function value for each value**: the first on the first, the second on the second.
`tri_star` does it for three.

```qd
fn main() {
	3 4 fn { 1 + } fn { 2 * } bi_star
	print nl print nl  // 8 then 4

	1 2 3 fn { 10 * } fn { 20 * } fn { 30 * } tri_star
	print nl print nl print nl  // 90 then 40 then 10
}
```

The first function value runs on what is under the others, so it can reach further down, and one
that leaves nothing is fine: `l w c fn { show } fn { show } fn { show } tri_star` prints three
values in order.

### both

Run the **same** function value on **two** values.

```qd
fn main() {
	3 4 fn { dup * } both
	print nl print nl  // 16 then 9
}
```

### times

Run a function value **n times**. Each run starts on what the last one left, so it must leave
the stack as it found it.

```qd
fn main() {
	1 5 fn { 2 * } times print nl  // 32
	2 3 fn { dup * } times print nl  // 256
}
```

## Over arrays

### each

Run a function value with each element on top. Like `times`, it must leave the stack as it
found it, so what it works on below the element carries from one element to the next:

```qd
fn main() {
	0 [1 2 3 4] fn { + } each print nl  // 10
	[1 2 3] fn { print " " print } each nl  // 1 2 3
}
```

### map

A new array of what the function value leaves for each element. It takes one value and leaves
one, and the result may be of another type:

```qd
fn main() {
	[1 2 3] fn { dup * } map -> squares
	squares 2 nth print nl  // 9
	[1 2 3] fn { cast<str> } map 0 nth print nl  // 1
}
```

### filter

A new array of the elements for which the function value leaves a true flag:

```qd
fn main() {
	[1 2 3 4 5 6] fn { 2 % 0 == } filter len print nl  // 3
}
```

### fold

Run the function value on an accumulator and each element in turn, starting from an initial
value. What it leaves is the next accumulator:

```qd
fn main() {
	[1 2 3 4] 0 fn { + } fold print nl  // 10
	[1 2 3 4] 1 fn { * } fold print nl  // 24
}
```

### find, index, scan, any and all

Search an array, stopping at the first element that decides the answer. The function value
takes one element and leaves a flag. `find` leaves the element it found and `1`, or the zero of
the element type and `0`; `index` leaves its position, or `-1`:

```qd
fn main() {
	[1 5 12 3] fn { 10 > } any print nl  // 1
	[1 5 12 3] fn { 0 > } all print nl  // 1
	[1 5 12 3] fn { 4 > } find
	print nl print nl  // 1 then 5
	["a" "bb" "ccc"] fn { len 5 > } find
	print nl print nl  // 0 then the empty string
	[1 5 12 3] fn { 4 > } index print nl  // 1
	[1 5 12 3] fn { 40 > } index print nl  // -1
}
```

These are what a `for` loop with a `break` was for: `any` and `all` stop as soon as the answer
is known, which `fold` cannot do.

`scan` is `index` from a position on, as Forth's `SCAN` is, for a scan that goes on from
where the last one stopped:

```qd
fn main() {
	[3 8 5 12] 2 fn { 4 > } scan print nl  // 2
	[3 8 5 12] 9 fn { 4 > } scan print nl  // -1
}
```

All of them also search a string, one character at a time. Each character is its codepoint, an
`i64`, and a position is a character index, the one `strings::char_at` takes:

```qd
use unicode

fn main() {
	"key=value" fn { 61 == } index print nl  // 3
	"a=b=c" 2 fn { 61 == } scan print nl  // 3
	"héllo" fn { unicode::is_upper } any print nl  // 0
	"2026" fn { unicode::is_digit } all print nl  // 1
}
```

## Function values that are not written in place

A combinator runs any function value, not only one written right before it: one held in a local,
passed as a parameter, or a named function's `&name`. It is called rather than inlined, and is
checked by its type.

```qd
fn square(x:i64 -- r:i64) {
	dup *
}

fn main() {
	fn { 100 + } -> add100
	1 2 add100 dip print " " print print nl  // 2 101
	[1 2 3] &square map 2 nth print nl  // 9
}
```

## The `hof` module

`hof` has the less common ones: `when` and `unless`, which run a function value on a condition,
and `fold_right`.

```qd
use hof

fn main() {
	5 1 fn { 2 * } hof::when print nl  // 10
	5 0 fn { 2 * } hof::when print nl  // 5
}
```

## Practical examples

### Data validation

```qd
fn validate_age(age:i64 -- valid:i64) {
	// Must be at least 18 and at most 120
	fn { 18 >= } fn { 120 <= } bi and
}

fn main() {
	25 validate_age if { "Valid" } else { "Invalid" } print nl  // Valid
	-5 validate_age if { "Valid" } else { "Invalid" } print nl  // Invalid
	150 validate_age if { "Valid" } else { "Invalid" } print nl  // Invalid
}
```

### Several results from one value

```qd
fn stats(n:i64 -- doubled:i64 squared:i64 incremented:i64) {
	fn { 2 * } fn { dup * } fn { 1 + } tri
}

fn main() {
	5 stats
	print nl print nl print nl  // 6 then 25 then 10
}
```

### A pipeline over an array

```qd
fn main() {
	// The sum of the squares of the even numbers
	[1 2 3 4 5 6]
	fn { 2 % 0 == } filter
	fn { dup * } map
	0 fn { + } fold
	print nl  // 56
}
```

## Comparison with Factor

Quadrate's combinators are [Factor](https://factorcode.org/)'s, and so are their names, except
where Factor's has a character a Quadrate name cannot:

| Factor | Quadrate |
|--------|----------|
| `[ 2 * ]` | `fn { 2 * }` |
| `5 [ 2 * ] [ 3 + ] bi` | `5 fn { 2 * } fn { 3 + } bi` |
| `3 4 [ sq ] bi@` | `3 4 fn { dup * } both` |
| `3 4 [ 1 + ] [ 2 * ] bi*` | `3 4 fn { 1 + } fn { 2 * } bi_star` |
| `{ 1 2 3 } 0 [ + ] reduce` | `[1 2 3] 0 fn { + } fold` |

A function value used by a combinator cannot be fallible: the combinator would have nowhere to
send the failure. Handle it inside the function value, or `call` it and handle it where you are.
See [Anonymous functions](anonymous-functions.md) for closures and variable capture.

## What's next?

Now let's learn about [Generics](generics.md) to write type-flexible code.
