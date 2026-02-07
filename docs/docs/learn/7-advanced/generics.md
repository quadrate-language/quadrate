# Generics

Generics let you write functions and structs that work with any type.

## Generic functions

Add type parameters in angle brackets after the function name:

```qd
fn identity<T>(x:T -- y:T) {
	-> val
	val
}

fn main() {
	42 identity print nl         // 42
	3.14 identity print nl       // 3.14
	"hello" identity print nl    // hello
}
```

The compiler infers the type `T` from the argument at each call site. No explicit type annotation is needed when calling.

## Multiple type parameters

Functions can have more than one type parameter:

```qd
fn pair<A, B>(a:A b:B -- a:A b:B) {
	-> second -> first
	first second
}

fn main() {
	1 "one" pair
	print nl   // one
	print nl   // 1
}
```

## Generic structs

Structs can also be generic:

```qd
struct Box<T> {
	value:T
}

fn main() {
	Box<i64> { value = 42 } -> b
	b @value print nl  // 42
}
```

!!! note
    Generic structs currently work best with integer types. For structs with mixed types, prefer non-generic structs with explicit field types.

## Built-in generic operations

### make

Create typed arrays:

```qd
10 make<i64> -> ints      // Array of 10 integers
5 make<f64> -> floats     // Array of 5 floats
3 make<str> -> strings    // Array of 3 strings
```

See [Arrays](../5-data-structures/arrays.md) for more on array operations.

### cast

Convert between types:

```qd
fn main() {
	3.14 cast<i64> print nl    // 3 (truncates)
	42 cast<f64> print nl      // 42
	100 cast<str> print nl     // 100
	"42" cast<i64> print nl    // 42
	"2.5" cast<f64> print nl   // 2.5
}
```

| From | To | Behavior |
|------|----|----------|
| `f64` | `i64` | Truncates decimal |
| `i64` | `f64` | Exact conversion |
| `i64` | `str` | Decimal string |
| `f64` | `str` | Decimal string |
| `str` | `i64` | Parses integer |
| `str` | `f64` | Parses float |

## How it works

The compiler uses **monomorphization**: it generates a specialized version of each generic function for every type combination used. This means generic code has the same performance as hand-written specialized functions.

## What's next?

Learn about [Memory Management](memory.md) for manual memory control.
