# quadpm

Quadrate module manager.

Installs and updates third-party modules from Git repositories, resolving
transitive dependencies automatically.

## Usage

```bash
quadpm [options] <command> [arguments]
```

## Commands

| Command | Description |
|---------|-------------|
| `install` | Install the dependencies listed in `qd.json` |
| `install --frozen` | Install only from `qd.lock` (fail if outdated) |
| `lock` | Generate or update `qd.lock` from the installed modules |
| `get <url>[@ref]` | Fetch and install one module from Git |
| `update [name]` | Update installed modules (`git pull`) |
| `remove <name>` | Remove an installed module |
| `list` | List installed modules |
| `outdated` | Show modules with newer versions available |
| `build` | Build the C sources of the module in this directory |

## Lockfile

`qd.lock` pins exact commit hashes for reproducible builds. `install` creates and
updates it, `install --frozen` uses it strictly (for CI), and `lock` regenerates
it from what is installed.

## Manifest

```json
{
  "name": "mymodule",
  "dependencies": {
    "glut":   "https://github.com/user/qd-glut@v1.0.0",
    "http":   { "url": "https://github.com/user/qd-http", "version": "^2.0.0" },
    "mylib":  "../local/path",
    "crypto": { "url": "https://github.com/user/qd-crypto",
                "version": "~1.5.0", "commit": "a1b2c3d4..." }
  }
}
```

Version ranges accept `^1.2.3`, `~1.2.3`, `1.2.x`, `>=1.0.0`, `<2.0.0`,
`1.0.0 - 2.0.0`, `>=1.0.0 <2.0.0 || >=3.0.0` and `*`.

## Module location

Modules are installed to, in order:

1. `$QUADRATE_PATH/`
2. `$XDG_DATA_HOME/quadrate/modules/`
3. `~/quadrate/modules/` (default)

## Examples

```bash
quadpm install                                  # Install everything qd.json asks for
quadpm install --frozen                         # Install strictly from qd.lock (for CI)
quadpm get https://github.com/user/zlib         # Install a module from Git
quadpm get https://github.com/user/zlib@1.2.0   # Install one version of it
quadpm list                                     # List what is installed
```

`quadpm --help` is the authoritative command and option list. Every Quadrate tool
accepts `-h`/`--help`, `-v`/`--version` and `--no-color`.
