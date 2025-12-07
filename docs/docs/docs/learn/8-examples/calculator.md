# Example: Calculator

A simple stack-based calculator demonstrating core concepts.

## The Code

```qd
use io
use str

fn main( -- ) {
	"Simple Calculator" print nl
	"Enter: number number operator" print nl
	"Operators: + - * /" print nl
	"Type 'quit' to exit" print nl
	nl

	loop {
		"> " print

		io::readline if {
			-> line

			line "quit" str::eq if {
				"Goodbye!" print nl
				break
			}

			line process_line
		} else {
			drop
			break
		}
	}
}

fn process_line(line:str -- ) {
	-> line

	// Parse: "5 3 +"
	line " " str::split if {
		-> parts

		parts len 3 != if {
			"Error: Need 'num num op'" print nl
		} else {
			parts 0 nth parse_number if {
				-> a
				parts 1 nth parse_number if {
					-> b
					parts 2 nth -> op

					a b op calculate if {
						-> result
						"Result: " print
						result print nl
					} else {
						drop
						"Error: Unknown operator" print nl
					}
				} else {
					drop
					"Error: Invalid second number" print nl
				}
			} else {
				drop
				"Error: Invalid first number" print nl
			}
		}
	} else {
		drop
		"Error: Could not parse input" print nl
	}
}

fn parse_number(s:str -- n:i64)! {
	-> s
	s str::to_i64 if {
		// Success
	} else {
		drop 0
		"not a number" 1 error
	}
}

fn calculate(a:i64 b:i64 op:str -- result:i64)! {
	-> op -> b -> a

	op "+" str::eq if {
		a b +
	} else {
		op "-" str::eq if {
			a b -
		} else {
			op "*" str::eq if {
				a b *
			} else {
				op "/" str::eq if {
					b 0 == if {
						0
						"division by zero" 1 error
					}
					a b /
				} else {
					0
					"unknown operator" 1 error
				}
			}
		}
	}
}
```

## Key Concepts

### Stack-Based Input

The calculator uses postfix notation (Reverse Polish Notation):
- `5 3 +` means add 5 and 3
- Natural for stack-based languages

### Error Handling

Multiple fallible operations:
- Input parsing can fail
- Number conversion can fail
- Division by zero is handled

### Modular Functions

Each function has a single responsibility:
- `main` - Main loop
- `process_line` - Parse and dispatch
- `parse_number` - Convert string to number
- `calculate` - Perform operation

## Running It

```
$ quad run calculator.qd
Simple Calculator
Enter: number number operator
Operators: + - * /
Type 'quit' to exit

> 10 5 +
Result: 15
> 20 4 *
Result: 80
> 100 0 /
Error: Unknown operator
> quit
Goodbye!
```

## Extending the Calculator

### Add More Operators

```qd
fn calculate(a:i64 b:i64 op:str -- result:i64)! {
	-> op -> b -> a

	op "%" str::eq if { a b % } else {
	op "^" str::eq if { a b power } else {
	// ... existing operators ...
	} }
}

fn power(base:i64 exp:i64 -- result:i64) {
	-> exp -> base
	1 -> result
	0 exp for i {
		result base * -> result
	}
	result
}
```

### Add History

```qd
struct History {
	entries:ptr
	count:i64
}

fn history_add(h:ptr result:i64 -- ) {
	-> result -> h
	h @entries h @count result set
	h @count 1 + h !count
}

fn history_show(h:ptr -- ) {
	-> h
	"History:" print nl
	0 h @count 1 for i {
		i print ": " print
		h @entries i nth print nl
	}
}
```

## What's Next?

See more examples:
- [File Processing](file-processing.md)
- [Data Structures](data-structures.md)
