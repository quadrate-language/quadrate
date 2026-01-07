# Tests

Quadrate test suite.

## Structure

```
tests/
├── qd/           # Quadrate language tests (.qd files with .out expected output)
├── formatter/    # Code formatter tests
├── linter/       # Linter tests
├── fuzz/         # Fuzzing tests with libFuzzer
├── quadpm/       # Package manager tests
├── quaduses/     # Dependency analyzer tests
├── run_args/     # CLI argument passing tests
├── run_all.sh    # Unified test runner (main entry point)
├── run_embed_tests.sh    # Embedding API tests
├── run_linter_tests.sh   # Linter test runner
├── run_mtls_test.sh      # mTLS tests
├── test_utils.sh         # Shared test utilities
├── external_modules.txt  # External modules for testing
└── valgrind.supp         # Valgrind suppressions
```

## Running tests

### All tests

```bash
make tests
```

### Specific test suites

```bash
# Run specific suite
make tests SUITE=qd
make tests SUITE=formatter
make tests SUITE=linter
make tests SUITE=embed
make tests SUITE=args

# Available suites: cpp, lsp, qd, formatter, linter, embed, quadpm, args, stdlib, mtls, fuzz
```

### Individual tests

```bash
# Run specific test by name
make tests TEST=arrays/make_basic

# Run only previously failed tests
make tests-failed

# Clear failed test cache
make tests-clear
```

### Memory testing

```bash
# Run with valgrind
make valgrind

# Run specific suite with valgrind
make valgrind SUITE=qd
```

### Fuzzing

```bash
# Run fuzzer (10 seconds by default)
make fuzz

# Custom duration and max input length
make fuzz TIME=60 LEN=5000
```

## Test suites

### C++ unit tests (`cpp`)

Tests for C++ components (parser, semantic validator, etc.). Located in `lib/*/tests/`.

```bash
bash tests/run_all.sh --suite cpp
```

### LSP tests (`lsp`)

Language Server Protocol tests. Python-based tests in `cmd/quadlsp/tests/`.

```bash
bash tests/run_all.sh --suite lsp
```

### Quadrate language tests (`qd`)

Tests for Quadrate language features. Each test is a `.qd` file with corresponding `.out` file containing expected output.

```bash
bash tests/run_all.sh --suite qd
```

### Formatter tests (`formatter`)

Tests that code formatter produces expected output and is idempotent.

```bash
bash tests/run_all.sh --suite formatter
```

### Linter tests (`linter`)

Tests for quadlint static analysis tool.

```bash
bash tests/run_all.sh --suite linter
```

### Embedding tests (`embed`)

Tests for embedding Quadrate in C/C++ applications via the JIT API.

```bash
bash tests/run_all.sh --suite embed
```

### Package manager tests (`quadpm`)

Tests for quadpm package manager.

```bash
bash tests/run_all.sh --suite quadpm
```

### CLI argument tests (`args`)

Tests for command-line argument passing via `--` separator.

```bash
bash tests/run_all.sh --suite args
```

### Standard library tests (`stdlib`)

Tests that use external stdlib modules (requires network access to clone).

```bash
bash tests/run_all.sh --suite stdlib
```

### mTLS tests (`mtls`)

Tests for mutual TLS functionality (requires openssl).

```bash
bash tests/run_all.sh --suite mtls
```

### Fuzz tests (`fuzz`)

Fuzzing tests using libFuzzer (requires clang). Runs for 10 seconds by default.

```bash
bash tests/run_all.sh --suite fuzz

# Custom duration
bash tests/run_all.sh --suite fuzz --fuzz-time 60
```

## Adding tests

### Quadrate language tests

1. Create `tests/qd/category/test_name.qd`
2. Run manually: `quadc -r tests/qd/category/test_name.qd`
3. Save expected output: `tests/qd/category/test_name.out`

The test runner will automatically discover and run new tests.

### Formatter tests

1. Create input file: `tests/formatter/test_name.qd`
2. Create expected output: `tests/formatter/expected/test_name.qd`

The formatter will be run on the input and compared to expected output, then run again to verify idempotency.

### Other test suites

See the respective directories and their README files for specific instructions.

## External modules

Tests can depend on external Quadrate modules listed in `external_modules.txt`. These are automatically cloned or built during test setup.

To skip external module tests locally, they will be skipped if modules fail to clone.
