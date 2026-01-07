# Operators

Operators work on values from the stack.

## Arithmetic

All arithmetic operators take two values and produce one result:

```qd
fn main() {
	3 4 + print nl   // 7
	10 3 - print nl  // 7
	6 7 * print nl   // 42
	20 4 / print nl  // 5
	17 5 % print nl  // 2 (modulo/remainder)
}
```

### How it works

```qd
3 4 +
```

1. Push `3` onto the stack
2. Push `4` onto the stack
3. `+` pops both, adds them, pushes `7`

Stack visualization:

```
[3]      -- after "3"
[3, 4]   -- after "4"
[7]      -- after "+"
```

### Word forms

You can also use words instead of symbols:

| Symbol | Word |
|--------|------|
| `+` | `add` |
| `-` | `sub` |
| `*` | `mul` |
| `/` | `div` |
| `%` | `mod` |

```qd
fn main() {
	3 4 add print nl  // 7
	10 3 sub print nl // 7
}
```

### Other arithmetic

```qd
fn main() {
	5 neg print nl  // -5 (negate)
	5 ++ print nl   // 6 (increment)
	5 -- print nl   // 4 (decrement)
}
```

You can also use `inc` and `dec` as word forms for `++` and `--`.

## Comparison

Comparisons return `1` (true) or `0` (false):

```qd
fn main() {
	5 3 > print nl   // 1 (5 > 3 is true)
	5 3 < print nl   // 0 (5 < 3 is false)
	5 5 == print nl  // 1 (5 == 5 is true)
	5 3 != print nl  // 1 (5 != 3 is true)
	5 5 >= print nl  // 1
	5 5 <= print nl  // 1
}
```

### Word forms

| Symbol | Word |
|--------|------|
| `==` | `eq` |
| `!=` | `neq` |
| `<` | `lt` |
| `<=` | `lte` |
| `>` | `gt` |
| `>=` | `gte` |

### Range check

`within` checks if a value is in a range [low, high):

```qd
fn main() {
	5 0 10 within print nl  // 1 (5 is in [0,10))
	10 0 10 within print nl // 0 (10 is not in [0,10))
}
```

## Logical

Logical operators work on boolean values (0 and non-zero):

```qd
fn main() {
	true true and print nl   // 1
	true false and print nl  // 0
	true false or print nl   // 1
	false false or print nl  // 0
}
```

For logical negation, use comparison with zero:

```qd
fn main() {
	true 0 == print nl   // 0 (logical NOT true)
	false 0 == print nl  // 1 (logical NOT false)
}
```

## Bitwise

For bit manipulation:

```qd
fn main() {
	0b1100 0b1010 and print nl  // 8 (0b1000)
	0b1100 0b1010 or print nl   // 14 (0b1110)
	0b1100 0b1010 xor print nl  // 6 (0b0110)
	0b1111 not print nl         // -16 (bitwise NOT)
	1 4 << print nl             // 16 (shift left)
	16 2 >> print nl            // 4 (shift right)
}
```

You can also use `shl` and `shr` as word forms for `<<` and `>>`.

**Note:** `and`, `or`, and `not` are bitwise operators. For logical operations on boolean values (0/1), `and` and `or` work correctly. For logical NOT, use `0 ==` instead.

## Combining operations

Build complex expressions by chaining:

```qd
fn main() {
	// (3 + 4) * 2
	3 4 + 2 * print nl  // 14

	// 10 - (2 * 3)
	10 2 3 * - print nl  // 4
}
```

The order you write operations is the order they execute. No operator precedence to remember!

## What's next?

You've learned the basics! Now let's dive deeper into [How the Stack Works](../2-stack/how-it-works.md).
