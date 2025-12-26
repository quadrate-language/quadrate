# Fuzzing

This directory contains fuzzing targets for the Quadrate parser using libFuzzer.

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

## Running the Fuzzer

```bash
# Basic run (uses seed corpus)
./build/fuzz/fuzz/fuzz_parser fuzz/corpus/

# Limit input size (recommended)
./build/fuzz/fuzz/fuzz_parser fuzz/corpus/ -max_len=10000

# Run for limited time (e.g., 60 seconds)
./build/fuzz/fuzz/fuzz_parser fuzz/corpus/ -max_total_time=60

# Use multiple cores
./build/fuzz/fuzz/fuzz_parser fuzz/corpus/ -jobs=4 -workers=4

# Save crashes to specific directory
./build/fuzz/fuzz/fuzz_parser fuzz/corpus/ -artifact_prefix=fuzz/crashes/
```

## Corpus

The `corpus/` directory contains seed inputs for the fuzzer:

- Valid Quadrate source files from the test suite
- Edge case inputs (empty, deeply nested, etc.)

The fuzzer will mutate these inputs to find crashes.

## Reproducing Crashes

When the fuzzer finds a crash, it saves the input to a file. To reproduce:

```bash
# Run with specific crash input
./build/fuzz/fuzz/fuzz_parser crash-<hash>
```

## Adding New Fuzz Targets

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
