# Tutorial: Understanding the Stack

Quadrate is a **stack-based** programming language. If you're coming from languages like Python, JavaScript, or C, this concept might be new to you. This tutorial will help you understand how stacks work and why they make Quadrate powerful.

## What is a Stack?

A stack is a data structure where you can only add or remove items from the top - like a stack of plates. The last item you put on is the first one you can take off (LIFO - Last In, First Out).

In Quadrate, all computation happens on the stack:

- **Push**: Put a value on top of the stack
- **Pop**: Remove and use the value on top

## Basic Stack Operations

### Pushing Values

When you write a literal value, it gets pushed onto the stack:

```qd
fn main() {
    42        // Push integer 42
    3.14      // Push float 3.14
    "hello"   // Push string "hello"
}
```

### Printing Values

The `print` instruction pops a value and prints it:

```qd
fn main() {
    42 print nl      // Prints: 42
    "hello" print nl // Prints: hello
}
```

### Arithmetic

Arithmetic operators pop two values and push the result:

```qd
fn main() {
    3 4 +     // Push 3, push 4, add them -> 7 on stack
    print nl  // Prints: 7

    10 3 -    // 10 - 3 = 7
    print nl  // Prints: 7

    6 7 *     // 6 * 7 = 42
    print nl  // Prints: 42

    20 4 /    // 20 / 4 = 5
    print nl  // Prints: 5
}
```

## Stack Manipulation

### `dup` - Duplicate

Copy the top value:

```qd
fn main() {
    5 dup     // Stack: [5, 5]
    + print nl // 5 + 5 = 10
}
```

### `drop` - Discard

Remove the top value:

```qd
fn main() {
    1 2 3     // Stack: [1, 2, 3]
    drop      // Stack: [1, 2]
    drop      // Stack: [1]
    print nl  // Prints: 1
}
```

### `swap` - Swap Top Two

Exchange the two top values:

```qd
fn main() {
    1 2       // Stack: [1, 2]
    swap      // Stack: [2, 1]
    print nl  // Prints: 1
    print nl  // Prints: 2
}
```

### `over` - Copy Second

Copy the second value to the top:

```qd
fn main() {
    1 2       // Stack: [1, 2]
    over      // Stack: [1, 2, 1]
    print nl  // Prints: 1
    print nl  // Prints: 2
    print nl  // Prints: 1
}
```

### `rot` - Rotate Top Three

Rotate the top three values:

```qd
fn main() {
    1 2 3     // Stack: [1, 2, 3]
    rot       // Stack: [2, 3, 1]
    print nl  // Prints: 1
    print nl  // Prints: 3
    print nl  // Prints: 2
}
```

## Local Variables

While stack manipulation is powerful, sometimes you need named values. Use `->` to pop into a local variable:

```qd
fn main() {
    42 -> x       // Pop 42 into variable x
    x print nl    // Push x, print it

    x x * -> square  // x squared
    square print nl
}
```

## Function Signatures

Functions declare their stack effect with `(inputs -- outputs)`:

```qd
// Takes two integers, returns one integer
fn add(a:i64 b:i64 -- sum:i64) {
    +
}

// Takes one integer, returns two integers
fn dup_and_square(n:i64 -- n:i64 squared:i64) {
    dup dup *
}

fn main() {
    3 4 add print nl       // Prints: 7
    5 dup_and_square
    print nl               // Prints: 25
    print nl               // Prints: 5
}
```

### Reading Stack Effects

The signature `fn foo(a:i64 b:i64 -- c:i64)` means:

- **Before call**: Stack has `[..., a, b]` (b on top)
- **After call**: Stack has `[..., c]`

## Types

Quadrate has these basic types:

| Type | Description | Example |
|------|-------------|---------|
| `i64` | 64-bit integer | `42`, `-17` |
| `f64` | 64-bit float | `3.14`, `-0.5` |
| `str` | String | `"hello"` |
| `ptr` | Pointer (for structs, arrays) | - |

