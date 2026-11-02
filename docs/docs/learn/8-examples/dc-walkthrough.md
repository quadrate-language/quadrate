# Walkthrough: building a desk calculator

The `dc` example in `examples/dc/` is a reverse-polish notation calculator
modeled on the Unix `dc` tool. It's a uniquely fitting example for a
stack-based language: `dc` itself is a stack machine, so we're building
a stack machine inside a stack machine.

It is also written the way stack code is meant to be written: no named
parameters, almost no locals, and words of a line or two that each do one
thing. This walkthrough explains how it works, section by section. Reading the
full source in `examples/dc/dc.qd` alongside this page is recommended.

## What dc does

`dc` reads tokens from its input. Numbers are pushed onto a stack; letters
are operators that pop operands and push results.

```
$ dc
3 4 + p        # 3+4, print: 7
5 d * p        # 5 squared (d duplicates): 25
16 v p         # square root: 4
q              # quit
```

With `-e`, you can pass an expression directly:

```
$ dc -e "2 3 * 4 + p"
10
```

## The calculator's stack

The calculator keeps its own stack of `f64` values, separate from Quadrate's:
a fixed array of cells, and a count of how many are in use. Both are globals,
because every operator works on them. A global cannot hold an array itself, so
it holds a struct that does:

<!-- doccheck: skip excerpt from examples/dc/dc.qd, shown piece by piece; the whole program is built by the example suite -->
```qd
struct Cells {
	at: []f64 = [1024]f64
}

var cells = Cells {}
var depth:i64 = 0
```

`[1024]f64` is an array of 1024 zeros. It never grows: a push onto a full
stack prints an error and is dropped.

## Push, pop and friends

Every word that touches the stack is a line:

<!-- doccheck: skip excerpt from examples/dc/dc.qd, shown piece by piece; the whole program is built by the example suite -->
```qd
fn push(v:f64 -- ) { depth Capacity >= if { drop "dc: stack overflow" print nl } else { cells <<at depth rot set depth 1 + -> depth } }
fn pop( -- v:f64) { depth 0 <= if { "dc: stack empty" print nl 0.0 } else { depth 1 - -> depth cells <<at depth nth } }
fn top( -- v:f64) { cells <<at depth 1 - nth }
fn operands( -- a:f64 b:f64) { pop pop swap }
```

Read `push` from the left, watching Quadrate's stack:

1. `v` is on the stack when `push` starts. A `fn` leaves its inputs there;
   the `v:f64` in the signature only documents it.
2. `cells <<at` reads the array out of the struct: `v arr`.
3. `depth` pushes the index of the first free cell: `v arr i`.
4. `rot` brings `v` to the top: `arr i v`, which is what `set` takes.
5. `depth 1 + -> depth` stores the new count. `->` into a global is an
   assignment, like Forth's `!`.

`operands` pops the two values an operator works on. `b` comes off first,
because it was pushed last, so `swap` puts them back in source order: `a b`.

Before an operator pops anything, it checks there is something to pop:

<!-- doccheck: skip excerpt from examples/dc/dc.qd, shown piece by piece; the whole program is built by the example suite -->
```qd
fn enough(n:i64 -- ok:i64) { depth swap >= dup not if { "dc: stack empty" print nl } }
fn nonzero(msg:str -- ok:i64) { top 0.0 != dup if { nip } else { swap print nl } }
```

`enough` leaves a flag, and says why when it is false. `nonzero` looks at the
top value without popping it, so a division can refuse a zero divisor before
it has taken anything off the stack.

## The operator table

The calculator proper is one `switch` on the first character of an operator.
Each arm is a line, and leaves `1` to say the operator is known:

<!-- doccheck: skip excerpt from examples/dc/dc.qd, shown piece by piece; the whole program is built by the example suite -->
```qd
fn known(c:i64 -- yes:i64) {
	switch {
		unicode::plus { 2 enough if { operands + push } 1 }
		unicode::slash { 2 enough if { "dc: divide by zero" nonzero if { operands / push } } 1 }
		unicode::v { 1 enough if { "dc: square root of negative number" nonneg if { pop math::sqrt push } } 1 }
		unicode::p { 1 enough if { top print nl } 1 }
		unicode::r { 2 enough if { pop pop swap push push } 1 }
		// ... and the rest
		_ { 0 }
	}
}
```

`2 enough if { operands + push }` reads as what it does: if there are two
values, take them, add them, push the sum. Anything longer than a line gets a
word of its own:

<!-- doccheck: skip excerpt from examples/dc/dc.qd, shown piece by piece; the whole program is built by the example suite -->
```qd
fn modulo(a:f64 b:f64 -- r:f64) { dup2 / math::floor * - }
fn divmod(a:f64 b:f64 -- ) { dup2 / math::floor dup push * - push }
```

