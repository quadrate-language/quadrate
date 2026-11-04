# Generic Functions

Generic functions allow you to write type-flexible code that works with multiple types.

## Syntax

Use angle brackets `<T>` to define type parameters:

```qd
def identity<T>(x:T -- y:T) {
    x
}
```

## Basic usage

The compiler automatically infers types based on the arguments:

```qd
def identity<T>(x:T -- y:T) {
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
def pair<A, B>(a:A b:B -- a:A b:B) {
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

### Exchange

```qd
def exchange<T>(a:T b:T -- b:T a:T) {
    b a
}

fn main() {
    10 20 exchange      // the stack is now 20 10
    -> top -> below
    below print " " print top print nl  // 20 10
}
```

### Apply

```qd
def apply<T>(x:T f:fn(T -- T) -- result:T) {
    x f call
}

fn main() {
    5 def (n:i64 -- r:i64) { n 2 * } apply print nl  // 10
}
```

## Generic arrays

The `[n]T` literal creates typed arrays, and `T` may be a type parameter:

```qd
fn main() {
    [5]i64 -> ints    // Integer array
    [5]f64 -> floats  // Float array
    [5]str -> strings // String array
    [5]ptr -> ptrs    // Raw-memory array
}
```

## Type inference

The compiler infers types at the call site. Each call can use different types:

```qd
def identity<T>(x:T -- y:T) { x }

fn main() {
    42 identity print nl     // T = i64
    3.14 identity print nl   // T = f64
    "hi" identity print nl   // T = str
}
```

## Limitations

- A type parameter stands for any concrete type: `i64`, `f64`, `d128`, `str`, `ptr`, `[]T`,
  a function type or a struct
- No type constraints (all types are accepted)

## Implementation

Generics use monomorphization: the compiler generates specialized versions for each type combination used. This provides the same performance as hand-written specialized functions.
