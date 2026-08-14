# Walkthrough: building a desk calculator

The `dc` example in `examples/dc/` is a reverse-polish notation calculator
modeled on the Unix `dc` tool. It's a uniquely fitting example for a
stack-based language: `dc` itself is a stack machine, so we're building
a stack machine inside a stack machine.

This walkthrough explains how it works, section by section. Reading the full
source in `examples/dc/dc.qd` alongside this page is recommended.

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

## The stack

The program's fundamental data structure is a `Stack` of f64 values:

```qd
pub struct Stack {
    data:     ptr
    size:     i64
    capacity: i64
}
```

- `data` is a raw memory buffer
- `size` is how many values are currently stored
- `capacity` is how many the buffer can hold

The buffer is allocated once with a fixed capacity (1024 slots) and never
grows. If it fills up, we print an error and drop the push.

## Allocation and basic operations

`stack_new` allocates the buffer and constructs the struct:

```qd
fn stack_new( -- s:ptr) {
    1024 -> cap
    cap 8 * mem::alloc! -> data
    Stack {
        data = data
        size = 0
        capacity = cap
    }
}
```

Read left to right:

1. Bind `1024` to the local `cap`
2. Allocate `cap * 8` bytes (each f64 is 8 bytes) into `data`
3. Construct a `Stack` with those fields

`stack_push` writes a value at the current top, then increments `size`:

```qd
fn stack_push(s:ptr val:f64 -- ) {
    s <<size -> sz
    s <<capacity -> cap
    sz cap >= if {
        "dc: stack overflow" print nl
    } else {
        val s <<data sz 8 * mem::set_f64
        s sz 1 + >>size!
    }
}
```

Key syntax:

- `s <<size` reads the `size` field from struct `s`
- `s sz 1 + >>size!` sets `size` to `sz + 1`. The `!` suffix means "consume
  the struct after the assignment" — we don't need it back on the stack.
- `val s <<data sz 8 * mem::set_f64` writes `val` at offset `sz * 8` in `data`

`stack_pop` is the mirror image — decrement `size`, read the value:

```qd
fn stack_pop(s:ptr -- val:f64) {
    s <<size -> sz
    sz 0 <= if {
        "dc: stack empty" print nl
        0.0
    } else {
        sz 1 - -> sz
        s sz >>size!
        s <<data sz 8 * mem::get_f64
    }
}
```

## Parsing a token

`is_number` decides whether a token is a number or an operator. dc uses
underscore `_` as the negative sign (since `-` is subtraction):

```qd
fn is_number(tok:str -- result:i64) {
    0 -> result
    tok strings::len 0 > if {
        tok 0 strings::char_at! -> first
        first unicode::is_digit if {
            1 -> result
        } else {
            first unicode::underscore == first unicode::minus == or if {
                tok strings::len 1 > if {
                    tok 1 strings::char_at! -> second
                    second unicode::is_digit -> result
                }
            }
        }
    }
    result
}
```

The pattern is:

- If first char is a digit, it's a number
- If first char is `_` or `-` and second char is a digit, it's a negative
  number
- Otherwise, it's an operator

`process_token` dispatches based on this:

```qd
fn process_token(s:ptr tok:str -- ) {
    tok is_number -> is_num
    is_num if {
        tok 0 strings::char_at! -> first
        first unicode::underscore == if {
            // dc convention: `_5` means -5
            tok 1 tok strings::len 1 - strings::substring! cast<f64> -> val
            s 0.0 val - stack_push
        } else {
            tok cast<f64> -> val
            s val stack_push
        }
    }
    is_num 0 == if {
        tok strings::len 0 > if {
            s tok process_operator
        }
    }
}
```

Notice how numbers with an `_` prefix are handled by extracting the digits
after the underscore and negating.

## The operator table

`process_operator` is where the actual calculator lives. It's a big `switch`
on the first character of the operator token:

