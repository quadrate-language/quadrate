# Quadrate Performance Benchmarks

Comparative performance benchmarks between Quadrate and other languages.

## Setup

All benchmarks test the same algorithms:
1. **Arithmetic Loop**: Tight loop with 10 million iterations performing arithmetic operations
2. **Recursive Fibonacci**: Calculate fibonacci(35) using naive recursive algorithm

## Languages Tested

- **Quadrate**: Native compilation via LLVM
- **C**: gcc -O3 (native compilation with optimizations)
- **Rust**: rustc -O (native compilation with optimizations)
- **Go**: go build (native compilation with default optimizations)
- **Node.js**: V8 JavaScript engine with JIT
- **Python**: CPython 3.x (interpreted)

## Results (Latest)

### Arithmetic Loop (10M iterations)

| Language | Time (ms) | Relative to C | Notes |
|----------|-----------|---------------|-------|
| **C (gcc -O3)** | **35** | **1.0x** | Baseline |
| **Rust** | **39** | **1.1x** | Nearly identical to C |
| **Go** | **39** | **1.1x** | Excellent native performance |
| **Node.js** | **217** | **6.2x** | V8 JIT optimization |
| **Quadrate** | **210** | **6.0x** | Native compilation via LLVM |
| **Python** | **1,017** | **29.1x** | CPython interpreter |

### Recursive Fibonacci (n=35)

| Language | Time (ms) | Relative to C | Notes |
|----------|-----------|---------------|-------|
| **C (gcc -O3)** | **14** | **1.0x** | Baseline |
| **Rust** | **25** | **1.8x** | Excellent |
| **Go** | **46** | **3.3x** | Good |
| **Node.js** | **100** | **7.1x** | JIT optimization |
| **Quadrate** | **439** | **31x** | Native compilation via LLVM |
| **Python** | **1,149** | **82x** | Interpreted overhead |

## Performance Comparison

### Compiled Native (C, Rust, Go)
- **C and Rust**: Nearly identical performance (~1.1-1.8x)
- **Go**: Good performance (1.1-3.3x)
- All three benefit from native compilation and GCC/LLVM optimizations

### JIT Compiled (Node.js)
- **6-7x slower** than native on these benchmarks
- V8's JIT optimizer works well for JavaScript patterns
- Good balance between performance and flexibility

### Quadrate (LLVM)
- **6x slower** than C on arithmetic loops - **competitive with Node.js!**
- **31x slower** than C on recursion - function call overhead
- **2.6-4.8x faster than Python** across all benchmarks
- Compiles to native code via LLVM

### Interpreted (Python)
- **29-82x slower** than native
- CPython interpreter overhead
- PyPy with JIT would be significantly faster

## Running Benchmarks

### Compile All Benchmarks

```bash
# C
gcc -O3 benchmarks/arithmetic.c -o benchmarks/arithmetic_c -lm

# Rust
rustc -O benchmarks/arithmetic.rs -o benchmarks/arithmetic_rust

# Go
go build -o benchmarks/arithmetic_go benchmarks/arithmetic.go

# Quadrate
quadc -O3 benchmarks/arithmetic.qd -o benchmarks/arithmetic_qd
```

### Run All Benchmarks

```bash
./benchmarks/run_benchmarks.sh
```

Or individually:

```bash
./benchmarks/arithmetic_c
./benchmarks/arithmetic_rust
./benchmarks/arithmetic_go
node benchmarks/arithmetic.js
python3 benchmarks/arithmetic.py
./benchmarks/arithmetic_qd
```

## Benchmark Code

All implementations are equivalent and located in:
- `arithmetic.qd` - Quadrate (compiled via LLVM)
- `arithmetic.c` - C (gcc -O3)
- `arithmetic.rs` - Rust (rustc -O)
- `arithmetic.go` - Go (default optimizations)
- `arithmetic.js` - Node.js (V8 JIT)
- `arithmetic.py` - Python (CPython interpreter)

## Analysis Notes

**Quadrate performance:**
Quadrate compiles to native code via LLVM and achieves performance competitive with Node.js on arithmetic loops. Recursive function calls have more overhead due to the stack-based execution model.

**Rust vs C performance:**
Rust's zero-cost abstractions deliver C-level performance. The small difference is within measurement variance.
