# Switch Statements

Handle multiple conditions cleanly with `switch`.

## Basic Switch

```qd
fn day_name(day:i64 -- name:str) {
	-> day
	day switch {
		1 {
			"Monday"
		}
		2 {
			"Tuesday"
		}
		3 {
			"Wednesday"
		}
		4 {
			"Thursday"
		}
		5 {
			"Friday"
		}
		6 {
			"Saturday"
		}
		7 {
			"Sunday"
		}
		_ {
			"Unknown"
		}
	}
}

fn main() {
	3 day_name print nl  // Wednesday
}
```

## The Default Case

Use `_` for the default (fallback) case:

```qd
fn describe(n:i64 -- desc:str) {
	-> n
	n switch {
		0 {
			"zero"
		}
		1 {
			"one"
		}
		_ {
			"many"
		}
	}
}

fn main() {
	0 describe print nl  // zero
	1 describe print nl  // one
	5 describe print nl  // many
}
```

## When to Use Switch vs If-Else

Use **switch** when:

- Comparing one value against many constants
- Each case is mutually exclusive
- You want cleaner, more readable code

Use **if-else** when:

- Conditions are complex expressions
- You need range comparisons
- Cases aren't mutually exclusive

### Switch (cleaner)

```qd
fn get_color(code:i64 -- name:str) {
	-> code
	code switch {
		0 {
			"black"
		}
		1 {
			"red"
		}
		2 {
			"green"
		}
		3 {
			"blue"
		}
		_ {
			"unknown"
		}
	}
}
```

### Equivalent if-else (more verbose)

```qd
fn get_color_if(code:i64 -- name:str) {
	-> code
	code 0 == if {
		"black"
	} else {
		code 1 == if {
			"red"
		} else {
			code 2 == if {
				"green"
			} else {
				code 3 == if {
					"blue"
				} else {
					"unknown"
				}
			}
		}
	}
}
```

## Common Patterns

### Menu Handler

```qd
fn handle_menu(choice:i64 -- ) {
	-> choice
	choice switch {
		1 {
			"Creating new file..." print nl
		}
		2 {
			"Opening file..." print nl
		}
		3 {
			"Saving file..." print nl
		}
		4 {
			"Exiting..." print nl
		}
		_ {
			"Invalid choice. Try again." print nl
		}
	}
}
```

### State Machine

```qd
fn next_state(current:i64 input:i64 -- next:i64) {
	-> input -> current
	current switch {
		0 {
			input 'a' == if {
				1
			} else {
				0
			}
		}
		1 {
			input 'b' == if {
				2
			} else {
				0
			}
		}
		2 {
			input 'c' == if {
				3
			} else {
				0
			}
		}
		_ {
			0
		}
	}
}
```

## What's Next?

Now let's explore [Data Structures](../5-data-structures/arrays.md) - arrays, structs, and more.
