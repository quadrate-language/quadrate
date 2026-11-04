# Switch statements

Handle multiple conditions cleanly with `switch`.

## Basic switch

```qd
fn day_name(day:i64 -- name:str) {
	switch {
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
	switch {
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
	switch {
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
	dup 0 == if {
		drop "black"
	} else {
		dup 1 == if {
			drop "red"
		} else {
			dup 2 == if {
				drop "green"
			} else {
				dup 3 == if {
					drop "blue"
				} else {
					drop "unknown"
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
	switch {
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
	swap switch {
		0 {
			97 == if { 1 } else { 0 }  // 'a'
		}
		1 {
			98 == if { 2 } else { 0 }  // 'b'
		}
		2 {
			99 == if { 3 } else { 0 }  // 'c'
		}
		_ {
			drop 0
		}
	}
}
```

## What's next?

Now let's explore [Data Structures](../5-data-structures/arrays.md) - arrays, structs, and more.
