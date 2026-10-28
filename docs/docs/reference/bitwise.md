# Logic and bits

`and`, `or` and `not` are logical: every non-zero value counts as true, and the result is
always `0` or `1`. The bitwise operations of the same names are in the
[`bits`](../stdlib/bits.md) module, where `bits::` says which meaning is meant. The shifts
have no logical counterpart and stay builtins.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `and` | `(a b -- result)` | Logical AND: 1 if both are non-zero |
| `or` | `(a b -- result)` | Logical OR: 1 if either is non-zero |
| `not` | `(a -- result)` | Logical NOT: 1 if the value is zero |
| `shl` | `(a n -- result)` | Shift left |
| `shr` | `(a n -- result)` | Shift right (logical) |
| `bits::and` | `(a b -- result)` | Bitwise AND |
| `bits::or` | `(a b -- result)` | Bitwise OR |
| `bits::xor` | `(a b -- result)` | Bitwise XOR |
| `bits::not` | `(a -- result)` | Bitwise NOT (ones' complement) |

Both operands of `and` and `or` are always evaluated: they are ordinary stack words, not
short-circuiting forms.

---

## Logical operations

### and

1 if both values are non-zero, 0 otherwise.

**Signature:** `(a b -- result)`

```qd
fn main() {
	1 1 and print nl  // 1
	2 1 and print nl  // 1 (bits::and would give 0)
	1 0 and print nl  // 0
}
```

### or

1 if either value is non-zero, 0 otherwise.

**Signature:** `(a b -- result)`

```qd
fn main() {
	0 0 or print nl  // 0
	2 1 or print nl  // 1 (bits::or would give 3)
}
```

### not

1 if the value is zero, 0 otherwise.

**Signature:** `(a -- result)`

```qd
fn main() {
	0 not print nl  // 1
	5 not print nl  // 0
}
```

---

## Shift operations

### shl

Shifts a left by n bits.

**Signature:** `(a n -- result)`

```qd
fn main() {
	1 4 shl print nl  // 16
}
```

### shr

Shifts a right by n bits. The shift is logical: the value is treated as unsigned and zeros
are shifted in, so a negative number becomes a large positive one.

**Signature:** `(a n -- result)`

```qd
fn main() {
	16 2 shr print nl  // 4
	-16 2 shr print nl  // 4611686018427387900
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
	FlagRead FlagWrite bits::or -> permissions

	// Check a flag
	permissions FlagRead bits::and 0 != if {
		"Has read permission" print nl
	}

	// Clear a flag
	permissions FlagWrite bits::not bits::and -> permissions

	// Toggle a flag
	permissions FlagExec bits::xor -> permissions
	permissions print nl  // 5
}
```
