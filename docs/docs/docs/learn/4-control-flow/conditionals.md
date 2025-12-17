# Conditionals

Control the flow of your program with `if` and `else`.

## The if Statement

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
	10 -> x

	x 5 > if {
		"x is greater than 5" print nl
	} else {
		"x is 5 or less" print nl
	}
}
```

## Nested Conditionals

```qd
fn classify(n:i64 -- ) {
	-> n
	n 0 < if {
		"negative" print nl
	} else {
		n 0 == if {
			"zero" print nl
		} else {
			"positive" print nl
		}
	}
}

fn main() {
	-5 classify  // negative
	0 classify   // zero
	7 classify   // positive
}
```

## Conditional Expressions

`if` can leave values on the stack:

```qd
fn abs(x:i64 -- result:i64) {
	-> x
	x 0 < if {
		x neg
	} else {
		x
	}
}

fn main() {
	-5 abs print nl  // 5
	7 abs print nl   // 7
}
```

Both branches must leave the same number of values on the stack.

## Combining Conditions

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
	has_id 0 == if {
		"No ID" print nl
	}
}
```

## Comparison Operators

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

## Common Patterns

### Guard Clauses

```qd
fn process(x:i64 -- result:i64) {
	-> x
	x 0 < if {
		0
		return // Early return for invalid input
	}
	x dup *    // Normal processing
}
```

### Default Values

```qd
fn get_or_default(value:i64 default:i64 -- result:i64) {
	-> default -> value
	value 0 == if {
		default
	} else {
		value
	}
}
```

### Range Checking

```qd
fn is_valid_age(age:i64 -- valid:i64) {
	-> age
	age 0 >= age 150 <= and
}

fn main() {
	25 is_valid_age print nl   // 1 (true)
	-5 is_valid_age print nl   // 0 (false)
	200 is_valid_age print nl  // 0 (false)
}
```

## What's Next?

Learn about [Loops](loops.md) to repeat operations.