`modulo` is `a - floor(a / b) * b`: `dup2` keeps `a b` for later, `/` and
`math::floor` make the quotient, `*` multiplies it by `b`, and `-` takes that
from `a`. `divmod` is the same computation, pushing the quotient on the way.

The operators implemented:

| Op | Description |
|----|-------------|
| `+`, `-`, `*`, `/` | Arithmetic |
| `%` | Remainder (float modulo) |
| `^` | Exponentiation |
| `v` | Square root |
| `~` | Divmod (pushes both quotient and remainder) |
| `p` | Print top without popping |
| `n` | Pop and print |
| `f` | Print the entire stack |
| `d` | Duplicate top |
| `r` | Reverse (swap) top two |
| `c` | Clear stack |
| `z` | Push stack depth |
| `q` | Quit |

An operator `known` has no arm for is reported, with its character in octal as
GNU `dc` does:

<!-- doccheck: skip excerpt from examples/dc/dc.qd, shown piece by piece; the whole program is built by the example suite -->
```qd
fn operator(op:str -- ) { dup 0 strings::char_at known if { drop } else { unimplemented } }
```

## Parsing a token

A token is a number when it starts with a digit, or with `_` or `-` and then a
digit. dc writes negative numbers with `_`, since `-` is subtraction:

<!-- doccheck: skip excerpt from examples/dc/dc.qd, shown piece by piece; the whole program is built by the example suite -->
```qd
fn digit_at(tok:str i:i64 -- yes:i64) { over len over > if { strings::char_at unicode::is_digit } else { drop2 0 } }

fn is_number(tok:str -- yes:i64) {
	dup 0 digit_at if { drop 1 } else { dup len 1 > if { dup signed swap 1 digit_at and } else { drop 0 } }
}

fn token(tok:str -- ) { dup is_number if { number } else { dup len 0 > if { operator } else { drop } } }
```

`digit_at` asks whether there is a digit at position `i`, and answers `0`
rather than reading past the end. `token` sends a number to `number`, which
parses and pushes it, and anything else to `operator`.

## Processing a line

A line is a comment-stripped, trimmed string of tokens separated by spaces:

<!-- doccheck: skip excerpt from examples/dc/dc.qd, shown piece by piece; the whole program is built by the example suite -->
```qd
fn uncomment(input:str -- input:str) { dup "#" strings::index_of dup 0 >= if { 0 swap strings::substring! } else { drop } }
fn line(input:str -- ) { uncomment strings::trim " " strings::split! fn { token } each }
```

`strings::split!` makes an array of the tokens, and `each` runs `token` on
every one. Two spaces in a row make an empty token, which `token` drops. A file
is a list of lines: `run_file` reads it whole and runs `line` on each one of
`strings::lines!`.

## The REPL

The interactive loop reads lines from stdin until the end of input:

<!-- doccheck: skip excerpt from examples/dc/dc.qd, shown piece by piece; the whole program is built by the example suite -->
```qd
fn repl() {
	loop {
		io::readline switch {
			Ok { if { line } else { drop 0 os::exit } }
			_ { drop2 1 os::exit }
		}
	}
}
```

`io::readline` leaves a line and a `more` flag. In the `Ok` arm the flag is on
top, so `if` tests it directly: while there is more, the line is run; at the
end of input the empty line is dropped and the program exits cleanly. The `_`
arm is a genuine read failure, and exits non-zero.

## Main and flags

<!-- doccheck: skip excerpt of examples/dc/dc.qd; helpers live in the full file -->
```qd
fn main() {
	os::args flag::parse
	dup "-h" "--help" flag::either if { help 0 os::exit }
	dup "-V" "--version" flag::either if { version 0 os::exit }
	dup "-e" flag::boolean if { dup "-e" flag::string! line 0 os::exit }
	// ... --expression, -f and --file the same way
	flag::destroy
	repl
}
```

The parsed flags stay on the stack while `main` asks about them, each question
starting with a `dup` so the flags are still there for the next one.

- `-h` / `--help` → print help and exit
- `-V` / `--version` → print version and exit
- `-e EXPR` / `--expression EXPR` → evaluate EXPR and exit
- `-f FILE` / `--file FILE` → evaluate FILE and exit
- No flags → start the REPL

## What this example teaches

- **Words of a line**: almost every function is one line, and reads as what
  it does, `2 enough if { operands + push }`
- **Inputs on the stack**: a `fn` leaves its inputs there, and the names in
  its signature are documentation
- **Globals for state every word shares**, assigned with `->` as Forth's `!`
  stores
- **An array behind a struct**, for a global that needs one
- **Dispatch on character** using `switch` and `unicode::` constants
- **Combinators over data**: `each` over the tokens of a line and the lines of
  a file, `times` to print the stack
- **Stack-based thinking** at its purest: the program IS a stack machine,
  and the implementation mirrors that directly

This is a complete, working CLI tool in 140 lines.
