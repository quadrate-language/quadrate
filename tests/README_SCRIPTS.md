# Test Scripts

All tests are run through a single unified script: `run_tests.sh`

## Usage

```bash
# Run Quadrate language tests (default)
bash tests/run_tests.sh qd

# Run formatter tests
bash tests/run_tests.sh formatter

# Run Quadrate tests with valgrind
bash tests/run_tests.sh valgrind
```

## Environment Variables

- `QUADC` - Path to quadc compiler (default: `build/debug/cmd/quadc/quadc`)
- `QUADFMT` - Path to quadfmt formatter (default: `dist/bin/quadfmt`)
- `QUADRATE_ROOT` - Path to standard library (default: `lib/stdqd/qd`)
- `QUADRATE_LIBDIR` - Path to libraries (default: `dist/lib`)

## Examples

```bash
# Run tests with custom compiler path
QUADC=/path/to/quadc bash tests/run_tests.sh qd

# Run formatter tests
bash tests/run_tests.sh formatter

# Run tests with valgrind for memory leak detection
bash tests/run_tests.sh valgrind
```
