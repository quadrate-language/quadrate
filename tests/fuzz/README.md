# Fuzzing

This directory contains fuzzing targets for the Quadrate parser using libFuzzer.

## Quick start

Fuzz tests are integrated into the test runner:

```bash
# Run quick 10-second fuzz test (part of make tests)
make tests

# Run fuzz tests only
bash tests/run_all.sh --suite fuzz

# Run longer fuzz session
bash tests/run_all.sh --suite fuzz --fuzz-time 60

# Extended fuzzing via make
make fuzz TIME=300
```

## Prerequisites

- Clang compiler (libFuzzer is built into Clang)
- AddressSanitizer support (comes with Clang)

## Building

```bash
# Set up fuzz build directory
CC=clang CXX=clang++ meson setup build/fuzz --buildtype=debug -Dbuild_fuzz=true

# Build fuzz targets
meson compile -C build/fuzz
```

## Running the fuzzer

```bash
# Basic run (uses seed corpus)
./build/fuzz/tests/fuzz/fuzz_parser tests/fuzz/corpus/

# Limit input size (recommended)
./build/fuzz/tests/fuzz/fuzz_parser tests/fuzz/corpus/ -max_len=10000

# Run for limited time (e.g., 60 seconds)
./build/fuzz/tests/fuzz/fuzz_parser tests/fuzz/corpus/ -max_total_time=60

# Use multiple cores
./build/fuzz/tests/fuzz/fuzz_parser tests/fuzz/corpus/ -jobs=4 -workers=4

# Save crashes to specific directory
./build/fuzz/tests/fuzz/fuzz_parser tests/fuzz/corpus/ -artifact_prefix=tests/fuzz/crashes/
```

## Corpus

The `corpus/` directory contains seed inputs for the fuzzer:

- Valid Quadrate source files from the test suite
- Edge case inputs (empty, deeply nested, etc.)

The fuzzer will mutate these inputs to find crashes.

## Reproducing crashes

When the fuzzer finds a crash, it saves the input to a file. To reproduce:

```bash
# Run with specific crash input
./build/fuzz/tests/fuzz/fuzz_parser crash-<hash>
```

## Adding new fuzz targets

1. Create a new `fuzz_<target>.cc` file
2. Implement `LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)`
3. Add the target to `meson.build`

## Sanitizers

The fuzzer is built with:
- **AddressSanitizer (ASan)**: Detects memory errors (buffer overflow, use-after-free)
- **UndefinedBehaviorSanitizer (UBSan)**: Detects undefined behavior

## Tips

- Run for extended periods (hours/days) to find rare bugs
- Keep the corpus directory small - libFuzzer minimizes it automatically
- Check `fuzz-<n>.log` files for fuzzer output when using multiple workers
