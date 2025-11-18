# quadpm - Quadrate Package Manager

A simple package manager for Quadrate that fetches 3rd party modules from Git repositories.

## Usage

### Install a package from Git

```bash
# Install from a Git URL (defaults to 'main' branch)
quadpm get https://git.sr.ht/~user/zlib

# Install a specific version (tag)
quadpm get https://git.sr.ht/~user/zlib@v1.2.0

# Install from a specific branch
quadpm get https://github.com/user/http@main

# Install from local repository
quadpm get /path/to/local/repo@v1.0.0
```

### List installed packages

```bash
quadpm list
# or
quadpm ls
```

### Package structure on disk

Packages are installed to one of these locations (in order of precedence):

1. `$QUADRATE_CACHE/` (if set)
2. `$XDG_DATA_HOME/quadrate/packages/` (if XDG_DATA_HOME is set)
3. `~/quadrate/packages/` (default)

Directory format: `<module>@<version>/`

Examples:
- `~/quadrate/packages/zlib@v1.2.0/`
- `~/quadrate/packages/http@main/`
- `~/quadrate/packages/json@v1.5.0/`

Each package should have a `module.qd` file at the root.

### Environment Variables

- **`QUADRATE_CACHE`**: Override package installation directory
- **`XDG_DATA_HOME`**: If set, packages install to `$XDG_DATA_HOME/quadrate/packages/`

Examples:
```bash
# Use custom cache location
export QUADRATE_CACHE=/opt/quadrate-packages
quadpm get https://git.sr.ht/~user/zlib@v1.2.0

# Use XDG standard
export XDG_DATA_HOME=~/.local/share
quadpm get https://git.sr.ht/~user/zlib@v1.2.0
# Installs to ~/.local/share/quadrate/packages/
```

## Package Structure

A valid Quadrate package should have this structure:

```
my-module/
├── module.qd              # Main module file (required)
├── helper.qd              # Additional Quadrate files (optional)
├── src/                   # C implementation (optional)
│   └── mymodule.c
└── quadrate.toml          # Package metadata (optional, for future use)
```

## Using Installed Packages

After installing a package, you can use it in your Quadrate code:

```quadrate
use zlib

fn main( -- ) {
    zlib::compress "Hello, World!"
}
```

The compiler will automatically find the package in `~/quadrate/packages/`.

## Implementation Details

- Uses `git` command directly (no libgit2 dependency)
- Performs shallow clones (`--depth 1`) for faster downloads
- Supports tags, branches, and commit hashes
- Multiple versions of the same module can coexist
- Simple flat directory structure

## Future Features

- `quadpm update` - Update packages to latest versions
- `quadpm remove` - Remove installed packages
- `quadpm search` - Search a package registry
- `quadrate.toml` - Dependency manifest files
- Dependency resolution and lockfiles
