# Performance Characteristics

This document describes Quadrate's performance characteristics and optimization guidance.

## Overview

Quadrate compiles to native code via LLVM, providing performance comparable to other compiled languages. It is typically faster than interpreted languages (Python, Ruby) and comparable to JIT-compiled languages (Node.js, LuaJIT).

## Optimization Levels

The `quadc` compiler supports LLVM optimization levels:

| Flag | Description | Use Case |
|------|-------------|----------|
| `-O0` | No optimization | Debug builds, fast compilation |
| `-O1` | Basic optimizations | Balance between speed and compile time |
| `-O2` | Standard optimizations | Production builds |
| `-O3` | Aggressive optimizations | Performance-critical code |

Note: Higher optimization levels increase compile time but typically improve runtime performance.

## Memory and Types

### Value Types vs Pointers

- **Integers and floats** are passed by value (copied)
- **Strings** are pointers to heap-allocated data with reference counting
- **Structs** are passed by value (copied on the stack)
- Use `ptr` types for large data structures to avoid copying

```quadrate
// Small struct - pass by value is fine
struct Point { x:i64 y:i64 }

// Large struct - consider using ptr
struct LargeData { buffer:ptr size:i64 /* ... many fields */ }
```

### String Operations

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `str::len` | O(1) | Length stored with string |
| `str::concat` | O(n+m) | Creates new string |
| `str::char_at` | O(1) | Direct index access |
| `str::slice` | O(k) | Where k is slice length |
| `str::find` | O(n*m) | Naive string search |
| `str::split` | O(n) | Returns array of strings |

For building strings in a loop, use `sb` (StringBuilder) module to avoid O(n²) concatenation:

```quadrate
use sb

fn build_string(n:i64 -- result:str) {
    sb::new -> builder
    0 n 1 for i {
        builder "item" sb::append
    }
    builder sb::finish
}
```

### Collections

**Vec<T>** (growable arrays):
- Push: Amortized O(1), doubles capacity on growth
- Access: O(1)
- Insert/Remove at index: O(n)

**HashMap<K,V>** (via containers module):
- Get/Set: Average O(1), worst O(n)
- Iteration: O(n)

## Function Calls

- Regular function calls have minimal overhead (LLVM inlines small functions)
- Recursive functions are not tail-call optimized (may cause stack overflow on deep recursion)
- Anonymous functions (closures) have slightly higher overhead due to capture handling

## Stack Operations

Core stack operations are highly optimized:
- `dup`, `swap`, `drop`, `over`, `rot` - essentially free (register operations)
- `pick n`, `roll n` - O(n) for deep stack access

## I/O Performance

File I/O uses buffered operations:
- Default buffer size: 8KB
- Use `io::flush` for immediate write when needed
- For bulk reads, `io::read_all` is more efficient than repeated `io::read_line`

## Benchmark Reference

Example benchmark results (Intel i7, -O3):

| Benchmark | Time |
|-----------|------|
| Arithmetic loop (10M iterations) | ~500ms |
| Fibonacci(35) recursive | ~800ms |

These numbers are for reference only. Performance varies by:
- CPU architecture
- Optimization level
- Workload characteristics

## Optimization Tips

1. **Use appropriate types**: `i64` for integers, `f64` for floating point
2. **Avoid string concatenation in loops**: Use `sb` module
3. **Pass large structs by pointer**: Reduces copying overhead
4. **Prefer iteration over recursion**: No tail-call optimization
5. **Use `-O3` for production**: Significant improvements for numerical code
6. **Profile your code**: Use `time` module to measure hot paths

## Comparison with Other Languages

Quadrate performance typically falls in this range:

```
Faster                                          Slower
   |                                               |
   C  Rust  Go  Node.js  Quadrate  LuaJIT  Python
```

This is a rough approximation. Actual performance depends heavily on:
- Workload type (numeric vs string vs I/O)
- Code patterns
- Optimization opportunities

For detailed benchmarks, see the `benchmarks/` directory.
