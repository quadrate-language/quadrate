# Thinking in stack-based

If you're coming from C, Python, Go, or JavaScript, Quadrate will feel backwards
at first. This guide shows how to translate your mental model so code stops
looking alien.

## The core shift: arguments come before the call

In most languages, you write the function name first and pass arguments in
parentheses:

```python
# Python
add(3, 4)
print(add(3, 4))
```

In Quadrate, arguments are pushed onto the stack *first*, then the function
consumes them:

```qd
// Quadrate
3 4 +           // push 3, push 4, add them
3 4 + print     // push 3, push 4, add, print
```

Read Quadrate code **left to right** as a sequence of stack operations. There
are no nested parentheses to untangle.

## Translating common patterns

### Variable assignment

```c
int x = 42;
int y = x * 2;
```

In Quadrate, you compute the value first, then bind it with `->`:

```qd
42 -> x
x 2 * -> y
```

Read it as: "push 42, store it as x", "push x, push 2, multiply, store as y".

### If/else

```python
if x > 10:
    print("big")
else:
    print("small")
```

The condition is computed first, then `if` pops it and branches:

```qd
x 10 > if {
    "big" print nl
} else {
    "small" print nl
}
```

### Function calls with multiple arguments

```go
result := clamp(value, 0, 100)
```

Push args, then call:

```qd
value 0 100 clamp -> result
```

### Method calls

```python
text = "hello".upper()
```

The receiver comes first, then the method name:

```qd
"hello" strings::upper -> text
```

### Chained method calls

```python
result = " hello ".strip().upper()
```

Each call consumes the previous result and produces a new one. Chains read
left-to-right like a pipeline:

```qd
" hello " strings::trim strings::upper -> result
```

### Conditional expressions

```rust
let color = if x > 0 { "positive" } else { "negative" };
```

`if` can leave a value on the stack:

```qd
x 0 > if { "positive" } else { "negative" } -> color
```

## Reading the stack

When you're confused, track what's on the stack at each step:

```qd
fn hypot(a:f64 b:f64 -- result:f64) {
    //            Stack: [a, b]
    dup *         // Stack: [a, b*b]
    swap dup *    // Stack: [b*b, a*a]
    +             // Stack: [a*a + b*b]
    math::sqrt    // Stack: [sqrt(a*a + b*b)]
}
```

The comment format `// Stack: [...]` is a convention used throughout the
stdlib. It documents the stack state after each line.

## When you reach for named parameters

If you find yourself tracking "which value is which" in a long chain, bind them
to names:

```qd
// Hard to read:
a b * c d + /

// Easier:
a b * -> num
c d + -> den
num den /
```

Or use named parameters in the function signature:

```qd
fn compute(a:f64 b:f64 c:f64 d:f64 -- result:f64) {
    a b *           // named params are on the stack
    c d +
    /
}
```

## When to use `dup`, `swap`, and friends

In most languages, you reference a variable by name whenever you need it:

```python
x * x + y * y
```

In Quadrate, you duplicate values with `dup` or use a local:

```qd
// Using dup
x dup *  y dup *  +

// Using locals (clearer for non-trivial cases)
x x *  y y *  +
```

The language provides both because each shines in different contexts. Short
calculations benefit from `dup`; anything with more than two intermediate
values reads better with named bindings.

## A worked example

Translate this Python function to Quadrate:

```python
def average_of_squares(nums):
    total = 0
    for n in nums:
        total += n * n
    return total / len(nums)
```

Step by step:

1. Input: an array of ints. Signature: `(nums:[]i64 -- result:f64)`
2. Iterate, accumulate squares into a running total.
3. Divide by length.

```qd
fn average_of_squares(nums:[]i64 -- result:f64) {
    0 -> total
    0 nums len 1 for i {
        nums i nth -> n
        total n n * + -> total
    }
    total cast<f64> nums len cast<f64> /
}
```

Walkthrough:

- `0 -> total` — initialize accumulator
- `0 nums len 1 for i` — iterate `i` from 0 to len-1 with step 1
- `nums i nth -> n` — get the i-th element
- `total n n * + -> total` — add n² to total
- After the loop, cast to f64 and divide

## The key insight

**Stack-based code is a pipeline, not a tree.** Most languages build a tree of
nested expressions; Quadrate builds a linear pipeline of operations that push
and pop values. Once the mental model clicks, reading becomes easier because
there's no structure to parse -- just follow the flow.

The biggest hurdle is *writing* at first, not *reading*. When writing, you
think bottom-up ("what do I need on the stack before calling `+`?"), which
feels unnatural until it becomes habit.
