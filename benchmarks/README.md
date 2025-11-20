# Quadrate Performance Benchmarks

Comparative performance benchmarks between Quadrate and other languages.

## Setup

All benchmarks test the same algorithms:
1. **Arithmetic Loop**: Tight loop with 10 million iterations performing arithmetic operations
2. **Recursive Fibonacci**: Calculate fibonacci(35) using naive recursive algorithm

## Languages Tested

- **Quadrate**: LLVM-based compilation with type-aware inline optimizations
- **C**: gcc -O3 (native compilation with optimizations)
- **Rust**: rustc -O (native compilation with optimizations)
- **Go**: go build (native compilation with default optimizations)
- **Node.js**: V8 JavaScript engine with JIT
- **Python**: CPython 3.x (interpreted)

## Results (Latest)

### Arithmetic Loop (10M iterations)

| Language | Time (ms) | Relative to C | Notes |
|----------|-----------|---------------|-------|
| **C (gcc -O3)** | **76** | **1.0x** | Baseline |
| **Rust** | **85** | **1.1x** | Nearly identical to C |
| **Go** | **82** | **1.1x** | Excellent native performance |
| **Node.js** | **383** | **5.0x** | V8 JIT optimization |
| **Python** | **2,706** | **35.6x** | CPython interpreter |
| **Quadrate (optimized)** | **3,859** | **50.8x** | Type-aware inline ops |

### Recursive Fibonacci (n=35)

| Language | Time (ms) | Relative to C | Notes |
|----------|-----------|---------------|-------|
| **C (gcc -O3)** | **47** | **1.0x** | Baseline |
| **Rust** | **58** | **1.2x** | Excellent |
| **Go** | **99** | **2.1x** | Good |
| **Node.js** | **280** | **6.0x** | JIT optimization |
| **Python** | **2,658** | **56.6x** | Interpreted overhead |
| **Quadrate (optimized)** | **9,350** | **198.9x** | Type-aware inline ops |

## Performance Comparison

### Compiled Native (C, Rust, Go)
- **C and Rust**: Nearly identical performance (~1.1x)
- **Go**: Slightly slower but still excellent (1.1-2.1x)
- All three benefit from native compilation and LLVM/GCC optimizations

### JIT Compiled (Node.js)
- **5-6x slower** than native on these benchmarks
- V8's JIT optimizer works well for JavaScript patterns
- Good balance between performance and flexibility

### Interpreted (Python)
- **36-57x slower** than native
- CPython interpreter overhead
- PyPy with JIT would be significantly faster

### Quadrate (LLVM + Type-Aware Inline)
- **51-199x slower** than native
- **1.4x faster than Python** on arithmetic
- **3.5x slower than Python** on recursion
- **Recent optimization: 9-12% improvement** via type-aware inlining

## Quadrate Optimization Progress

### Baseline (Before Optimization)
- Arithmetic: 4,315 ms
- Fibonacci: 10,810 ms
- **Every operation was a runtime function call**

### Current (Type-Aware Inline)
- Arithmetic: 3,859 ms (**9.8% faster**)
- Fibonacci: 9,350 ms (**13.5% faster**)
- **Inline arithmetic for integers, runtime calls for floats**

### How Type-Aware Optimization Works

```llvm
; Check if both operands are integers
if (type1 == INT && type2 == INT) {
    // Fast path: inline arithmetic (~10 instructions)
    result = value1 + value2
} else {
    // Slow path: call runtime function (handles floats)
    qd_add(ctx)
}
```

Benefits:
- ✅ Integer operations use fast inline path
- ✅ Float operations handled correctly by runtime
- ✅ CPU branch prediction learns the pattern
- ✅ All 133 tests passing

## Why Quadrate Is Still Slow

1. **Stack-Based Execution**: Every value lives in memory, not registers
2. **Type Tags**: Runtime type information for each value
3. **Limited Inlining**: Only +, -, * are inline; other ops still call functions
4. **No Register Allocation**: Values don't stay in CPU registers across operations

## Optimization Roadmap

### Completed ✅
- [x] Inline integer push
- [x] Type-aware inline arithmetic (+, -, *)
- [x] Runtime type checking with fast path

### Next Steps (Expected Impact)

1. **Inline more operations** (HIGH - 10-15% gain)
   - Division, modulo, comparisons
   - Stack operations (dup, swap, drop)

2. **Stack pointer caching** (HIGH - 20-30% gain)
   - Cache stack pointer and size in LLVM registers
   - Reduce memory loads/stores

3. **Static type analysis** (VERY HIGH - 50-100% gain)
   - Infer types at compile time
   - Skip runtime type checks for proven integer operations

4. **LLVM optimization passes** (MEDIUM - 10-20% gain)
   - Enable -O2/-O3 for generated LLVM IR
   - Let LLVM optimize inline operations

**Goal**: Reach 5-10x slower than C (comparable to Node.js JIT performance)

## Running Benchmarks

### Compile All Benchmarks

```bash
# C
gcc -O3 benchmarks/arithmetic.c -o benchmarks/arithmetic_c

# Rust
rustc -O benchmarks/arithmetic.rs -o benchmarks/arithmetic_rust

# Go
go build -o benchmarks/arithmetic_go benchmarks/arithmetic.go

# Quadrate
QUADRATE_LIBDIR=dist/lib QUADRATE_ROOT=dist/share/quadrate \
    build/debug/cmd/quadc/quadc benchmarks/arithmetic.qd \
    -o benchmarks/arithmetic_qd_typeaware
```

### Run All Benchmarks

```bash
cd benchmarks
./arithmetic_c
./arithmetic_rust
./arithmetic_go
node arithmetic.js
python3 arithmetic.py
./arithmetic_qd_typeaware
```

## Benchmark Code

All implementations are equivalent and located in:
- `arithmetic.qd` - Quadrate (type-aware inline optimizations)
- `arithmetic.c` - C (gcc -O3)
- `arithmetic.rs` - Rust (rustc -O)
- `arithmetic.go` - Go (default optimizations)
- `arithmetic.js` - Node.js (V8 JIT)
- `arithmetic.py` - Python (CPython interpreter)

## Analysis Notes

**Why -O0 vs -O3 made no difference originally:**
LLVM couldn't optimize external runtime function calls. With inline operations, LLVM optimizations now have an effect.

**Branch prediction advantage:**
Modern CPUs predict the integer fast path correctly 99%+ of the time in tight loops, making the type check nearly free.

**Rust vs C performance:**
Rust's zero-cost abstractions deliver C-level performance. The small difference (1.1x) is within measurement variance.
