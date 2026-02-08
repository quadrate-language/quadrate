# Benchmarks

Performance benchmarks comparing Quadrate to other languages.

## Run

```bash
./benchmarks/run_benchmarks.sh
```

## Methodology

- All compiled languages use maximum optimization (`-O3` or equivalent)
- Each benchmark runs a **warmup** pass, then takes the **best of 3** timed runs
- Rust uses `std::hint::black_box()` to prevent compile-time constant folding
- JIT languages (Node.js, C#) benefit from warmup for JIT compilation

## Tests

### Integer

- **Arithmetic Loop** - 10M iterations with arithmetic operations
- **Recursive Fibonacci** - fib(35) with naive recursion
- **String Concat** - 50K iterations of single-char append
- **Collatz Conjecture** - longest chain under 1M
- **Prime Counting** - trial division under 1M
- **Ackermann(3,11)** - deep recursion stress test
- **Popcount** - total set bits 1..10M
- **Tak(24,16,8)** - Takeuchi function, triple recursion
- **Euler Totient** - sum of totients 1..10K (nested while via GCD)
- **Digit Sum** - sum of digit sums 1..10M (while inside for)

### Float

- **Basel Problem** - sum of 1/i^2 for i=1..10M (loop + float division)
- **Leibniz Pi** - pi/4 = 1 - 1/3 + 1/5 - ... for 10M terms (loop + float arithmetic)
- **Mandelbrot** - 200x200 grid escape iteration (nested loops + mixed int/float)

## Languages

- C (gcc -O3)
- Rust (rustc -O + black_box)
- Go
- C# (dotnet -c Release)
- Node.js
- Python
- Quadrate (quadc -O3)

## Results

All compiled languages use -O3 (or equivalent) optimization. Quadrate compiles to native code via LLVM and achieves C-level performance for both integer and float functions thanks to the native calling convention optimization.

### Arithmetic Loop (10M iterations)

| Language | Time |
|----------|------|
| C        | 75 ms |
| C#       | 76 ms |
| Go       | 80 ms |
| Rust     | 85 ms |
| Quadrate | 86 ms |
| Node.js  | 376 ms |
| Python   | 2571 ms |

### Recursive Fibonacci (n=35)

| Language | Time |
|----------|------|
| C        | 38 ms |
| Quadrate | 55 ms |
| Rust     | 55 ms |
| C#       | 75 ms |
| Go       | 87 ms |
| Node.js  | 257 ms |
| Python   | 2540 ms |

### String Concat (50K iterations)

| Language | Time |
|----------|------|
| Rust     | <1 ms |
| Node.js  | 2 ms |
| Python   | 97 ms |
| C        | 108 ms |
| Quadrate | 125 ms |
| Go       | 405 ms |
| C#       | 534 ms |

### Integer Compute Benchmarks

All integer-only functions qualify for native calling convention and achieve near-C performance.

#### Collatz Longest Chain (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 293 ms |
| Rust     | 298 ms |
| C        | 300 ms |
| Go       | 455 ms |
| C#       | 506 ms |
| Node.js  | 2538 ms |
| Python   | 20550 ms |

#### Prime Counting (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 257 ms |
| Rust     | 260 ms |
| Node.js  | 310 ms |
| C#       | 519 ms |
| C        | 544 ms |
| Go       | 566 ms |
| Python   | 5044 ms |

#### Ackermann(3, 11)

| Language | Time |
|----------|------|
| C        | 192 ms |
| Rust     | 290 ms |
| Quadrate | 291 ms |
| C#       | 860 ms |
| Go       | 1610 ms |
| Node.js  | 2814 ms |
| Python   | 44446 ms |

#### Popcount Sum (1..10M)

| Language | Time |
|----------|------|
| C        | 187 ms |
| C#       | 196 ms |
| Go       | 215 ms |
| Rust     | 253 ms |
| Node.js  | 267 ms |
| Quadrate | 273 ms |
| Python   | 26885 ms |

#### Tak(24, 16, 8)

| Language | Time |
|----------|------|
| C        | 4 ms |
| Quadrate | 6 ms |
| Rust     | 6 ms |
| Go       | 7 ms |
| C#       | 10 ms |
| Node.js  | 21 ms |
| Python   | 204 ms |

#### Euler Totient Sum (1..10K)

| Language | Time |
|----------|------|
| Quadrate | 5407 ms |
| Rust     | 5553 ms |
| Node.js  | 5777 ms |
| C#       | 9437 ms |
| C        | 9577 ms |
| Go       | 9649 ms |
| Python   | 36589 ms |

#### Digit Sum (1..10M)

| Language | Time |
|----------|------|
| C        | 130 ms |
| Quadrate | 140 ms |
| Go       | 169 ms |
| C#       | 176 ms |
| Rust     | 231 ms |
| Node.js  | 886 ms |
| Python   | 9483 ms |

### Float Compute Benchmarks

Float functions with f64 parameters also qualify for native calling convention and achieve C-level performance.

#### Basel Problem (sum 1/i^2, 1..10M)

| Language | Time |
|----------|------|
| C        | 74 ms |
| Quadrate | 74 ms |
| Rust     | 74 ms |
| Go       | 74 ms |
| Node.js  | 74 ms |
| C#       | 75 ms |
| Python   | 2199 ms |

#### Leibniz Pi (10M terms)

| Language | Time |
|----------|------|
| C        | 74 ms |
| Quadrate | 74 ms |
| Rust     | 74 ms |
| Go       | 74 ms |
| C#       | 74 ms |
| Node.js  | 74 ms |
| Python   | 3100 ms |

#### Mandelbrot (200x200)

| Language | Time |
|----------|------|
| C        | 3 ms |
| Rust     | 3 ms |
| Go       | 3 ms |
| Node.js  | 3 ms |
| C#       | 9 ms |
| Quadrate | 24 ms |
| Python   | 178 ms |

Results from an Intel Core i3-4030U @ 1.90GHz. Warmup + best-of-3 methodology. Run `./benchmarks/run_benchmarks.sh` to reproduce.
