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

| Command | Runs | Purpose |
|---------|------|---------|
| `build` | `quadc` | Compile source files |
| `run` | `quadc -r` | Build and run a program; `quad run greet.qd -- Alice` passes `Alice` to it |
| `test` | `quadc --test` | Run the `test` blocks in the current directory |
| `fmt` | `quadfmt -w` | Format files in place; `quad fmt --check` only reports |
| `lint` | `quadlint` | Check code for common issues |
| `uses` | `quaduses -w` | Add and remove `use` statements; `quad uses --check` only reports |
| `repl` | `quadrepl` | Start the interactive REPL |
| `lsp` | `quadlsp` | Start the language server |
| `doc` | `quaddoc` | Generate HTML documentation |
| `pm` | `quadpm` | Manage third-party modules |
| `mcp` | `quadmcp` | Start the MCP server |
| `init` | | Create a new project |
| `clean` | | Remove build artifacts |
| `version` | | Show version information |
| `help` | | `quad help <command>` shows that command's help |

Any option `quad` does not know is passed through to the tool, so `quad lint --json` is
`quadlint --json`. `quad script.qd` runs a script directly, which makes a
`#!/usr/bin/env quad` shebang line work.

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

| Option | Description |
|--------|-------------|
| `-o`, `--output <name>` | Output executable name (default: the source file's name) |
| `-O0` ... `-O3` | Optimization level (default: `-O0`) |
| `-g`, `--debug` | Generate debug information for GDB/LLDB |
| `-s`, `--stack-size <size>` | Stack size in elements (default: 1024) |
| `-I`, `--include <path>` | Add a module search path (repeatable) |
| `-l`, `--module <mod@ver>` | Pin a module to a version, e.g. `-l color@1.0.0` |
| `-r`, `--run` | Compile and run immediately, using the JIT |
| `--no-jit` | With `-r`, link an executable and run it instead of using the JIT |
| `--test` | Compile and run the `test` blocks |
| `--coverage` | With `--test`, print a function coverage report |
| `--target <triple>` | Cross-compile, e.g. `--target aarch64-linux-gnu` |
| `--freestanding` | No hosted runtime: emit a `.o` with no libc and no automatic `main` |
| `--default-float <f64\|d128>` | The type of a float literal written without `d` (default: `f64`) |
| `--werror` | Treat warnings as errors |
| `--verbose` | Show detailed compilation steps |
| `--save-temps` | Keep temporary files for debugging |
| `--dump-tokens`, `--dump-ast`, `--dump-ir` | Print the token stream, the AST or the LLVM IR |
| `-- <args>` | With `-r`, pass the rest to the program |

`--default-float d128` is for a calculator, where every number typed should be decimal: it
reads `1.5` as `1.5d` in the files named on the command line, but not in the modules they
import. Integer literals stay `i64` and widen where they meet a `d128`.

## quadfmt

Code formatter that enforces consistent style across your codebase.

```bash
quadfmt hello.qd       # Show formatted output (dry run)
quadfmt -w hello.qd    # Format file in place
quadfmt -w src/        # Format all .qd files in directory
```

| Option | Description |
|--------|-------------|
| `-w`, `--write` | Format files in place |
| `-c`, `--check` | Report whether files are formatted; exit 1 if not |
| `--no-sort-imports` | Leave `use` statements in their original order |
| `-` | Read from stdin, write to stdout (for an editor) |

Options can also be set in a `.quadfmt.json`, searched for in the current directory and its
parents: `{ "sortImports": true, "alignStructFields": true }`.

Note: `quad fmt` automatically adds `-w` to format files in place. Use `quad fmt --check` for dry run.

## quadlint

Static analyzer that catches common mistakes and potential issues.

```bash
quadlint hello.qd      # Check single file
quadlint src/          # Check all files in directory
quadlint --json src/   # Report as JSON, for an editor or CI
```

| Option | Description |
|--------|-------------|
| `--json` | Report issues as JSON |
| `-q`, `--quiet` | Only show the summary |
| `--max-nesting <N>` | Maximum nesting depth (default: 4) |
| `--no-unused-functions` | Disable unused function warnings |
| `--no-unused-variables` | Disable unused variable warnings |
| `--no-dead-code` | Disable dead code warnings |
| `--no-deep-nesting` | Disable deep nesting warnings |
| `--no-missing-defer` | Disable missing `defer` warnings |
| `--no-shadow-variables` | Disable shadowed variable warnings |
| `--no-empty-blocks` | Disable empty block warnings |
| `--no-constant-conditions` | Disable constant condition warnings |
| `--check-magic-numbers` | Warn about magic numbers (off by default) |
| `--check-long-functions` | Warn about long functions (off by default) |
| `--max-function-lines <N>` | With `--check-long-functions`, the limit (default: 50) |
| `--check-naming` | Check naming conventions (off by default) |
| `--source-path <path>` | Lint the one file given as if it were at `path`: for an editor linting an unsaved copy |

A function is unused only if nothing in its directory calls it: `quadlint` reads the other
`.qd` files beside the one it checks, as the compiler merges them.

`//nolint` at the end of a line suppresses every warning on it; `//nolint:rule1,rule2`
suppresses the named ones. The rules are `unused-functions`, `unused-variables`,
`dead-code`, `deep-nesting`, `missing-defer`, `shadow-variables`, `empty-blocks`,
`constant-conditions`, `magic-numbers`, `long-functions` and `naming`.

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

The diagnostics include `quadlint`'s warnings. By default the LSP runs the `quadlint` built
beside it, and otherwise the one on `PATH`. Both can be changed in the initialization
options, or later under `quadrate` in `workspace/didChangeConfiguration`:

```json
{ "lint": { "enabled": true, "path": "/usr/local/bin/quadlint" } }
```

## quadrepl

Interactive Read-Eval-Print Loop for experimenting with Quadrate expressions.

```bash
quadrepl                    # Start an interactive session
echo '1 2 add' | quadrepl   # Evaluate a pipe and print the final stack
```

Inside the REPL, what a line leaves on the stack stays there, and the prompt shows it:
```
[]> 5 3
[5 3]> +
[8]> print
8
[]> fn double(x:i64 -- y:i64) { x x + }
Function defined
[]> 21 double print
42
[]> exit
```

| Command | Description |
|---------|-------------|
| `help`, `:help`, `:h` | Show the commands and key bindings |
| `exit`, `quit`, `:q` | Exit (Ctrl+D also works) |
| `stack`, `:stack` | Show the stack's values |
| `type`, `:type` | Show the stack's types |
| `clear`, `:clear` | Clear the stack |
| `depth`, `:depth` | Show how many values are on the stack |
| `reset`, `:reset` | Reset the stack, definitions and imports |
| `:doc <name>` | Show a signature; with no exact match, search names |
| `:save <file>` | Save the session history to a file |
| `:load <file>` | Load and run commands from a file |

`-p`/`--print` prints the stack on exit; it is implied when stdin is a pipe.

## quadpm

Module manager for installing third-party modules from Git repositories.

```bash
quadpm install                           # Install the dependencies in qd.json
quadpm install --frozen                  # Install strictly from qd.lock (for CI)
quadpm lock                              # Write qd.lock from what is installed
quadpm get <url>[@ref]                   # Install one module from Git
quadpm update [name]                     # Update installed module(s)
quadpm remove <name>                     # Remove an installed module
quadpm list                              # List installed modules
quadpm outdated                          # Show modules with newer versions
quadpm build                             # Build C sources in current module
```

`--no-scripts` skips modules' prebuild scripts.

Dependencies are listed in `qd.json`, as a URL with an optional `@ref`, a local path, or an
object with a version range:

```json
{
  "name": "mymodule",
  "dependencies": {
    "glut":  "https://github.com/user/qd-glut@v1.0.0",
    "http":  { "url": "https://github.com/user/qd-http", "version": "^2.0.0" },
    "mylib": "../local/path"
  }
}
```

A range is `^1.2.3` (compatible: below 2.0.0), `~1.2.3` (below 1.3.0), `1.2.x`,
comparisons such as `>=1.0.0, <2.0.0`, a hyphen range `1.0.0 - 2.0.0`, alternatives joined
with `||`, or `*`. `qd.lock` pins the exact commit of each module, transitive dependencies
included, so a build can be reproduced.

Modules are installed in `$QUADRATE_PATH` if set, else `$XDG_DATA_HOME/quadrate/modules`,
else `~/quadrate/modules`.

The `build` command is useful during local module development. It compiles any C source files in `src/` and creates both shared (`.so`) and static (`.a`) libraries in `lib/`.

## quaduses

Automatically manages `use` statements in your source files. Analyzes your code and adds missing imports or removes unused ones.

```bash
quaduses hello.qd        # Show what changes would be made
quaduses -w hello.qd     # Update file in place
quaduses -w src/         # Update all .qd files in directory recursively
```

`-c`/`--check` reports whether files need changes and exits 1 if so; `-n`/`--dry-run`
shows the changes without making them.

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
quaddoc --css custom.css lib/       # Append a stylesheet after the default styles
```

With no `-o`, the output goes to `docs/` in the current directory. `-q`/`--quiet` prints
nothing but errors.

The generated documentation includes function signatures, parameter tables, return values, error conditions, code examples, and `Calls:`/`Called by:` cross-references.

## quadmcp

Model Context Protocol server for AI assistants. Provides language documentation, code examples, and standard library reference to MCP-compatible tools.

```bash
quadmcp          # Serve MCP on stdin/stdout (how a client starts it)
quadmcp --http   # Serve MCP over HTTP on localhost:3000
quadmcp --http --host 0.0.0.0   # Bind to another address
```

Configure your MCP client to connect to `quadmcp` as a stdio-based server.
