# Logic and bits

`and`, `or` and `not` are logical: every non-zero value counts as true, and the result is
always `0` or `1`. The bitwise operations of the same names are in the
[`bits`](../stdlib/bits.md) module, where `bits::` says which meaning is meant, and the shifts
are there with them.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `and` | `(a b -- flag)` | Logical AND: 1 if both are non-zero |
| `or` | `(a b -- flag)` | Logical OR: 1 if either is non-zero |
| `not` | `(a -- flag)` | Logical NOT: 1 if the value is zero |
| `bits::and` | `(a b -- result)` | Bitwise AND |
| `bits::or` | `(a b -- result)` | Bitwise OR |
| `bits::xor` | `(a b -- result)` | Bitwise XOR |
| `bits::not` | `(a -- result)` | Bitwise NOT (ones' complement) |
| `bits::shl` | `(a n -- result)` | Shift left |
| `bits::shr` | `(a n -- result)` | Shift right (logical) |

Both operands of `and` and `or` are always evaluated: they are ordinary stack words, not
short-circuiting forms.

---

## Logical operations

### and

1 if both values are non-zero, 0 otherwise.

**Signature:** `(a b -- flag)`

```qd
fn main() {
	1 1 and print nl  // 1
	2 1 and print nl  // 1 (bits::and would give 0)
	1 0 and print nl  // 0
}
```

### or

1 if either value is non-zero, 0 otherwise.

**Signature:** `(a b -- flag)`

```qd
fn main() {
	0 0 or print nl  // 0
	2 1 or print nl  // 1 (bits::or would give 3)
}
```

### not

1 if the value is zero, 0 otherwise.

**Signature:** `(a -- flag)`

```qd
fn main() {
	0 not print nl  // 1
	5 not print nl  // 0
}
```

---

## Shift operations

### bits::shl

Shifts a left by n bits.

**Signature:** `(a n -- result)`

```qd
use bits

fn main() {
	1 4 bits::shl print nl  // 16
}
```

### bits::shr

Shifts a right by n bits. The shift is logical: the value is treated as unsigned and zeros
are shifted in, so a negative number becomes a large positive one.

**Signature:** `(a n -- result)`

```qd
use bits

fn main() {
	16 2 bits::shr print nl  // 4
	-16 2 bits::shr print nl  // 4611686018427387900
}
```

The count `n` must be 0 to 63; any other count is a fatal error.

---

## Bit flags

The bitwise operations come from the `bits` module.

```qd
use bits

const FlagRead = 1
const FlagWrite = 2
const FlagExec = 4

fn main() {
	// Set flags
	FlagRead FlagWrite bits::or

	// Check a flag
	dup FlagRead bits::and 0 != if {
		"Has read permission" print nl
	}

	// Clear a flag
	FlagWrite bits::not bits::and

	// Toggle a flag
	FlagExec bits::xor
	print nl  // 5
}
```
