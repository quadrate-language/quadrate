# quaddoc

Quadrate documentation generator.

Generates an HTML API reference from `///` doc comments, reading the `@param`,
`@return`, `@error`, `@example` and `@field` tags. The output includes function
signatures, parameter tables, return values, error conditions, code examples, and
`Calls:`/`Called by:` cross-references built from a call-graph pass.

Declarations are read from the compiler's own AST, so what gets documented is
what the code actually declares: signatures are exact (including generics,
receivers and function-pointer parameter types), parameter tables use the names
and types from the declaration rather than from the tags, and the call graph
counts calls rather than words that happen to appear in a comment. A file that
does not parse is reported as `file:line:column: warning: ...` and documented as
far as it got, rather than published half-empty.

Doc comments themselves are still read from the text: the lexer discards `///`
along with every other comment, so there is no AST to read them from.

## Usage

```bash
quaddoc [options] [directory]   # default: the current directory
```

## Options

| Option | Description |
|--------|-------------|
| `-o, --output <dir>` | Output directory (default: `docs`) |
| `-q, --quiet` | Print nothing but errors |
| `--title <str>` | Project title (default: `"Quadrate API"`) |
| `--css <file>` | Append a custom CSS file after the default styles |

## Examples

```bash
quaddoc                          # Scan the current directory, write to docs/
quaddoc -o api lib/              # Scan lib/, write to api/
quaddoc --title "My Project" .   # Set the project title
quaddoc --css custom.css lib/    # Append custom styling
```

`quaddoc --help` is the authoritative option list. Every Quadrate tool accepts
`-h`/`--help`, `-v`/`--version` and `--no-color`.
