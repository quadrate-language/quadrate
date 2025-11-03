# libstdqd - Quadrate Standard Library

A standard library for Quadrate providing networking, file I/O, and other common utilities.

## Overview

`libstdqd` is a C library that provides standard library functions for Quadrate programs. It follows the same conventions as `libquadrate`:

- All functions take `qd_context*` as the first parameter
- All functions return `qd_exec_result`
- Functions manipulate the stack for input/output
- Both static and dynamic library versions are available

## Building

The library is built as part of the main Quadrate project:

```bash
make debug    # Build debug version
make release  # Build release version
make tests    # Run tests
```

Build outputs:
- **Shared library**: `build/debug/lib/stdqd/libstdqd.so`
- **Static library**: `build/debug/lib/stdqd/libstdqd_static.a`

## Directory Structure

```
lib/stdqd/
├── include/
│   └── stdqd/
│       └── stdqd.h          # Main header file
├── src/
│   └── stdqd.c              # Library implementation
├── tests/
│   ├── meson.build          # Test build configuration
│   └── test_stdqd.c         # Unit tests
├── meson.build              # Library build configuration
└── README.md                # This file
```

## API

### Version Information

```c
const char* stdqd_version(void);
```

Returns the library version string (e.g., "0.1.0").

### Networking Functions

(To be implemented)

### File I/O Functions

(To be implemented)

## Usage from Quadrate

Once imported in Quadrate (planned syntax):

```qd
import "libstdqd.so" as std {
    fn network_connect(host:s port:i -- socket:i)
    fn file_open(path:s mode:s -- handle:p)
}

fn main() {
    "example.com" 80 std::network_connect
    // ...
}
```

## Testing

Run tests:

```bash
meson test -C build/debug stdqd --print-errorlogs
```

Or run directly:

```bash
./build/debug/lib/stdqd/tests/test_stdqd
```

## Development

To add new functions:

1. Add function declaration to `include/stdqd/stdqd.h`
2. Implement function in `src/` (create new files as needed)
3. Add tests to `tests/`
4. Update `meson.build` if new source files are added

## License

Same as the Quadrate project.
