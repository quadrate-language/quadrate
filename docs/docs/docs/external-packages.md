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

## Creating packages

See the [Modules](learn/3-functions/modules.md) section for information on creating your own packages.