### Type Casting

Convert between types with the `cast<T>` operator:

```qd
fn main() {
    42 cast<f64> print nl     // Integer to float: 42.0
    3.7 cast<i64> print nl    // Float to integer: 3 (truncates)
    65 cast<str> print nl     // Integer to string: "65"
}
```

## Comparison Operators

Comparisons pop two values and push 1 (true) or 0 (false):

```qd
fn main() {
    5 3 >  print nl  // 5 > 3?  Prints: 1
    5 3 <  print nl  // 5 < 3?  Prints: 0
    5 5 == print nl  // 5 == 5? Prints: 1
    5 3 != print nl  // 5 != 3? Prints: 1
    5 5 >= print nl  // 5 >= 5? Prints: 1
    5 5 <= print nl  // 5 <= 5? Prints: 1
}
```

## Booleans and Logic

Quadrate uses `true` (1) and `false` (0):

```qd
fn main() {
    true print nl      // Prints: 1
    false print nl     // Prints: 0

    true false and print nl   // 0
    true false or print nl    // 1
    true not print nl         // 0
}
```

## Control Flow

### If Statements

`if` pops a condition and executes the block if true:

```qd
fn main() {
    5 3 > if {
        "Five is greater than three" print nl
    }

    // With else
    10 20 > if {
        "10 > 20" print nl
    } else {
        "10 <= 20" print nl
    }
}
```

### If as Expression

`if` can leave a value on the stack:

```qd
fn main() {
    5 3 > if { "yes" } else { "no" } print nl  // Prints: yes
}
```

### For Loops

`for` takes start, end, step:

```qd
fn main() {
    // Count 0 to 4
    0 5 1 for i {
        i print nl
    }

    // Count by 2s
    0 10 2 for i {
        i print nl  // 0, 2, 4, 6, 8
    }

    // Count down
    5 0 -1 for i {
        i print nl  // 5, 4, 3, 2, 1
    }
}
```

### Loop and Break

Infinite loop with `break`:

```qd
fn main() {
    0 -> count
    loop {
        count print nl
        count 1 + -> count
        count 5 >= if {
            break
        }
    }
}
```

## The `ctx` Block

The `ctx` block creates an isolated computation context:

```qd
fn main() {
    10 20 30      // Stack: [10, 20, 30]
    ctx {
        // Child gets copy: [10, 20, 30]
        + +       // 10 + 20 + 30 = 60
    }
    // Parent stack unchanged, result added: [10, 20, 30, 60]
    print nl      // Prints: 60
    print nl      // Prints: 30
    print nl      // Prints: 20
    print nl      // Prints: 10
}
```

## Practical Example: Factorial

```qd
fn factorial(n:i64 -- result:i64) {
    dup 1 <= if {
        drop 1
    } else {
        dup 1 - factorial *
    }
}

fn main() {
    5 factorial print nl   // Prints: 120
    10 factorial print nl  // Prints: 3628800
}
```

## Practical Example: Fibonacci

```qd
fn fib(n:i64 -- result:i64) {
    dup 2 < if {
        // Base case: return n
    } else {
        dup 1 - fib
        swap 2 - fib
        +
    }
}

fn main() {
    0 10 1 for i {
        i fib print nl
    }
}
```

## Summary

Key concepts:

1. **Values are pushed onto the stack** by writing them
2. **Operations pop their arguments** and push results
3. **Stack manipulation** (`dup`, `drop`, `swap`, `over`, `rot`) lets you arrange values
4. **Local variables** (`-> name`) store values when stack manipulation gets complex
5. **Function signatures** (`fn name(inputs -- outputs)`) declare stack effects
6. **Control flow** uses values from the stack

## Next Steps

- **Next:** [Structs Tutorial](tutorial-structs.md) - Working with structured data
- [Standard Library](stdlib/index.md) - Available modules and functions

