# External packages

The following modules are available as separate packages. Install them using `quadpm get`:

```bash
quadpm get https://github.com/quadrate-language/compress
```

## Available packages

| Package | Description | Install |
|---------|-------------|---------|
| [compress](https://github.com/quadrate-language/compress) | Compression (gzip, zlib) | `quadpm get https://github.com/quadrate-language/compress` |
| [sqlite](https://github.com/quadrate-language/sqlite) | SQLite database | `quadpm get https://github.com/quadrate-language/sqlite` |

## Usage

After installing a package, use it like any other module:

<!-- doccheck: skip illustrative third-party package, not installed here -->
```qd
use compress

fn main() {
    "Hello, World!" compress::gzip! compress::gunzip! print nl
}
```

## Package management

```bash
# Install a single package
quadpm get https://github.com/quadrate-language/compress

# Install a specific version/ref
quadpm get https://github.com/quadrate-language/compress@v1.0.0

# Install all dependencies from qd.json
quadpm install

# List installed packages
quadpm list

# Update a package
quadpm update compress

# Remove a package
quadpm remove compress
```

## Package examples

### sqlite

Database access with SQLite:

<!-- doccheck: skip illustrative third-party package, not installed here -->
```qd
use sqlite

fn main() {
    // Open in-memory database
    ":memory:" sqlite::open! -> db

    // Create table
    "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)"
    db sqlite::exec!

    // Insert data
    "INSERT INTO users (name, age) VALUES ('Alice', 30)"
    db sqlite::exec!

    // Query with prepared statement
    "SELECT id, name, age FROM users" db sqlite::prepare! -> stmt

    stmt sqlite::step! 1 == if {
        1 stmt sqlite::column_text print " is " print
        2 stmt sqlite::column_int print " years old" print nl
    }

    stmt sqlite::finalize
    db sqlite::close
}
```

### compress

Compression with gzip:

<!-- doccheck: skip illustrative third-party package, not installed here -->
```qd
use compress

fn main() {
    "Hello, World! This is a test string for compression."

    // Compress
    compress::gzip!
    "Compressed size: " print dup len print nl

    // Decompress
    compress::gunzip! print nl
}
```

## Creating packages

See the [Modules](learn/3-functions/modules.md) section for information on creating your own packages.
