# Getting Started

This guide will help you install Quadrate and write your first program.

## Prerequisites

Quadrate requires the following build tools:

- **Meson** (build system)
- **Ninja** (build backend)
- **Clang/LLVM** (compiler toolchain)

### Installing Dependencies

#### Arch Linux
```bash
sudo pacman -S meson clang ninja llvm
```

#### Ubuntu/Debian
```bash
sudo apt install meson ninja-build clang llvm-dev
```

#### Fedora
```bash
sudo dnf install meson ninja-build clang llvm-devel
```

#### macOS (with Homebrew)
```bash
brew install meson ninja llvm
```

## Installation

### From Source

Clone the repository and build:

```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make release
sudo make install
```

This installs:

- Binaries to `/usr/bin/`
- Libraries to `/usr/lib/`
- Standard library to `/usr/share/quadrate/`

### Verifying Installation

```bash
quadc --version
```

## Your First Program

Create a file called `hello.qd`:

```qd
fn main( -- ) {
    "Hello, World!" print nl
}
```

Compile and run it:

```bash
quadc -r hello.qd
```

Output:
```
Hello, World!
```

### What Just Happened?

- `fn main( -- )` declares a function called `main` with no inputs and no outputs
- `"Hello, World!"` pushes a string onto the stack
- `print` pops the string and prints it
- `nl` prints a newline

## The Toolchain

Quadrate comes with a complete set of tools:

| Tool | Description |
|------|-------------|
| `quadc` | Compiler - compiles `.qd` files to native binaries |
| `quadfmt` | Formatter - formats code to standard style |
| `quadlint` | Linter - static analysis for common issues |
| `quadlsp` | Language server - IDE integration |
| `quadrepl` | REPL - interactive shell for experimentation |
| `quadpm` | Package manager - dependency management |
| `quaduses` | Analyzer - shows function usage across modules |

### Compiler Options

```bash
# Compile to binary
quadc hello.qd -o hello

# Compile and run immediately
quadc -r hello.qd

# Show verbose output
quadc --verbose hello.qd

# Dump LLVM IR (for debugging)
quadc --dump-ir hello.qd
```

### Using the Formatter

```bash
# Check formatting (dry run)
quadfmt hello.qd

# Format in place
quadfmt -w hello.qd
```

### Using the Linter

```bash
quadlint hello.qd
```

## Editor Support

### Neovim

Tree-sitter grammar and LSP support are available. See the `editors/nvim/` directory for installation instructions.

## Next Steps

- [Hello World Tutorial](tutorial-hello-world.md) - A deeper look at your first program
- [Stack Tutorial](tutorial-stack.md) - Understanding stack-based programming
- [Structs Tutorial](tutorial-structs.md) - Working with structured data
- [Standard Library](stdlib/index.md) - Available modules and functions

