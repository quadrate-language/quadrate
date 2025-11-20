# Quadrate Performance Benchmarks

Comparative performance benchmarks between Quadrate and other languages.

## Setup

All benchmarks test the same algorithms:
1. **Arithmetic Loop**: Tight loop with 10 million iterations performing arithmetic operations
2. **Recursive Fibonacci**: Calculate fibonacci(35) using naive recursive algorithm

## Languages Tested

- **Quadrate**: JIT-compiled via LLVM (debug build)
- **C**: gcc -O3 (native compilation with optimizations)
- **Rust**: rustc -O (native compilation with optimizations)
- **Go**: go build (native compilation with default optimizations)
- **Node.js**: V8 JavaScript engine with JIT
- **Python**: CPython 3.x (interpreted)

## Results

### Arithmetic Loop (10M iterations)

| Language | Time (ms) | Relative to C |
|----------|-----------|---------------|
| C (gcc -O3) | 74 | 1.0x |
| Go | 81 | 1.1x |
| Node.js | 379 | 5.1x |
| Python | 3,019 | 40.8x |
| Quadrate | 4,315 | 58.3x |

### Recursive Fibonacci (n=35)

| Language | Time (ms) | Relative to C |
|----------|-----------|---------------|
| C (gcc -O3) | 40 | 1.0x |
| Go | 94 | 2.4x |
| Node.js | 280 | 7.0x |
| Python | 2,892 | 72.3x |
| Quadrate | 10,810 | 270.3x |

## Analysis

### Current Performance

**Quadrate vs C:**
- 58-270x slower than optimized C
- **Optimization flags (-O0 vs -O3) make NO difference** (<1% improvement)
- Bottleneck is runtime function calls, not generated code

**Quadrate vs Python:**
- 1.4x faster on arithmetic (comparable to interpreted Python!)
- 3.7x slower on fibonacci

**Quadrate vs Node.js:**
- 11.4x slower on arithmetic
- 38.6x slower on fibonacci

**Why Optimizations Don't Help:**
The LLVM IR shows every operation is an external runtime call:
```llvm
call %qd_exec_result @qd_dup(ptr %ctx)
call %qd_exec_result @qd_push_i(ptr %ctx, i64 2)
call %qd_exec_result @qd_lt(ptr %ctx)
```
LLVM cannot optimize away these external function calls, so -O3 has minimal effect.

### Observations

1. **Debug Build Impact**: Quadrate is currently tested in debug mode with sanitizers enabled
2. **Optimization Potential**: LLVM optimizations not fully utilized
3. **Runtime Overhead**: Stack operations may have overhead compared to register-based execution
4. **Tail Call Optimization**: Recursive fibonacci performance suggests missing optimizations
5. **Go Performance**: Go's compiled performance is nearly identical to C (excellent!)
6. **Compiled vs Interpreted**: Clear advantage for compiled languages (C, Go, Rust) over interpreted (Python)

### Next Steps

To improve performance (architectural changes needed):

1. **Inline Stack Operations**: Replace `qd_dup()`, `qd_add()` etc. with inline LLVM IR
2. **Register Allocation**: Keep hot values in LLVM virtual registers instead of stack
3. **Eliminate Runtime Calls**: Generate direct code instead of calling runtime library
4. **Stack Effect Analysis**: Optimize away redundant stack manipulations at compile time
5. **Tail Call Optimization**: Implement TCO for recursive functions
6. **JIT Register Allocation**: Use LLVM's register allocator instead of explicit stack

**Note**: Testing showed that `-O0` vs `-O3` makes <1% difference because the bottleneck
is function call overhead to the runtime library, which LLVM cannot optimize away.

## Running Benchmarks

```bash
# Compile Quadrate benchmark
QUADRATE_LIBDIR=dist/lib QUADRATE_ROOT=dist/share/quadrate \
    build/debug/cmd/quadc/quadc benchmarks/arithmetic.qd -o benchmarks/arithmetic_qd

# Compile C benchmark
gcc -O3 benchmarks/arithmetic.c -o benchmarks/arithmetic_c

# Run all benchmarks
bash benchmarks/run_benchmarks.sh
```

## Benchmark Code

All benchmark implementations are equivalent and located in:
- `arithmetic.qd` - Quadrate implementation
- `arithmetic.c` - C implementation
- `arithmetic.rs` - Rust implementation (requires rustc)
- `arithmetic.go` - Go implementation
- `arithmetic.js` - Node.js implementation
- `arithmetic.py` - Python implementation

**Note**: Rust benchmark requires `rustc` to be installed:
```bash
rustc -O benchmarks/arithmetic.rs -o benchmarks/arithmetic_rust
```
