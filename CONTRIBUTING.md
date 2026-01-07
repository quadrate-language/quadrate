# Contributing to Quadrate

Thank you for your interest in contributing to Quadrate! We welcome contributions of all kinds, including bug reports, feature requests, documentation improvements, and code contributions.

## Building the project

### Prerequisites
- Meson 0.55 or higher
- C++20 compatible compiler (GCC 10+, Clang 10+)
- Make
- u8t library (tokenizer/scanner - must be installed system-wide)
- clang-format (for code formatting)
- valgrind (optional, for memory leak testing)

### Build commands

```bash
# Debug build (with sanitizers enabled)
make debug

# Release build
make release

# Build examples
make examples

# Run tests
make tests

# Format code
make format

# Clean build artifacts
make clean
```

Build outputs are placed in the `dist/` directory:
- `dist/bin/` - Executables (quadc, quadfmt) - built from `cmd/`
- `dist/lib/` - Shared and static libraries (libquadrate.so, libquadrate_static.a)
- `dist/include/` - Public headers

Intermediate build files are in `build/debug/` or `build/release/`.

## Running tests

```bash
# Run all tests
make tests

# Run tests with valgrind (memory leak detection)
make valgrind
```

Tests are located in `lib/*/tests/` directories and use Meson's built-in test framework.

## Code style

Quadrate follows a consistent code style enforced by `.clang-format`:

- **Indentation**: Tabs (width: 4)
- **Brace style**: Attached (opening brace on same line)
- **Naming conventions**:
  - Classes/Namespaces: PascalCase (`Ast`, `Compiler`)
  - Functions/Variables: camelCase (`generate`, `tokenText`)
  - Constants: UPPER_SNAKE_CASE
- **Line length**: 120 characters maximum
- **Pointer/Reference alignment**: Left (`int* ptr`, not `int *ptr`)

### Formatting your code

Format your code before submitting:

```bash
# Format all C++ files
make format
```

## Project structure

```
quadrate/
├── cmd/          # Command-line tools
│   ├── quadc/    # Compiler
│   ├── quadfmt/  # Formatter
│   ├── quadlsp/  # Language server
│   └── ...
├── lib/          # Libraries
│   ├── qc/       # Compiler frontend (parser, AST, semantic analysis)
│   ├── qd/       # Embedding API (libqd)
│   ├── qdrt/     # Runtime (libqdrt)
│   ├── llvmgen/  # LLVM code generator
│   └── qd*/      # Standard library modules
├── examples/     # Example programs
├── tests/        # Test suite
└── docs/         # Documentation source
```

Editor integrations are in separate repositories:
- **Neovim**: https://git.sr.ht/~klahr/quadrate.nvim
- **VS Code**: https://git.sr.ht/~klahr/quadrate-vscode

Each library follows the structure:
```
lib/name/
├── meson.build
├── include/name/  # Public headers
├── src/           # Implementation
├── qd/            # Quadrate source (for stdlib modules)
└── tests/         # Unit tests
```

## Compiler warnings

All warnings are treated as errors (`-Werror`). The build uses comprehensive warning flags including:
- `-Wall -Wextra -Wpedantic`
- `-Wconversion -Wsign-conversion`
- `-Wshadow -Wnull-dereference`

Ensure your code compiles without warnings.

## Submitting contributions

Quadrate is hosted on [SourceHut](https://sr.ht/~klahr/quadrate). We use email-based workflows:

### Sending patches

1. Make your changes in a local branch
2. Commit with clear, descriptive messages
3. Send patches to the mailing list:

```bash
git send-email --to=~klahr/quadrate@lists.sr.ht HEAD^
```

If you're new to `git send-email`, see [git-send-email.io](https://git-send-email.io) for setup instructions.

### Commit messages

Write clear commit messages:

```
Short summary (50 chars or less)

More detailed explanation if needed. Wrap at 72 characters.
Explain what changed and why, not how (the diff shows how).

- Use bullet points for multiple changes
- Reference issues if applicable
```

### Before submitting

- [ ] Code compiles without warnings (`make debug` or `make release`)
- [ ] Tests pass (`make tests`)
- [ ] Code is formatted (`make format`)
- [ ] New features include tests
- [ ] Public API changes are documented

## Reporting issues

Report bugs and request features via the [issue tracker](https://todo.sr.ht/~klahr/quadrate) or by sending email to the mailing list.

Include:
- Quadrate version
- Operating system and compiler version
- Minimal reproduction case
- Expected vs. actual behavior

## Questions?

- Mailing list: ~klahr/quadrate@lists.sr.ht
- Documentation: https://quad.r8.rs
- Maintainer: [~klahr](https://sr.ht/~klahr)

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (see LICENSE).
