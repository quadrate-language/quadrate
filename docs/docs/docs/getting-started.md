# Getting Started

This guide will help you install Quadrate and write your first program.

## Prerequisites

Quadrate requires the following build tools:

- **Meson** (build system)
- **Ninja** (build backend)
- **Clang/LLVM** (compiler toolchain)

### Installing Dependencies

> **Note:** Arch Linux, Debian and Haiku are currently the only tested and supported platforms. Other platforms may work but are untested.

#### Arch Linux
```bash
pacman -S meson clang ninja llvm readline
```

#### Debian
```bash
apt install meson clang libreadline-dev
```

#### Haiku
```bash
pkgman install meson llvm21 llvm21_clang readline_devel
```

## Installation

### From Source

Clone the repository and build:

```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make release
make install
```

This installs:

#### Default Locations
- Binaries to `/usr/bin/`
- Libraries to `/usr/lib/`
- Standard library to `/usr/share/quadrate/`

#### Haiku Specific Locations
- Binaries to `/boot/home/config/non-packaged/bin/`
- Libraries to `/boot/home/config/non-packaged/lib/`
- Standard library to `/boot/home/config/non-packaged/data/quadrate/`

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

Tree-sitter grammar and LSP support: [quadrate.nvim](https://git.sr.ht/~klahr/quadrate.nvim)

### Visual Studio Code / Code - OSS

Syntax highlighting and LSP support: [quadrate-vscode](https://git.sr.ht/~klahr/quadrate-vscode)

## Next Steps

Continue learning Quadrate:

1. [Hello World Tutorial](tutorial-hello-world.md) - Understanding your first program
2. [Stack Tutorial](tutorial-stack.md) - The core concepts of stack-based programming
3. [Structs Tutorial](tutorial-structs.md) - Working with structured data
4. [Error Handling](tutorial-errors.md) - Handling errors properly
5. [Modules Tutorial](tutorial-modules.md) - Creating reusable modules

Reference:

- [Standard Library](stdlib/index.md) - Available modules and functions
- [Toolchain](toolchain.md) - All available tools

