# Benchmarks

Performance benchmarks comparing Quadrate to other languages.

## Run

```bash
./benchmarks/run_benchmarks.sh
```

## Tests

- **Arithmetic Loop** - 10M iterations with arithmetic operations
- **Recursive Fibonacci** - fib(35) with naive recursion

## Languages

- C (gcc -O3)
- Rust (rustc -O)
- Go
- Node.js
- Python
- Quadrate

## Results

See benchmark output for current performance numbers. Quadrate compiles to native code via LLVM and typically performs between Node.js and Python.
