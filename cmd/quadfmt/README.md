# quadfmt

Quadrate code formatter.

Formats Quadrate source files with consistent style. Formatting is AST-based, so
the output is reparsed and checked before anything is written.

## Usage

```bash
quadfmt [options] <file|directory>...
quadfmt [options] -                  # read from stdin, write to stdout
```

## Options

| Option | Description |
|--------|-------------|
| `-c, --check` | Report whether files are formatted (exit 1 if not) |
| `-w, --write` | Format files in place |
| `--no-sort-imports` | Leave use statements in their original order |

## Configuration

Options can be set in `.quadfmt.json`, searched for in the current directory and
its parents:

```json
{ "sortImports": true, "alignStructFields": true }
```

## Examples

```bash
quadfmt file.qd           # Print the formatted file to stdout
quadfmt -w file.qd        # Format in place
quadfmt -w src/           # Format every .qd file in a directory, recursively
quadfmt -c src/           # Check whether any file needs formatting (for CI)
cat file.qd | quadfmt -   # Format a buffer piped from an editor
```

`quadfmt --help` is the authoritative option list. Every Quadrate tool accepts
`-h`/`--help`, `-v`/`--version` and `--no-color`.
