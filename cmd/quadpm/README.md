# quadpm

Quadrate package manager.

## Usage

```bash
quadpm <command> [args]
```

## Commands

- `get <url>[<<version]` - Install module from Git
- `list`, `ls` - List installed modules

## Examples

```bash
quadpm get https://git.sr.ht/~user/mylib
quadpm get https://git.sr.ht/~user/lib@v1.0.0
quadpm list
```

## Module location

Modules are installed to:
1. `$QUADRATE_PATH/` (if set)
2. `$XDG_DATA_HOME/quadrate/modules/`
3. `~/quadrate/modules/` (default)
