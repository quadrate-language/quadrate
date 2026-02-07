# Toolchain

Quadrate provides a complete set of tools for developing, building, and maintaining Quadrate programs.

## quad

The main CLI that provides a unified interface to all Quadrate tools. Instead of remembering separate commands, use `quad` as your single entry point.

```bash
quad run hello.qd      # Compile and run
quad build hello.qd    # Compile to binary
quad fmt hello.qd      # Format code
quad lint hello.qd     # Check for issues
quad test              # Run tests
quad repl              # Start interactive shell
quad init              # Initialize a new project
quad clean             # Remove build artifacts
```

For most development tasks, `quad` is all you need.

### quad init

Creates a new Quadrate project in the current directory:

```bash
quad init           # Create project named "myproject"
quad init myapp     # Create project with custom name
```

This creates three files:

| File | Description |
|------|-------------|
| `qd.json` | Package manifest with name, version, and dependencies |
| `main.qd` | Main source file with a Hello World program |
| `.gitignore` | Ignores build output and editor files |

After initializing, run your project with:

```bash
quad run
```

## quadc

The Quadrate compiler. Compiles `.qd` source files to native executables via LLVM.

```bash
quadc hello.qd              # Compile to ./hello
quadc hello.qd -o myprogram # Compile with custom output name
quadc -r hello.qd           # Compile and run immediately
quadc --verbose hello.qd    # Show compilation details
quadc --dump-ir hello.qd    # Output LLVM IR (for debugging)
quadc --dump-ast hello.qd   # Output parsed AST structure
quadc --dump-tokens hello.qd # Show token stream
```

## quadfmt

Code formatter that enforces consistent style across your codebase.

```bash
quadfmt hello.qd       # Show formatted output (dry run)
quadfmt -w hello.qd    # Format file in place
quadfmt -w src/        # Format all .qd files in directory
```

Note: `quad fmt` automatically adds `-w` to format files in place. Use `quad fmt --check` for dry run.

## quadlint

Static analyzer that catches common mistakes and potential issues.

```bash
quadlint hello.qd      # Check single file
quadlint src/          # Check all files in directory
```

## quadlsp

Language Server Protocol implementation for IDE integration. Provides:

- Syntax highlighting
- Error diagnostics
- Go to definition
- Autocompletion
- Hover information

Configure your editor to use `quadlsp` as the language server for `.qd` files.

## quadrepl

Interactive Read-Eval-Print Loop for experimenting with Quadrate expressions.

```bash
quadrepl
```

Inside the REPL:
```
[]> 5 3 + print
8
[]> fn double(x:i64 -- y:i64) { dup + }
Function defined
[]> 21 double print
42
[]> exit
```

## quadpm

Module manager for installing third-party modules from Git repositories.

```bash
quadpm list                              # List installed modules
quadpm get <url>[@ref]                   # Install a module from Git
quadpm update [name]                     # Update installed module(s)
quadpm remove <name>                     # Remove an installed module
quadpm build                             # Build C sources in current module
```

The `build` command is useful during local module development. It compiles any C source files in `src/` and creates both shared (`.so`) and static (`.a`) libraries in `lib/`.

## quaduses

Automatically manages `use` statements in your source files. Analyzes your code and adds missing imports or removes unused ones.

```bash
quaduses hello.qd        # Show what changes would be made
quaduses -w hello.qd     # Update file in place
quaduses -w src/         # Update all .qd files in directory recursively
```

Note: `quad uses` automatically adds `-w` to update files in place. Use `quad uses --check` for dry run.

## quadmcp

Model Context Protocol server for AI assistants. Provides language documentation, code examples, and standard library reference to MCP-compatible tools.

```bash
quadmcp    # Start the MCP server
```

Configure your MCP client to connect to `quadmcp` as a stdio-based server.
