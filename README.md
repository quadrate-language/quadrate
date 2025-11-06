# Quadrate

A stack-based programming language that compiles to C using GCC as a backend.

## Table of Contents
- [Description](#description)
- [Features](#features)
- [Dependencies](#dependencies)
- [Installation](#installation)
- [Usage](#usage)
- [Library Architecture](#library-architecture)
- [Standard Library](#standard-library)
- [Examples](#examples)
- [Development](#development)
- [License](#license)
- [Maintainers](#maintainers)
- [Contributing](#contributing)

## Description

Quadrate is a stack-based programming language inspired by Forth. It produces optimized native executables with a clean syntax, strong type checking, module system, and comprehensive standard library.

For detailed documentation, visit https://quad.r8.rs

## Features

- **Stack-based execution** with type-tagged stack elements (int, float, string, pointer)
- **Native compilation** for maximum performance and portability
- **Type checking** with multi-pass semantic validation
- **Module system** with imports and namespacing
- **Standard library** with networking, formatting, time, and threading
- **Built-in functions** for math, stack manipulation, and control flow
- **UTF-8 string support** via external tokenizer
- **LSP server** for IDE integration
- **Code formatter** for consistent style
- **Multithreading** with pthread-based thread spawning

## Dependencies

- **Meson** build system (used internally via Makefile)
- **libu8t** - UTF-8 tokenization library (system-installed)
- **libunit-check** - Unit testing framework (for building tests)
- **Python 3** (for LSP tests)

### Install Dependencies

**Arch Linux:**
```bash
sudo pacman -S meson
# Install libu8t from AUR or build from source
```

**Debian/Ubuntu:**
```bash
sudo apt install meson
# Install libu8t from source
```

## Installation

```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make && sudo make install
```

This installs:
- Binaries to `/usr/bin/` (quadc, quadfmt, quadlsp)
- Libraries to `/usr/lib/` (libqdrt, libqd, libstdqd)
- Headers to `/usr/include/`
- Standard library to `/usr/share/quadrate/`

### Custom Installation Prefix

```bash
sudo make install PREFIX=/usr/local
```

### Uninstall

```bash
sudo make uninstall
```

## Usage

### Hello World

Create `hello.qd`:
```rust
fn main() {
    "Hello, World!" print nl
}
```

Compile and run:
```bash
quadc -o hello hello.qd
./hello
```

### Using Standard Library

```rust
use fmt

fn main() {
    "Hello %s! Answer: %d\n" "World" 42 fmt::printf
}
```

### Run Without Saving Binary

```bash
quadc --run hello.qd
```

## Library Architecture

Quadrate consists of three main libraries:

### libqdrt (Quadrate Runtime)
- **Files**: `libqdrt.so`, `libqdrt_static.a`
- **Headers**: `<qdrt/runtime.h>`, `<qdrt/context.h>`, `<qdrt/stack.h>`
- **Purpose**: Low-level runtime with stack operations, built-in instructions, context management
- **Used by**: Compiled Quadrate programs, libqd, libstdqd

### libqd (Quadrate Embedding API)
- **Files**: `libqd.so`, `libqd_static.a`
- **Headers**: `<qd/qd.h>`
- **Purpose**: High-level API for embedding Quadrate in C programs
- **Depends on**: libqdrt

### libstdqd (Standard Library)
- **Files**: `libstdqd.so`, `libstdqd_static.a`
- **Headers**: `<stdqd/fmt.h>`, `<stdqd/net.h>`, `<stdqd/time.h>`
- **Purpose**: Networking, formatting, time, and file I/O utilities
- **Depends on**: libqdrt

### Linking

When embedding Quadrate or using the runtime from other languages:
- **Runtime library**: `-lqdrt`
- **Embedding API**: `-lqd`

## Standard Library

The standard library is installed to `/usr/share/quadrate/` and includes:

- **fmt**: Formatted printing and string operations
- **net**: Networking (TCP sockets, listen, connect, send, receive)
- **time**: Sleep functions with nanosecond precision and duration constants

### Module Search Order

The compiler searches for modules in this order:
1. Local path (relative to source file)
2. `$QUADRATE_ROOT` environment variable
3. `$HOME/quadrate/`
4. `/usr/share/quadrate/` (system-wide installation)

### Override Standard Library Location

```bash
export QUADRATE_ROOT=~/custom-quadrate
```

## Examples

### Hello World

```rust
fn main() {
    "Hello, World!" print nl
}
```

### Fibonacci

```rust
fn fib(n:i -- result:i) {
    dup 2 gte if {
        // Recursive case: fib(n-1) + fib(n-2)
        dup 1 - fib
        swap 2 - fib
        +
    }
    // Base case: n < 2, result = n (already on stack)
}

fn main() {
    0 20 1 for {
        $ dup fib print nl
    }
}
```

### Threading with Sleep

```rust
use time

fn alpha() {
    0 5 1 for {
        "Alpha working..." print nl
        500 time::Millisecond * time::sleep
    }
}

fn bravo() {
    0 5 1 for {
        "Bravo working..." print nl
        500 time::Millisecond * time::sleep
    }
}

fn main() {
    &alpha spawn
    &bravo spawn

    wait
    wait

    "All done!" print nl
}
```

### More Examples

See the `examples/` directory:
- `hello-world/` - Simple hello world in Quadrate
- `hello-world-c/` - Embedding example
- `embed/` - Full embedding example
- `bmi/` - BMI calculator
- `web-server/` - Simple TCP web server
- `threading/` - Multithreading example

Build examples:
```bash
make examples
./dist/examples/hello-world
```

## Development

### Build Commands

All development tasks use the Makefile:

```bash
# Debug build (default)
make

# Release build
make release

# Run all tests
make tests

# Run tests with valgrind
make valgrind

# Build examples
make examples

# Format code
make format

# Clean all build artifacts
make clean
```

### Project Structure

```
quadrate/
├── bin/               # Executable tools
│   ├── quadc/        # Compiler
│   ├── quadfmt/      # Formatter
│   └── quadlsp/      # LSP server
├── lib/              # Libraries
│   ├── qc/           # Compiler frontend (parser, validator)
│   ├── cgen/         # Code generator (transpiler)
│   ├── qdrt/         # Runtime library
│   ├── qd/           # Embedding API
│   └── stdqd/        # Standard library (native + Quadrate modules)
├── tests/            # Test suites
├── examples/         # Example programs
└── editors/          # Editor integration (tree-sitter, nvim)
```

### Compilation Pipeline

1. Parse `.qd` files → AST (using external u8t tokenizer)
2. Multi-pass semantic validation
3. Transpile AST → native code
4. Link with runtime library

### Build System

Quadrate uses Meson as the underlying build system, but all interactions should be through the Makefile for consistency. The Makefile provides convenient targets that wrap Meson commands.

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
