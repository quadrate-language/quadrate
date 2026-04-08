# Generic Functions

Generic functions allow you to write type-flexible code that works with multiple types.

## Syntax

Use angle brackets `<T>` to define type parameters:

```qd
fn identity<T>(x:T -- y:T) {
    x
}
```

## Basic usage

The compiler automatically infers types based on the arguments:

```qd
fn identity<T>(x:T -- y:T) {
    x
}

fn main() {
    42 identity print nl        // Works with i64
    3.14 identity print nl      // Works with f64
    "hello" identity print nl   // Works with str
}
```

## Multiple type parameters

Functions can have multiple type parameters:

```qd
fn pair<A B>(a:A b:B -- a:A b:B) {
    a b
}

fn main() {
    1 "one" pair
    -> s
    -> n
    n print " " print s print nl  // 1 one
}
```

## Common patterns

### Swap

```qd
fn swap<T>(a:T b:T -- b:T a:T) {
    b a
}

fn main() {
    10 20 swap
    -> a -> b
    a print " " print b print nl  // 10 20
}
```

### Apply

```qd
fn apply<T>(x:T f:ptr -- result:T) {
    x f call
}

fn main() {
    5 fn (n:i64 -- r:i64) { n 2 * } apply print nl  // 10
}
```

## Generic arrays

The `make<T>` builtin creates typed arrays:

```qd
fn main() {
    5 make<i64> -> ints    // Integer array
    5 make<f64> -> floats  // Float array
    5 make<str> -> strings // String array
    5 make<ptr> -> ptrs    // Pointer array
}
```

## Type inference

The compiler infers types at the call site. Each call can use different types:

```qd
fn identity<T>(x:T -- y:T) { x }

fn main() {
    42 identity      // T = i64
    3.14 identity    // T = f64
    "hi" identity    // T = str
}
```

## Limitations

- Type parameters must be concrete types (`i64`, `f64`, `str`, `ptr`, `[]T`)
- No type constraints (all types are accepted)
- Generic structs are supported but currently work best with `i64` fields

## Implementation

Generics use monomorphization: the compiler generates specialized versions for each type combination used. This provides the same performance as hand-written specialized functions.
