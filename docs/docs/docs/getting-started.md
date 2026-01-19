# Getting started

This guide will help you install Quadrate and write your first program.

## Prerequisites

Quadrate requires the following build tools:

- **Meson** (build system)
- **Ninja** (build backend)
- **Clang/LLVM** (compiler toolchain)

### Installing dependencies

> **Note:** [Arch Linux](https://archlinux.org/), [Debian](https://www.debian.org/) and [Haiku](https://haiku-os.org/) are currently the only tested and supported platforms. Other platforms may work but are untested.

<img src="https://archlinux.org/static/logos/archlinux-logo-dark-scalable.svg" alt="https://archlinux.org/" style="height: 40px">

```bash
pacman -S meson clang ninja llvm readline
```

#### <img src="https://www.debian.org/logos/openlogo-nd.svg" alt="https://www.debian.org/" style="height: 32px"> Debian
```bash
apt install meson clang libreadline-dev
```

<img src="https://raw.githubusercontent.com/haiku/haiku/b31ff5b650da52911640cd5514a08887732f3342/data/artwork/HAIKU%20logo%20-%20black.svg" alt="https://www.haiku-os.org/" style="height: 32px">

```bash
pkgman install meson llvm21 llvm21_clang readline_devel
```

## Installation

### From source

Clone the repository and build:

```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make release
make install
```

This installs:

#### Default locations
- Binaries to `/usr/bin/`
- Libraries to `/usr/lib/`
- Standard library to `/usr/share/quadrate/`

#### Haiku specific locations
- Binaries to `/boot/home/config/non-packaged/bin/`
- Libraries to `/boot/home/config/non-packaged/lib/`
- Standard library to `/boot/home/config/non-packaged/data/quadrate/`

### Verifying installation

```bash
quad version
```

To verify everything works, try running a quick test:

```bash
echo 'fn main() { "Hello!" print nl }' | quad run -
```

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

## Editor support

### Neovim

Tree-sitter grammar and LSP support: [quadrate.nvim](https://git.sr.ht/~klahr/quadrate.nvim)

### Visual Studio Code / Code - OSS

Syntax highlighting and LSP support: [quadrate-vscode](https://git.sr.ht/~klahr/quadrate-vscode)

## Next steps

Now that Quadrate is installed, continue to [Hello World](learn/1-basics/hello-world.md) to write your first program.

The learn guide covers:

- [The Stack](learn/2-stack/how-it-works.md) - Core concepts of stack-based programming
- [Functions](learn/3-functions/defining.md) - Defining and calling functions
- [Structs](learn/5-data-structures/structs.md) - Working with structured data
- [Error Handling](learn/6-error-handling/basics.md) - Handling errors properly
- [Modules](learn/3-functions/modules.md) - Creating reusable modules

Reference:

- [Standard Library](stdlib/index.md) - Available modules and functions
- [Toolchain](toolchain.md) - All available tools

