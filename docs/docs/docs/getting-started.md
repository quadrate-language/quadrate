# Getting Started

This guide will help you install Quadrate and write your first program.

## Prerequisites

Quadrate requires the following build tools:

- **Meson** (build system)
- **Ninja** (build backend)
- **Clang/LLVM** (compiler toolchain)

### Installing Dependencies

> **Note:** Arch Linux is currently the only tested and supported platform. Other platforms may work but are untested.

#### Arch Linux
```bash
sudo pacman -S meson clang ninja llvm readline
```

#### Ubuntu/Debian
```bash
sudo apt install meson ninja-build clang llvm-dev libreadline-dev
```

#### Fedora
```bash
sudo dnf install meson ninja-build clang llvm-devel readline-devel
```

#### FreeBSD
```bash
pkg install meson ninja llvm readline
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
quad version
```

## Your First Program

Create a file called `hello.qd`:

```qd
fn main() {
    "Hello, World!" print nl
}
```

Compile and run it:

```bash
quad run hello.qd
```

Output:
```
Hello, World!
```

### What Just Happened?

- `fn main()` declares a function called `main` with no inputs and no outputs
- `"Hello, World!"` pushes a string onto the stack
- `print` pops the string and prints it
- `nl` prints a newline

## Using quad

The `quad` command is your main interface to Quadrate. It provides a unified way to build, run, test, and format your code:

```bash
quad run hello.qd      # Compile and run
quad build hello.qd    # Compile to binary
quad fmt hello.qd      # Format code
quad lint hello.qd     # Check for issues
quad test              # Run tests
quad repl              # Start interactive shell
```

For most tasks, `quad` is all you need. It automatically finds your source files and handles the details.

See [Toolchain](toolchain.md) for documentation on all available tools including the compiler (`quadc`), formatter (`quadfmt`), linter (`quadlint`), and more.

## Editor Support

### Neovim

Tree-sitter grammar and LSP support are available: https://git.sr.ht/~klahr/quadrate.nvim

### Code - OSS / Visual Studio Code

Syntax highlighting and LSP support are available: https://git.sr.ht/~klahr/quadrate-vscode

## Next Steps

- [Hello World Tutorial](tutorial-hello-world.md) - A deeper look at your first program
- [Stack Tutorial](tutorial-stack.md) - Understanding stack-based programming
- [Structs Tutorial](tutorial-structs.md) - Working with structured data
- [Standard Library](stdlib/index.md) - Available modules and functions

