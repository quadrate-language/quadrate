# sha256sum

Compute SHA-256 hash of files.

## Run

```bash
quad run sha256sum.qd <file>
```

## Features

- `crypto::sha256` over a file's bytes, read whole with `io::read_file`
- Checking a checksum file split with `strings::lines`, each kind of report a word of its own
- Command-line flags with `flag::either`, read once into globals
- Stack code throughout: no `def`, and no local but the flags' globals and a failure count
