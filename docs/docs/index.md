# Quadrate

A programming language where data flows through your code like water through pipes.

> **Quadrate is under active development. Language syntax and standard library interfaces may change in future releases.**

```qd
fn main() {
	"Hello, World!" print nl
}
```

Quadrate is a **concatenative, stack-based language** that compiles to native code. If you've ever wondered what programming would feel like without variables cluttering every line, you're in the right place.

## What Makes Quadrate Different?

In most languages, you write `result = add(a, b)`. In Quadrate, you write:

```qd
a b +
```

Values flow onto a stack, operations consume them and produce new values. It's simple, explicit, and surprisingly powerful.

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

## Why Quadrate?

**See the data flow.** Function signatures tell you exactly what goes in and what comes out. No hidden state, no surprises.

**Catch errors early.** The compiler validates stack effects and types before your code runs. If it compiles, the stack is balanced.

**Run fast.** LLVM compilation means your code runs at native speed, not in an interpreter.

**Handle errors honestly.** Fallible functions force you to deal with errors. No exceptions flying across your codebase.

## Get Started

New to Quadrate? Start here:

1. **[Install Quadrate](docs/getting-started.md)** - Get the toolchain running
2. **[Hello World](docs/learn/1-basics/hello-world.md)** - Write your first program
3. **[Learn the Stack](docs/learn/2-stack/how-it-works.md)** - Understand the core concept

Or dive into the **[Learn section](docs/learn/1-basics/hello-world.md)** for a complete tutorial from basics to advanced topics.

## The Toolchain

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

## A Taste of the Language

Here's a slightly more involved example - reading command line arguments:

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

## Learn More

- **[Standard Library](docs/stdlib/index.md)** - Strings, I/O, math, networking, and more
- **[Language Reference](docs/reference/index.md)** - All keywords and built-in operations
- **[About Quadrate](about.md)** - Philosophy, history, and links

## License

Quadrate is free software under the [GPL-3.0 License](https://www.gnu.org/licenses/gpl-3.0.html).
