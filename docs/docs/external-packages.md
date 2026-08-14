# External packages

The following modules are available as separate packages. Install them using `quadpm get`:

```bash
quadpm get https://git.sr.ht/~klahr/qdcompress
```

## Available packages

| Package | Description | Install |
|---------|-------------|---------|
| [compress](https://git.sr.ht/~klahr/qdcompress) | Compression (gzip, zlib) | `quadpm get https://git.sr.ht/~klahr/qdcompress` |
| [sqlite](https://git.sr.ht/~klahr/qdsqlite) | SQLite database | `quadpm get https://git.sr.ht/~klahr/qdsqlite` |

## Usage

After installing a package, use it like any other module:

<!-- doccheck: skip illustrative third-party package, not installed here -->
```qd
use compress

fn main() {
    "Hello, World!" compress::gzip! -> compressed
    compressed compress::gunzip! -> decompressed
    decompressed print nl
}
```

## Package management

```bash
# Install a single package
quadpm get https://git.sr.ht/~klahr/qdcompress

# Install a specific version/ref
quadpm get https://git.sr.ht/~klahr/qdcompress@v1.0.0

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

    stmt sqlite::step! -> has_row
    has_row 1 == if {
        0 stmt sqlite::column_int -> id
        1 stmt sqlite::column_text -> name
        2 stmt sqlite::column_int -> age
        name print " is " print age print " years old" print nl
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
    "Hello, World! This is a test string for compression." -> data

    // Compress
    data compress::gzip! -> compressed
    "Compressed size: " print compressed strings::len print nl

    // Decompress
    compressed compress::gunzip! -> decompressed
    decompressed print nl
}
```

## Creating packages

See the [Modules](learn/3-functions/modules.md) section for information on creating your own packages.
