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

Options: `--frozen` (with `install`), `--no-scripts`, and the usual `-h`, `-v`,
`--no-color`.

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

## Native build configuration

A module's C is compiled with a fixed line — `-c -fPIC -O2 -Wall` plus the
include paths quadpm knows about. `native` adds to that:

```json
{
  "name": "mymodule",
  "native": {
    "link": ["m", "ssl"],
    "cflags": ["-DMYMODULE_SIZE=400", "-Wno-unused-function"]
  }
}
```

`cflags` are appended after quadpm's own flags and include paths, so a module
can override what it needs to: the compiler takes the last of a repeated option.
Both keys take a platform suffix — `link_linux`, `cflags_haiku` — which is added
to the common list rather than replacing it.

Without `cflags` the only way to set a compile-time constant in a vendored
dependency is to patch it, so a module that wraps a library with a build-time
switch needs this.

## Prebuild scripts

quadpm compiles a module's C as part of installing it, which leaves no moment
for anything to be prepared first. A module that generates or vendors sources
can declare a command to run in its own directory beforehand:

```json
{
  "name": "mymodule",
  "scripts": {
    "prebuild": "make import"
  }
}
```

It runs before the `src/` check, so it may be what creates `src/`. A non-zero
exit fails the build. The command is echoed as it runs:

```
  → Running prebuild for mymodule: make import
  ✓ Prebuild finished
```

`--no-scripts` skips every prebuild script and still succeeds, for installing a
module without executing anything it ships: auditing one before trusting it, or
mirroring it.

Note what that flag does and does not buy. Installing a module already means
compiling its C and linking it into the program that will run it, so a script
does not decide whether someone else's code runs on your machine, only whether
it runs at install time as well.

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
