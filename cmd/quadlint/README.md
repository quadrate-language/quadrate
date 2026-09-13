# quadlint

Quadrate code linter.

Checks Quadrate source files for code quality issues, working from the compiler's
own AST rather than from text.

## Usage

```bash
quadlint [options] <file|directory>...
```

## Options

| Option | Description |
|--------|-------------|
| `--json` | Report issues as JSON (for editors and CI) |
| `-q, --quiet` | Only show the summary, not individual issues |
| `--max-nesting <N>` | Maximum nesting depth (default: 4) |
| `--no-<rule>` | Disable one of the default rules |

Default rules: `unused-functions`, `unused-variables`, `dead-code`,
`deep-nesting`, `missing-defer`, `shadow-variables`, `empty-blocks`,
`constant-conditions`.

Stricter rules, off by default: `--check-magic-numbers`,
`--check-long-functions`, `--check-naming` (with `--max-function-lines <N>`).

## Inline suppression

Add `//nolint` on a line to suppress all warnings on that line, or
`//nolint:rule1,rule2` to suppress specific rules by the names above.

## Examples

```bash
quadlint file.qd       # Lint a single file
quadlint src/          # Lint every .qd file in a directory, recursively
quadlint --json src/   # Report issues as JSON for an editor or CI
quadlint -q src/       # Only show the summary
```

`quadlint --help` is the authoritative option list. Every Quadrate tool accepts
`-h`/`--help`, `-v`/`--version` and `--no-color`.
