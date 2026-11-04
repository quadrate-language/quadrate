# Example: user input

Reading input from the user and command-line arguments.

## Reading from stdin

Use `io::readline` to read a line of input:

<!-- doccheck: compile-only reads from stdin -->
```qd
use io

fn main() {
	"Enter your name: " print
	io::readline! drop
	"Hello, " print print "!" print nl
}
```

`io::readline` is a fallible function that reads one line from stdin (without the trailing newline). It has two outputs: the line, and on top of it `more`, which is `1` when a line was read and `0` at the end of input. Running out of input is not an error; `drop` above throws `more` away. Use `!` to abort on error, or handle errors explicitly:

```qd
use io

fn main() {
	"Enter your name: " print
	io::readline switch {
		Ok {
			drop
			"Hello, " print print "!" print nl
		}
		_ {
			drop drop
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
	io::readline! drop
	strconv::atoi ok if {
		"Double: " print 2 * print nl
	} else {
		drop
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
				0 == if {
					drop nl
					break
				}
				"You said: " print print nl
			}
			_ {
				drop drop
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
				0 == if {
					drop nl break
				}
				dup len 0 == if {
					drop continue
				}
				strconv::parse_float ok if {
					"= " print print nl
				} else {
					drop
					"not a number" print nl
				}
			}
			_ {
				drop drop
				nl break
			}
		}
	}
}
```

## What's next?

Learn about [Writing Tests](testing.md) to ensure your code works correctly.
