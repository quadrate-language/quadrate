# Example: user input

Reading input from the user and command-line arguments.

## Reading from stdin

Use `io::readline` to read a line of input:

```qd
use io

fn main() {
	"Enter your name: " print
	io::readline! -> name
	"Hello, " print name print "!" print nl
}
```

`io::readline` is a fallible function that reads one line from stdin (without the trailing newline). Use `!` to abort on error, or handle errors explicitly:

```qd
use io

fn main() {
	"Enter your name: " print
	io::readline switch {
		Ok {
			-> name
			"Hello, " print name print "!" print nl
		}
		_ {
			"Could not read input" print nl
		}
	}
}
```

## Reading numbers

`io::readline` returns a string. Use `cast` to convert to other types:

```qd
use io

fn main() {
	"Enter a number: " print
	io::readline! cast<i64> -> n
	"Double: " print n 2 * print nl
}
```

## Interactive loop

Read input repeatedly until EOF (Ctrl+D):

```qd
use io

fn main() {
	loop {
		"> " print
		io::readline switch {
			Ok {
				-> line
				"You said: " print line print nl
			}
			_ {
				nl
				break
			}
		}
	}
	"Goodbye!" print nl
}
```

## Command-line arguments

Use the `read` builtin to access command-line arguments:

```qd
fn main() {
	read -> argc

	argc 0 == if {
		"No arguments provided" print nl
	} else {
		"Arguments:" print nl
		argc 0 > if {
			-> arg1
			"  1: " print arg1 print nl
		}
		argc 1 > if {
			-> arg2
			"  2: " print arg2 print nl
		}
	}
}
```

Run with: `quad run args.qd -- hello world`

The `read` builtin pushes all arguments onto the stack with the count on top.

## Simple calculator

A complete example combining input and parsing:

```qd
use io
use strings

fn main() {
	loop {
		"calc> " print
		io::readline switch {
			Ok {
				-> line
				line strings::len 0 == if {
					continue
				}
				line cast<f64> -> num
				"= " print num print nl
			}
			_ {
				nl break
			}
		}
	}
}
```

## What's next?

Learn about [Writing Tests](testing.md) to ensure your code works correctly.
