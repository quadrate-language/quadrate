# Switch Statements

Handle multiple conditions cleanly with `switch`.

## Basic Switch

```qd
fn day_name(day:i64 -- name:str) {
	-> day
	day switch {
		1 => { "Monday" }
		2 => { "Tuesday" }
		3 => { "Wednesday" }
		4 => { "Thursday" }
		5 => { "Friday" }
		6 => { "Saturday" }
		7 => { "Sunday" }
		_ => { "Unknown" }
	}
}

fn main( -- ) {
	3 day_name print nl  // Wednesday
}
```

## The Default Case

Use `_` for the default (fallback) case:

```qd
fn describe(n:i64 -- desc:str) {
	-> n
	n switch {
		0 => { "zero" }
		1 => { "one" }
		_ => { "many" }
	}
}

fn main( -- ) {
	0 describe print nl  // zero
	1 describe print nl  // one
	5 describe print nl  // many
}
```

## Multiple Values Per Case

```qd
fn is_vowel(c:i64 -- result:i64) {
	-> c
	c switch {
		'a' => { 1 }
		'e' => { 1 }
		'i' => { 1 }
		'o' => { 1 }
		'u' => { 1 }
		_ => { 0 }
	}
}
```

## Switch with Side Effects

Cases can contain statements:

```qd
fn handle_command(cmd:i64 -- ) {
	-> cmd
	cmd switch {
		1 => {
			"Starting..." print nl
			// do_start
		}
		2 => {
			"Stopping..." print nl
			// do_stop
		}
		3 => {
			"Status: OK" print nl
		}
		_ => {
			"Unknown command" print nl
		}
	}
}
```

## Switch on Expressions

Switch on computed values:

```qd
fn classify_score(score:i64 -- grade:str) {
	-> score
	score 10 / switch {
		10 => { "A+" }
		9 => { "A" }
		8 => { "B" }
		7 => { "C" }
		6 => { "D" }
		_ => { "F" }
	}
}

fn main( -- ) {
	95 classify_score print nl  // A
	73 classify_score print nl  // C
	45 classify_score print nl  // F
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
		0 => { "black" }
		1 => { "red" }
		2 => { "green" }
		3 => { "blue" }
		_ => { "unknown" }
	}
}
```

### Equivalent if-else (more verbose)

```qd
fn get_color_if(code:i64 -- name:str) {
	-> code
	code 0 == if { "black" }
	else { code 1 == if { "red" }
	else { code 2 == if { "green" }
	else { code 3 == if { "blue" }
	else { "unknown" } } } }
}
```

## Stack Balance

All cases must have the same stack effect:

```qd
// CORRECT: All cases push one value
fn to_string(n:i64 -- s:str) {
	-> n
	n switch {
		1 => { "one" }
		2 => { "two" }
		_ => { "other" }
	}
}

// INCORRECT: Mismatched stack effects (would not compile)
// fn bad(n:i64 -- ???) {
//     -> n
//     n switch {
//         1 => { "one" }       // pushes 1
//         2 => { "a" "b" }     // pushes 2 - ERROR!
//         _ => { }             // pushes 0 - ERROR!
//     }
// }
```

## Common Patterns

### Menu Handler

```qd
fn handle_menu(choice:i64 -- ) {
	-> choice
	choice switch {
		1 => { "Creating new file..." print nl }
		2 => { "Opening file..." print nl }
		3 => { "Saving file..." print nl }
		4 => { "Exiting..." print nl }
		_ => { "Invalid choice. Try again." print nl }
	}
}
```

### State Machine

```qd
fn next_state(current:i64 input:i64 -- next:i64) {
	-> input -> current
	current switch {
		0 => {
			input 'a' == if { 1 } else { 0 }
		}
		1 => {
			input 'b' == if { 2 } else { 0 }
		}
		2 => {
			input 'c' == if { 3 } else { 0 }
		}
		_ => { 0 }
	}
}
```

## What's Next?

Now let's explore [Data Structures](../5-data-structures/arrays.md) - arrays, structs, and more.
