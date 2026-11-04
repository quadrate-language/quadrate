# Conditionals

Control the flow of your program with `if` and `else`.

## The if statement

Execute code when a condition is true:

```qd
fn main() {
	5 3 > if {
		"5 is greater than 3" print nl
	}
}
```

The `if` pops a value from the stack. If non-zero (true), it executes the block.

## if-else

Handle both cases:

```qd
fn main() {
	10 dup 5 > if {
		print " is greater than 5" print nl
	} else {
		print " is 5 or less" print nl
	}
}
```

## Conditional expressions

`if` works on the values already on the stack:

```qd
fn abs(x:i64 -- result:i64) {
	dup 0 < if {
		neg
	}
}

fn main() {
	-5 abs print nl  // 5
	7 abs print nl   // 7
}
```

Both branches must leave the same number of values on the stack, and an `if` without `else`
must leave as many as it found.

## Combining conditions

Use logical operators:

```qd
fn main() {
	18 -> age
	1 -> has_id

	// AND: both must be true
	age 18 >= has_id and if {
		"Can enter" print nl
	}

	// OR: either can be true
	age 21 >= age 18 < or if {
		"Special case" print nl
	}

	// NOT: invert condition
	has_id not if {
		"No ID" print nl
	}
}
```

## Comparison operators

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `<=` | Less or equal |
| `>` | Greater than |
| `>=` | Greater or equal |

```qd
fn main() {
	5 5 == if { 
		"equal" print nl
	}
	5 3 != if {
		"not equal" print nl
	}
	3 5 < if {
		"less than" print nl
	}
}
```

## Common patterns

### Guard clauses

Handle edge cases first with `if`/`else`:

```qd
fn process(x:i64 -- result:i64) {
	dup 0 < if {
		drop 0         // Return 0 for invalid input
	} else {
		dup *          // Normal processing
	}
}
```

### Default values

```qd
fn get_or_default(value:i64 default:i64 -- result:i64) {
	over 0 == if {
		nip
	} else {
		drop
	}
}
```

### Range checking

```qd
fn is_valid_age(age:i64 -- valid:i64) {
	dup 0 >= swap 150 <= and
}

fn main() {
	25 is_valid_age print nl   // 1 (true)
	-5 is_valid_age print nl   // 0 (false)
	200 is_valid_age print nl  // 0 (false)
}
```

## What's next?

Learn about [Loops](loops.md) to repeat operations, or [Switch](switch.md) for multi-way branching.
