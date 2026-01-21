# External packages

The following modules have been moved out of the core standard library and are now available as separate packages. Install them using `quadpm`:

```bash
quadpm install <package-name>
```

## Available packages

| Package | Description | Install |
|---------|-------------|---------|
| [hof](https://github.com/quadrate-language/hof) | Higher-order function combinators | `quadpm install hof` |
| [log](https://github.com/quadrate-language/log) | Logging with levels and formatting | `quadpm install log` |
| [compress](https://github.com/quadrate-language/compress) | Compression (gzip, zlib) | `quadpm install compress` |
| [net](https://github.com/quadrate-language/net) | TCP/UDP networking | `quadpm install net` |
| [tls](https://github.com/quadrate-language/tls) | TLS/SSL support | `quadpm install tls` |
| [http](https://github.com/quadrate-language/http) | HTTP client and server | `quadpm install http` |
| [sqlite](https://github.com/quadrate-language/sqlite) | SQLite database | `quadpm install sqlite` |
| [json](https://github.com/quadrate-language/json) | JSON parsing and generation | `quadpm install json` |
| [regex](https://github.com/quadrate-language/regex) | Regular expressions | `quadpm install regex` |
| [uri](https://github.com/quadrate-language/uri) | URI parsing | `quadpm install uri` |
| [hex](https://github.com/quadrate-language/hex) | Hex encoding/decoding | `quadpm install hex` |
| [base64](https://github.com/quadrate-language/base64) | Base64 encoding/decoding | `quadpm install base64` |
| [uuid](https://github.com/quadrate-language/uuid) | UUID generation | `quadpm install uuid` |
| [crypto](https://github.com/quadrate-language/crypto) | Cryptographic hashes (SHA256, MD5, CRC32) | `quadpm install crypto` |
| [ct](https://github.com/quadrate-language/ct) | Compile-time utilities | `quadpm install ct` |
| [sort](https://github.com/quadrate-language/sort) | Sorting algorithms | `quadpm install sort` |

## Usage

After installing a package, use it like any other module:

```qd
use json
use http

fn main() {
    // Use json module
    "{\"name\": \"test\"}" json::parse! -> obj

    // Use http module
    "https://api.example.com" http::get! -> response
}
```

## Package management

```bash
# Install a package
quadpm install json

# Install a specific version
quadpm install json@1.0.0

# List installed packages
quadpm list

# Update a package
quadpm update json

# Remove a package
quadpm remove json
```

## Package examples

### sqlite

Database access with SQLite:

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

### http

HTTP server with routing:

```qd
use http

fn handle_root(c:ptr -- ) {
    -> c
    c 200 "Hello, World!" http::string
}

fn handle_users(c:ptr -- ) {
    -> c
    c "id" http::param -> id
    c 200 id http::string
}

fn main() {
    http::engine -> e

    // Register routes
    e "/" &handle_root http::GET
    e "/users/:id" &handle_users http::GET

    // Start server
    e ":8080" http::run
}
```

### json

JSON parsing and generation:

```qd
use json

fn main() {
    // Parse JSON
    "{\"name\": \"Alice\", \"age\": 30}" json::parse! -> obj

    // Access fields
    obj "name" json::get_string -> name
    obj "age" json::get_int -> age

    name print " is " print age print nl

    // Create JSON
    json::object -> person
    person "name" "Bob" json::set_string
    person "age" 25 json::set_int

    person json::stringify -> output
    output print nl
}
```

### net

TCP networking:

```qd
use net

fn main() {
    // TCP client
    "127.0.0.1" 8080 net::connect! -> sock

    "GET / HTTP/1.0\r\n\r\n" sock net::send!

    1024 sock net::recv! -> response
    response print nl

    sock net::close
}
```

### crypto

Cryptographic hashing:

```qd
use crypto

fn main() {
    "Hello, World!" -> msg

    msg crypto::sha256 -> hash
    "SHA256: " print hash print nl

    msg crypto::md5 -> md5_hash
    "MD5: " print md5_hash print nl

    msg crypto::crc32 -> checksum
    "CRC32: " print checksum print nl
}
```

### regex

Regular expressions:

```qd
use regex

fn main() {
    "hello123world" -> text
    "[0-9]+" regex::compile! -> re

    text re regex::match! if {
        "Found numbers" print nl
    }

    text re regex::find_all! -> matches
    matches len 0 do i {
        i matches nth print nl
    }

    re regex::free
}
```

### compress

Compression with gzip:

```qd
use compress

fn main() {
    "Hello, World! This is a test string for compression." -> data

    // Compress
    data compress::gzip! -> compressed
    "Compressed size: " print compressed str::len print nl

    // Decompress
    compressed compress::gunzip! -> decompressed
    decompressed print nl
}
```

## Creating packages

See the [Modules](learn/3-functions/modules.md) section for information on creating your own packages.
