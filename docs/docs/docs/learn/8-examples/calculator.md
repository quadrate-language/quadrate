# Example: Calculator

A simple stack-based calculator demonstrating core concepts.

## The Code

```qd
use io
use str

fn main( -- ) {
	"Simple Calculator" io::println
	"Enter: number number operator" io::println
	"Operators: + - * /" io::println
	"Type 'quit' to exit" io::println
	io::println

	loop {
		"> " io::print

		io::readline if {
			-> line

			line "quit" str::eq if {
				"Goodbye!" io::println
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

		parts @len 3 != if {
			"Error: Need 'num num op'" io::println
		} else {
			parts 0 @[] parse_number if {
				-> a
				parts 1 @[] parse_number if {
					-> b
					parts 2 @[] -> op

					a b op calculate if {
						-> result
						"Result: " io::print
						result io::println
					} else {
						drop
						"Error: Unknown operator" io::println
					}
				} else {
					drop
					"Error: Invalid second number" io::println
				}
			} else {
				drop
				"Error: Invalid first number" io::println
			}
		}
	} else {
		drop
		"Error: Could not parse input" io::println
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
	result h @entries h @count ![]
	h @count 1 + h !count
}

fn history_show(h:ptr -- ) {
	-> h
	"History:" io::println
	0 h @count for i {
		i print ": " print
		h @entries i @[] io::println
	}
}
```

## What's Next?

See more examples:
- [File Processing](file-processing.md)
- [Data Structures](data-structures.md)
