# Quadrate

A stack-based programming language that compiles to native code.

**Documentation**: https://quad.r8.rs

## What is Quadrate?

Quadrate is a Forth-inspired stack language with modern features: static typing, modules, and native compilation via LLVM. Values live on an explicit stack, and functions manipulate that stack directly.

## Quick Start

Install:
```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make && sudo make install
```

Hello World (`hello.qd`):
```rust
fn main() {
    "Hello, World!" .  nl
}
```

Compile and run:
```bash
quadc hello.qd -r
```

## Examples

### Simple Math
```rust
fn main() {
    5 3 +       // Push 5, push 3, add them
    . nl        // Print result (8) and newline
}
```

### Function with Stack Signature
```rust
fn square( x:float -- result:float ) {
    dup *       // Duplicate x, multiply
}

fn main() {
    7.0 square . nl    // Prints 49.0
}
```

### Control Flow
```rust
fn main() {
    10 5 > if {
        "10 is greater" . nl
    } else {
        "5 is greater" . nl
    }
}
```

### Loops
```rust
fn main() {
    0 10 1 for {
        $ . nl    // $ is the loop counter
    }
}
```

## Core Concepts

**Stack Operations**: `dup` (duplicate), `swap`, `drop`, `over`, `rot`

**Arithmetic**: `+ - * / %` `sqrt` `abs` `inc` `dec`

**Comparison**: `< > <= >= == !=`

**Control Flow**: `if`/`else`, `for` loops, `switch`/`case`

**Functions**: Can have stack signatures showing inputs and outputs
```rust
fn add( a:float b:float -- sum:float ) {
    +
}
```

## Standard Library

Import with `use`:
```rust
use fmt

fn main() {
    "Hello %s!\n" "World" fmt::printf
}
```

Available modules:
- **fmt** - Formatted printing
- **net** - TCP networking
- **time** - Sleep and timing

## Build Commands

```bash
make           # Build debug version
make release   # Build optimized version
make tests     # Run all tests
make examples  # Build example programs
```

## Tools

- **quadc** - Compiler (`-r` to run, `-o name` for output)
- **quadfmt** - Code formatter
- **quadlsp** - Language server for IDE integration

## Installation

Default install to `/usr`:
```bash
sudo make install
```

Custom prefix:
```bash
sudo make install PREFIX=/usr/local
```

Uninstall:
```bash
sudo make uninstall
```

## Dependencies

- Meson build system
- libu8t (UTF-8 tokenizer)
- LLVM (for code generation)

## License

This repository contains source code generated with assistance from an AI model (Anthropic Claude).
It may include or resemble material that is licensed under the GNU General Public License (GPL), version 3 or later.

Therefore, all files in this repository are distributed **under the GPL v3**.

The distributor makes **no claim of authorship or copyright ownership** of the generated content.
This repository is provided solely for research and demonstration purposes.

See the [`LICENSE`](./LICENSE) file for full terms.

## Maintainers

[~klahr](https://sr.ht/~klahr)

## Contributing

We enthusiastically encourage and welcome contributions.

### Reporting Issues

Report bugs and feature requests at: https://todo.sr.ht/~klahr/quadrate

### Submitting Patches

Send patches to: ~klahr/quadrate@lists.sr.ht

Learn more about sourcehut workflows: https://man.sr.ht/git.sr.ht/send-email.md
