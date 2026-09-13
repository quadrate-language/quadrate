# quad

Quadrate toolchain — a single entry point to the other tools.

Each command runs the matching tool and passes your options straight through to
it, so `quad lint --json src/` is `quadlint --json src/`.

## Usage

```bash
quad <command> [options] [arguments]
quad <file.qd> [arguments]           # run a script directly (for a shebang line)
```

## Commands

| Command | Runs | Description |
|---------|------|-------------|
| `build` | quadc | Compile Quadrate source files |
| `run` | quadc | Build and run a Quadrate program |
| `test` | quadc | Run tests |
| `fmt` | quadfmt | Format Quadrate source files |
| `lint` | quadlint | Check code for common issues |
| `repl` | quadrepl | Start interactive REPL |
| `uses` | quaduses | Manage use statements |
| `lsp` | quadlsp | Start language server |
| `doc` | quaddoc | Generate HTML documentation |
| `pm` | quadpm | Manage third-party modules |
| `mcp` | quadmcp | Start the MCP server |
| `init` | — | Initialize a new Quadrate project |
| `clean` | — | Remove build artifacts |

`quad fmt` and `quad uses` add `-w` for you, so they update files in place;
pass `--check` for a dry run.

## Examples

```bash
quad build main.qd           # Compile main.qd
quad run greet.qd -- Alice   # Run with the argument 'Alice'
quad fmt                     # Format every .qd file in place
quad test                    # Run the tests in the current directory
quad init myapp              # Create qd.json, main.qd and .gitignore
```

Run `quad help <command>` for a command's own options, or `quad --help` for the
full list. Every Quadrate tool accepts `-h`/`--help`, `-v`/`--version` and
`--no-color`.