```qd
fn process_operator(s:ptr op:str -- ) {
    op 0 strings::char_at! switch {
        unicode::plus {
            s 2 stack_check if {
                s stack_pop -> b
                s stack_pop -> a
                s a b + stack_push
            }
        }
        unicode::minus {
            // ...
        }
        // ... many more cases
    }
}
```

Each case:

1. Checks the stack has enough operands (`stack_check`)
2. Pops operands — note the order: **b pops first** because it was pushed
   last, so `a b` reflects the source order `a` then `b`
3. Performs the operation
4. Pushes the result

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

### Edge cases

Division and remainder check for zero denominators:

```qd
unicode::slash {
    s 2 stack_check if {
        s stack_peek "dc: divide by zero" check_nonzero if {
            s stack_pop -> b
            s stack_pop -> a
            s a b / stack_push
        }
    }
}
```

`stack_peek` reads the top without popping — that way we can check the
divisor before committing to pop both operands. `check_nonzero` prints an
error and returns 0 if the value is zero; the `if` only proceeds on a
non-zero check.

### Printing the whole stack (`f`)

```qd
unicode::f {
    s stack_depth -> n
    0 n 1 for it {
        s it stack_pick print nl
    }
}
```

`stack_pick(s, n)` returns the value at position `n` from the top. Iterating
`it` from 0 to `n-1` walks the stack from top to bottom.

## Processing a line

`process_line` tokenizes an input line and processes each token:

```qd
fn process_line(s:ptr input:str -- ) {
    // Strip anything after `#` (comments)
    input "#" strings::index_of -> comment_pos
    comment_pos 0 >= if {
        input 0 comment_pos strings::substring! -> input
    }
    input strings::trim -> trimmed

    loop {
        trimmed strings::len 0 <= if { break }
        trimmed " " strings::index_of -> space_pos
        space_pos 0 < if {
            s trimmed process_token
            break
        }
        trimmed 0 space_pos strings::substring! -> token
        token strings::len 0 > if {
            s token process_token
        }
        trimmed space_pos 1 + trimmed strings::len space_pos - 1 - strings::substring! -> trimmed
    }
}
```

The loop repeatedly:

1. Finds the next space
2. Extracts the token before it
3. Processes it
4. Advances past the space

If there's no space, the remaining string is the last token.

## The REPL

The interactive loop just reads lines from stdin until EOF:

```qd
fn repl(s:ptr -- ) {
    loop {
        io::readline switch {
            Ok { -> input  s input process_line  input drop }
            _  { 0 os::exit }
        }
    }
}
```

`io::readline` returns a `Result`. `Ok` binds the line and processes it; any
error (including EOF) exits.

## Main and flags

`main` parses command-line flags with the `flag` module:

<!-- doccheck: skip excerpt of examples/dc/dc.qd; helpers live in the full file -->
```qd
fn main() {
    stack_new -> s
    defer { s stack_free }

    read flag::parse -> f
    defer { f flag::destroy }

    // ... check each flag
}
```

Two `defer` blocks ensure the stack's buffer and the flag parser are freed
when `main` exits, even if an earlier branch calls `os::exit`.

The flag logic is straightforward:

- `-h` / `--help` → print help and exit
- `-V` / `--version` → print version and exit
- `-e EXPR` / `--expression EXPR` → evaluate EXPR and exit
- `-f FILE` / `--file FILE` → evaluate FILE and exit
- No flags → start the REPL

## What this example teaches

- **Struct definition with fields** and the `<<field` / `>>field!` access
  syntax
- **Raw memory operations** via the `mem` module for implementing data
  structures
- **Dispatch on character** using `switch` and `unicode::` constants
- **Named parameters** as locals — the `s` and `tok` parameters are referenced
  by name inside the function, not pulled off the stack
- **`defer`** for cleanup that runs no matter how the function exits
- **`io::readline switch`** for result-based error handling
- **Stack-based thinking** at its purest: the program IS a stack machine,
  and the implementation mirrors that directly

This is a complete, working CLI tool in about 470 lines. The stack-based
style made the operator dispatch especially natural — in most languages, each
operator's implementation would be wrapped in function-call syntax; here,
operators look like operators.
