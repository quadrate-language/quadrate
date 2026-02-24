# uuid

UUID v4 generation for Quadrate.

## Installation

```bash
quadpm get https://git.sr.ht/~klahr/qduuid
```

## Usage

```qd
use uuid

fn main() {
    // Generate a random UUID
    uuid::v4 -> id
    id print nl  // e.g., "550e8400-e29b-41d4-a716-446655440000"

    // Generate with specific seed (reproducible)
    12345 uuid::v4_seeded -> id2
    id2 print nl

    // Validate UUID format
    id uuid::is_valid -> valid
    valid print nl  // 1
}
```

## API

### Functions

- `v4( -- uuid:str)` - Generate random UUID v4
- `v4_seeded(seed:i64 -- uuid:str)` - Generate UUID v4 with seed
- `is_valid(s:str -- valid:i64)` - Check if string is valid UUID format

## License

Apache 2.0

## Contributing

Contributions welcome! Please open an issue or submit a patch on [SourceHut](https://git.sr.ht/~klahr/qduuid).
