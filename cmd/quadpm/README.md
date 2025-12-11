# quadpm

Quadrate package manager.

## Usage

```bash
quadpm <command> [args]
```

## Commands

- `get <url>[@version]` - Install package from Git
- `list`, `ls` - List installed packages

## Examples

```bash
quadpm get https://git.sr.ht/~user/mylib
quadpm get https://github.com/user/lib@v1.0.0
quadpm list
```

## Package Location

Packages are installed to:
1. `$QUADRATE_PATH/` (if set)
2. `$XDG_DATA_HOME/quadrate/packages/`
3. `~/quadrate/packages/` (default)
