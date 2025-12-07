# Quadrate

Quadrate is a stack-based programming language that compiles to native code via LLVM.

## Features

- **Stack-based** - Simple, explicit data flow with stack effects
- **Native compilation** - Fast execution through LLVM code generation
- **Explicit error handling** - Fallible functions with mandatory error checks
- **Automatic memory management** - Reference-counted structs and arrays
- **C interoperability** - Easy FFI to call C libraries
- **Complete toolchain** - Compiler, formatter, linter, LSP, REPL, and package manager

## Quick Example

```qd
use math

struct Point {
    x:f64
    y:f64
}

fn distance(p:ptr -- d:f64) {
    -> p
    p @x dup *
    p @y dup * +
    math::sqrt
}

fn main( -- ) {
    Point {
        x = 3.0
        y = 4.0
    } -> p
    "Distance: " print p distance print nl  // 5
}
```

## Documentation

- [Getting Started](docs/getting-started.md) - Installation and setup
- [Hello World](docs/tutorial-hello-world.md) - Your first program
- [Stack Tutorial](docs/tutorial-stack.md) - Understanding stack-based programming
- [Structs Tutorial](docs/tutorial-structs.md) - Working with structured data
- [Standard Library](docs/stdlib/index.md) - Available modules and functions

## The Toolchain

| Tool | Description |
|------|-------------|
| `quad`| The main command-line interface |
| `quadc` | Compiler |
| `quadfmt` | Code formatter |
| `quadlint` | Linter |
| `quadlsp` | Language server |
| `quadmcp` | Model Context Protocol server |
| `quadpm` | Package manager |
| `quadrepl` | Interactive REPL |
| `quaduses` | Manage imported packages automatically |

## License

GPL-3.0 License

