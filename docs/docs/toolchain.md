# Toolchain

Quadrate provides a complete set of tools for developing, building, and maintaining Quadrate programs.

## What every tool shares

The ten tools accept the same three options and report themselves the same way, so
what you learn from one carries to the rest:

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show the tool's help page on stdout |
| `-v`, `--version` | Show version information on stdout |
| `--no-color` | Disable coloured output |

- `--version` prints one line: `<tool> <version> (<commit>, built <date>)`.
- Messages about the tool itself go to **stderr** as `<tool>: <message>`, in lower
  case. Messages about your code use the compiler form below.
- An unrecognised option is an error, not something to ignore: the tool prints
  `<tool>: unknown option: <flag>` and `Try '<tool> --help' for more information.`,
  then exits 1.
- Tools that take paths exit 1 with `<tool>: no input files` when given none;
  `quad` and `quadpm` dispatch commands instead, so a bare invocation shows help.
- `--check` means "report, change nothing, exit 1 if there is something to change",
  for use in CI. `-w`/`--write` updates files in place.

`quad --no-color` is passed on to whichever tool it runs.

### Shell completions

Completions for all ten tools ship for bash, zsh and fish, and are installed by
`make install`:

| Shell | Installed as |
|-------|--------------|
| bash | `$PREFIX/share/bash-completion/completions/quad`, plus a link per tool |
| zsh | `$PREFIX/share/zsh/site-functions/_quad` |
| fish | `$PREFIX/share/fish/vendor_completions.d/quad.fish`, plus a link per tool |

bash and fish load a file named after the command being completed, hence the
per-tool links; zsh reads the commands from the file's `#compdef` line, so one
file is enough there.

The zsh and fish completions carry a description for every option, restrict file
arguments to the extensions a tool accepts, and complete `quad`'s and `quadpm`'s
subcommands — passing through to the underlying tool, so `quad lint --<TAB>`
offers what `quadlint` accepts.

### Two kinds of message

Tools distinguish what they say about *themselves* from what they say about
*your code*:

```
quadfmt: options -w and -c are mutually exclusive      <- the tool
main.qd:7:2: warning: Magic number '42'                <- your code
```

A message about the tool is `<tool>: <message>`. A message about your code is
`file:line:column: level: message` — the form gcc, clang and every editor's
error parser expect — where `level` is `error`, `warning` or `note`. `quadc` and
`quadlint` both emit it, so one editor configuration handles both.

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
quadc hello.qd               # Compile to ./hello
quadc hello.qd -o myprogram  # Compile with a custom output name
quadc -r hello.qd            # Compile and run immediately
quadc -r -                   # Compile and run source piped on stdin
quadc --verbose hello.qd     # Show compilation details
quadc --dump-ir hello.qd     # Output LLVM IR (for debugging)
quadc --dump-ast hello.qd    # Output the parsed AST
quadc --dump-tokens hello.qd # Show the token stream
```

The short options that take a value also have long forms: `-o`/`--output`,
`-g`/`--debug`, `-s`/`--stack-size`, `-I`/`--include` and `-l`/`--module`.

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

| Area | Provided |
|------|----------|
| Diagnostics | Syntax and semantic errors as you type |
| Completion | Builtins, keywords, user functions and stdlib names |
| Hover | Signatures and documentation |
| Navigation | Go to definition, find references, document and workspace symbols |
| Hierarchies | Call hierarchy and type hierarchy |
| Editing | Rename, code actions, linked editing, on-type formatting |
| Display | Semantic tokens, inlay hints, folding ranges, code lens, document links |

Also implemented: signature help, selection ranges, document highlight, and range
formatting.

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
quadpm get <url>[<<ref]                   # Install a module from Git
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

## quaddoc

Generates HTML documentation from Quadrate source files. Parses `///` doc comments with `@param`, `@return`, `@error`, `@example`, and `@field` tags.

Declarations come from the compiler's own parser, so the published signature is
the declared one — generics, receivers and function-pointer parameter types
included — and parameter tables use the names and types from the declaration,
with the tags supplying the prose. A file that does not parse is reported as
`file:line:column: warning: ...` and documented as far as it got.

```bash
quaddoc lib/                        # Generate docs for all modules in lib/
quaddoc -o api-docs lib/            # Output to custom directory (--output)
quaddoc --title "My API" src/       # Custom project title
```

The generated documentation includes function signatures, parameter tables, return values, error conditions, code examples, and `Calls:`/`Called by:` cross-references.

## quadmcp

Model Context Protocol server for AI assistants. Provides language documentation, code examples, and standard library reference to MCP-compatible tools.

```bash
quadmcp          # Serve MCP on stdin/stdout (how a client starts it)
quadmcp --http   # Serve MCP over HTTP on localhost:3000
```

Configure your MCP client to connect to `quadmcp` as a stdio-based server.
