# Benchmarks

Performance benchmarks comparing Quadrate to other languages.

## Run

```bash
./benchmarks/run_benchmarks.sh
```

## Tests

- **Arithmetic Loop** - 10M iterations with arithmetic operations
- **Recursive Fibonacci** - fib(35) with naive recursion
- **String Concat** - 50K iterations of single-char append
- **Collatz Conjecture** - longest chain under 1M
- **Prime Counting** - trial division under 1M
- **Ackermann(3,11)** - deep recursion stress test
- **Popcount** - total set bits 1..10M
- **Tak(24,16,8)** - Takeuchi function, triple recursion

## Languages

- C (gcc -O3)
- Rust (rustc -O)
- Go
- Node.js
- Python
- Quadrate (quadc -O3)

## Results

All compiled languages use -O3 (or equivalent) optimization. Quadrate compiles to native code via LLVM and achieves C-level performance for integer-only functions thanks to the native calling convention optimization.

### Arithmetic Loop (10M iterations)

| Language | Time |
|----------|------|
| C        | 75 ms |
| Go       | 81 ms |
| Rust     | 84 ms |
| Quadrate | 86 ms |
| Node.js  | 378 ms |
| Python   | 2584 ms |

### Recursive Fibonacci (n=35)

| Language | Time |
|----------|------|
| C        | 37 ms |
| Rust     | 53 ms |
| Quadrate | 55 ms |
| Go       | 91 ms |
| Node.js  | 264 ms |
| Python   | 2636 ms |

### String Concat (50K iterations)

| Language | Time |
|----------|------|
| Rust     | <1 ms |
| Node.js  | 6 ms |
| Python   | 102 ms |
| C        | 111 ms |
| Quadrate | 125 ms |
| Go       | 447 ms |

### Compute Benchmarks

Functions with loops use scoped variables and don't qualify for native calling convention, so they run on the stack machine. Recursive functions (Ackermann, Tak) qualify for native calling convention and achieve near-C performance.

#### Collatz Longest Chain (under 1M)

| Language | Time |
|----------|------|
| Rust     | 306 ms |
| C        | 326 ms |
| Go       | 573 ms |
| Node.js  | 2367 ms |
| Quadrate | 13567 ms |
| Python   | 20865 ms |

#### Prime Counting (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 241 ms |
| Rust     | 262 ms |
| Node.js  | 315 ms |
| C        | 545 ms |
| Go       | 609 ms |
| Python   | 5023 ms |

#### Ackermann(3, 11)

| Language | Time |
|----------|------|
| C        | 212 ms |
| Rust     | 253 ms |
| Quadrate | 298 ms |
| Go       | 1626 ms |
| Node.js  | 2889 ms |
| Python   | 44795 ms |

#### Popcount Sum (1..10M)

| Language | Time |
|----------|------|
| C        | 166 ms |
| Go       | 190 ms |
| Quadrate | 249 ms |
| Rust     | 266 ms |
| Node.js  | 275 ms |
| Python   | 26446 ms |

#### Tak(24, 16, 8)

| Language | Time |
|----------|------|
| C        | 4 ms |
| Quadrate | 6 ms |
| Rust     | 6 ms |
| Go       | 7 ms |
| Node.js  | 21 ms |
| Python   | 205 ms |

Results from an Intel Core i3-4030U @ 1.90GHz. Run `./benchmarks/run_benchmarks.sh` to reproduce.
