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
| C        | 74 ms |
| Rust     | 84 ms |
| Go       | 85 ms |
| Quadrate | 86 ms |
| Node.js  | 380 ms |
| Python   | 2594 ms |

### Recursive Fibonacci (n=35)

| Language | Time |
|----------|------|
| C        | 37 ms |
| Rust     | 53 ms |
| Quadrate | 55 ms |
| Go       | 94 ms |
| Node.js  | 259 ms |
| Python   | 2586 ms |

### String Concat (50K iterations)

| Language | Time |
|----------|------|
| Rust     | <1 ms |
| Node.js  | 7 ms |
| Python   | 101 ms |
| C        | 114 ms |
| Quadrate | 123 ms |
| Go       | 431 ms |

### Compute Benchmarks

All integer-only functions qualify for native calling convention and achieve near-C performance.

#### Collatz Longest Chain (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 295 ms |
| Rust     | 319 ms |
| C        | 330 ms |
| Go       | 577 ms |
| Node.js  | 2339 ms |
| Python   | 20766 ms |

#### Prime Counting (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 262 ms |
| Rust     | 260 ms |
| Node.js  | 320 ms |
| C        | 548 ms |
| Go       | 594 ms |
| Python   | 5024 ms |

#### Ackermann(3, 11)

| Language | Time |
|----------|------|
| C        | 210 ms |
| Quadrate | 244 ms |
| Rust     | 255 ms |
| Go       | 1624 ms |
| Node.js  | 2883 ms |
| Python   | 44869 ms |

#### Popcount Sum (1..10M)

| Language | Time |
|----------|------|
| C        | 162 ms |
| Go       | 184 ms |
| Quadrate | 245 ms |
| Rust     | 267 ms |
| Node.js  | 272 ms |
| Python   | 26171 ms |

#### Tak(24, 16, 8)

| Language | Time |
|----------|------|
| C        | 4 ms |
| Quadrate | 6 ms |
| Rust     | 6 ms |
| Go       | 7 ms |
| Node.js  | 21 ms |
| Python   | 206 ms |

Results from an Intel Core i3-4030U @ 1.90GHz. Run `./benchmarks/run_benchmarks.sh` to reproduce.
