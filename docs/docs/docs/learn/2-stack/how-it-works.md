# How the Stack Works

The stack is the core concept in Quadrate. Everything revolves around it.

## What is a Stack?

A stack is like a pile of plates:

- You can only add plates to the **top**
- You can only remove plates from the **top**
- The last plate you put on is the first one you take off (LIFO)

## Push and Pop

In Quadrate:

- **Push**: Writing a value adds it to the top
- **Pop**: Operations remove values from the top

```qd
fn main( -- ) {
	1       // Push 1      Stack: [1]
	2       // Push 2      Stack: [1, 2]
	3       // Push 3      Stack: [1, 2, 3]
	print   // Pop & print Stack: [1, 2]     Output: 3
	print   // Pop & print Stack: [1]        Output: 2
	print   // Pop & print Stack: []         Output: 1
}
```

## Visualizing the Stack

Throughout this documentation, we show stacks like `[1, 2, 3]` where **3 is on top** (rightmost).

```
Stack: [1, 2, 3]

    3  <-- top
    2
    1  <-- bottom
```

## Operations Consume and Produce

Every operation has a **stack effect**:

- How many values it **consumes** (pops)
- How many values it **produces** (pushes)

Examples:

| Operation | Consumes | Produces | Effect |
|-----------|----------|----------|--------|
| `42` | 0 | 1 | Pushes 42 |
| `print` | 1 | 0 | Pops and prints |
| `+` | 2 | 1 | Pops two, pushes sum |
| `dup` | 1 | 2 | Copies top value |
| `drop` | 1 | 0 | Discards top value |

## Stack Effects in Signatures

Function signatures declare stack effects with `( inputs -- outputs )`:

```qd
fn add(a:i64 b:i64 -- sum:i64) {
	+
}
```

This means:

- **Before**: Stack has `[..., a, b]`
- **After**: Stack has `[..., sum]`

The `--` separates inputs from outputs.

## Why Stack-Based?

Benefits of stack-based programming:

1. **Explicit data flow** - You always know where data comes from
2. **No hidden state** - Function signatures tell you everything
3. **Composability** - Functions chain naturally
4. **Simple mental model** - Just push and pop

## A Complete Example

Let's trace through a calculation:

```qd
fn main( -- ) {
	2 3 + 4 * print nl  // (2 + 3) * 4 = 20
}
```

Step by step:

| Code | Action | Stack |
|------|--------|-------|
| `2` | Push 2 | [2] |
| `3` | Push 3 | [2, 3] |
| `+` | Pop 2,3; Push 5 | [5] |
| `4` | Push 4 | [5, 4] |
| `*` | Pop 5,4; Push 20 | [20] |
| `print` | Pop and print 20 | [] |
| `nl` | Print newline | [] |

## What's Next?

Now let's learn [Stack Manipulation](manipulation.md) - operations to rearrange values on the stack.
