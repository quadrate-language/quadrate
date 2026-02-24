# uri

URI encoding, decoding, and parsing for Quadrate.

## Installation

```bash
quadpm get https://git.sr.ht/~klahr/qduri
```

## Usage

```qd
use uri

fn main() {
    // Percent-encode a string
    "hello world" uri::encode -> encoded
    encoded print nl  // "hello%20world"

    // Decode a percent-encoded string
    "hello%20world" uri::decode -> decoded
    decoded print nl  // "hello world"

    // Parse a URI
    "https://example.com:8080/path?q=1#top" uri::parse -> u
    u @scheme print nl  // "https"
    u @host print nl    // "example.com"
    u @port print nl    // 8080
    u @path print nl    // "/path"
    u @query print nl   // "q=1"

    // Query parameter access
    "foo=bar&x=1" "foo" uri::query_get -> val
    val print nl  // "bar"

    // Build a URI from components
    u uri::build -> url
    url print nl
}
```

## API

### Types

- `Uri` - Parsed URI components (scheme, host, port, path, query, fragment)

### Functions

- `encode(s:str -- encoded:str)` - Percent-encode a string
- `decode(s:str -- decoded:str)` - Decode a percent-encoded string
- `parse(s:str -- u:ptr)` - Parse a URI string into components
- `build(u:ptr -- s:str)` - Build a URI string from components
- `query_get(query:str key:str -- value:str)` - Get query parameter value
- `query_has(query:str key:str -- exists:i64)` - Check if query parameter exists

## License

Apache 2.0

## Contributing

Contributions welcome! Please open an issue or submit a patch on [SourceHut](https://git.sr.ht/~klahr/qduri).
