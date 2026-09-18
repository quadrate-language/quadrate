# Example: user input

Reading input from the user and command-line arguments.

## Reading from stdin

Use `io::readline` to read a line of input:

<!-- doccheck: compile-only reads from stdin -->
```qd
use io

fn main() {
	"Enter your name: " print
	io::readline! -> _ -> name
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
			-> _ -> name
			"Hello, " print name print "!" print nl
		}
		_ {
			"Could not read input" print nl
		}
	}
}
```

## Reading numbers

`io::readline` returns a string. Converting it to a number can fail -- whatever the user
typed is not necessarily a number -- so use `strconv`, which reports that:

<!-- doccheck: compile-only reads from stdin -->
```qd
use io
use strconv

fn main() {
	"Enter a number: " print
	io::readline! -> _ -> line
	line strconv::atoi if {
		-> n
		"Double: " print n 2 * print nl
	} else {
		"That is not a number." print nl
	}
}
```

`cast<i64>` will not do this: `cast` only performs conversions that always succeed, and
parsing is not one of them. Reading input is exactly the case where the difference matters.

## Interactive loop

Read input repeatedly until EOF (Ctrl+D):

```qd
use io

fn main() {
	loop {
		"> " print
		io::readline switch {
			Ok {
				-> ok -> line
				ok 0 == if {
					nl
					break
				}
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

Use `os::args` to access command-line arguments. It returns them as an array,
excluding the program name:

```qd
use os

fn main() {
	os::args -> args
	args len -> argc

	argc 0 == if {
		"No arguments provided" print nl
	} else {
		"Arguments:" print nl
		0 argc 1 for i {
			"  " print i 1 + print ": " print args i nth print nl
		}
	}
	args free
}
```

Run with: `quad run args.qd -- hello world`

The `read` builtin pushes all arguments onto the stack with the count on top.

## Simple calculator

A complete example combining input and parsing:

```qd
use io
use strconv
use strings

fn main() {
	loop {
		"calc> " print
		io::readline switch {
			Ok {
				-> ok -> line
				ok 0 == if {
					nl break
				}
				line strings::len 0 == if {
					continue
				}
				line strconv::parse_float if {
					-> num
					"= " print num print nl
				} else {
					"not a number" print nl
				}
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
