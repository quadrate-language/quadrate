# Quadrate

A stack-based concatenative programming language with static typing and LLVM compilation.

> **Quadrate is under active development. Language syntax and standard library interfaces may change in future releases.**

```qd
fn main() {
	"Hello, World!" print nl
}
```

Quadrate is a concatenative language where data flows through a stack. Operations consume values from the stack and produce new values. Function signatures explicitly declare stack effects, enabling compile-time validation.

## Core concepts

In Quadrate, you write operations in postfix notation:

```qd
a b +
```

Values are pushed onto a stack, then operations consume them. The compiler validates all stack operations at compile time.

```qd
fn factorial(n:i64 -- result:i64) {
	dup 1 <= if {
		drop
		1
	} else {
		dup -- factorial *
	}
}

fn main() {
	5 factorial print nl  // 120
}
```
```bash
quad run factorial.qd
```

## Key features

**Explicit stack effects.** Function signatures declare what they consume and produce: `(inputs -- outputs)`. The compiler validates all stack operations.

**Static type checking.** Stack types are validated at compile time. Type mismatches are caught before execution.

**Native code generation.** LLVM backend produces optimized native binaries. No interpreter or VM overhead.

**Structured error handling.** Fallible functions are marked with `!` and must be explicitly handled at call sites. No exceptions.

## Get started

New to Quadrate? Start here:

1. **[Try it Online](https://quad.r8.rs/play/)** - Experiment in the playground
2. **[Install Quadrate](getting-started.md)** - Get the toolchain running
3. **[Hello World](learn/1-basics/hello-world.md)** - Write your first program
4. **[Learn the Stack](learn/2-stack/how-it-works.md)** - Understand stack-based evaluation

Or dive into the **[Learn section](learn/1-basics/hello-world.md)** for a complete tutorial from basics to advanced topics.

## The toolchain

Quadrate comes with everything you need:

| Tool | What it does |
|------|--------------|
| `quad` | Main CLI - run, build, test, format in one command |
| `quadc` | Compiler - turns `.qd` files into executables |
| `quadfmt` | Formatter - keeps your code consistent |
| `quadlint` | Linter - catches common mistakes |
| `quadlsp` | Language server - IDE integration |
| `quadrepl` | REPL - experiment interactively |
| `quadpm` | Package manager - manage dependencies |

## Command-line arguments

Reading command-line arguments:

```qd
use str

fn main() {
	read -> argc

	argc 0 == if {
		"Usage: greet <name>" print nl
	} else {
		-> name
		"Hello, " name str::concat "!" str::concat print nl
	}
}
```
```bash
quad run greet.qd -- Millie
```

## Learn more

- **[Standard Library](stdlib/index.md)** - Strings, I/O, math, threading, and more
- **[Language Reference](reference/index.md)** - All keywords and built-in operations
- **[About Quadrate](about.md)** - Design philosophy and links

## License

Quadrate is free software under the [GPL-3.0 License](https://www.gnu.org/licenses/gpl-3.0.html).
