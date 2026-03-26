# Switch statements

Handle multiple conditions cleanly with `switch`.

## Basic switch

```qd
fn day_name(day:i64 -- name:str) {
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

## The default case

Use `_` for the default (fallback) case:

```qd
fn describe(n:i64 -- desc:str) {
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

## When to use switch vs If-Else

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
	-> code  // bind parameter
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
	-> code  // bind parameter
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

## Common patterns

### Menu handler

```qd
fn handle_menu(choice:i64 -- ) {
	-> choice  // bind parameter
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

### State machine

```qd
fn next_state(current:i64 input:i64 -- next:i64) {
	-> input -> current
	current switch {
		0 {
			input 97 == if { 1 } else { 0 }  // 'a'
		}
		1 {
			input 98 == if { 2 } else { 0 }  // 'b'
		}
		2 {
			input 99 == if { 3 } else { 0 }  // 'c'
		}
		_ {
			0
		}
	}
}
```

## What's next?

Now let's explore [Data Structures](../5-data-structures/arrays.md) - arrays, structs, and more.
