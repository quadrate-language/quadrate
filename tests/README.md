# Tests

Quadrate test suite.

## Structure

```
tests/
├── qd/           # Quadrate language tests (.qd files with .out expected output)
├── formatter/    # Formatter tests
└── run_tests.sh  # Unified test runner
```

## Running Tests

```bash
# All tests
make tests

# Quadrate language tests only
bash tests/run_tests.sh qd

# Formatter tests
bash tests/run_tests.sh formatter

# With valgrind
bash tests/run_tests.sh valgrind
```

## Adding Tests

1. Create `tests/qd/category/test_name.qd`
2. Run manually: `quadc -r tests/qd/category/test_name.qd`
3. Save expected output: `tests/qd/category/test_name.out`
