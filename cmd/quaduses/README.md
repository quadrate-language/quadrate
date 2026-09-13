# quaduses

Quadrate use-statement manager.

Analyses a source file and adds the use statements it needs, removing the ones it
does not.

## Usage

```bash
quaduses [options] <file|directory>...
```

## Options

| Option | Description |
|--------|-------------|
| `-w, --write` | Update files in place |
| `-c, --check` | Report whether files need changes (exit 1 if so) |
| `-n, --dry-run` | Show what would change without modifying |

## Examples

```bash
quaduses file.qd      # Print the updated file to stdout
quaduses -w file.qd   # Update use statements in place
quaduses -w src/      # Update every .qd file in a directory, recursively
quaduses -c src/      # Check whether any file needs updating (for CI)
quaduses -n file.qd   # Show the changes that would be made
```

`quaduses --help` is the authoritative option list. Every Quadrate tool accepts
`-h`/`--help`, `-v`/`--version` and `--no-color`.
