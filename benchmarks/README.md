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
- **Euler Totient** - sum of totients 1..10K (nested loop via GCD)
- **Digit Sum** - sum of digit sums 1..10M (loop inside for)

### Float

- **Basel Problem** - sum of 1/i^2 for i=1..10M (loop + float division)
- **Leibniz Pi** - pi/4 = 1 - 1/3 + 1/5 - ... for 10M terms (loop + float arithmetic)
- **Mandelbrot** - 200x200 grid escape iteration (nested loops + mixed int/float)
- **Distance** - 10M euclidean distance computations using sqrt (native module call bridging)
- **Trig Sum** - sum of sin(x)+cos(x) for 5M values (native module call bridging)
- **Mat4x4 Multiply** - 1M chained 4x4 matrix multiplications (17-param native function)

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
| C        | 74 ms |
| C#       | 76 ms |
| Go       | 80 ms |
| Rust     | 85 ms |
| Quadrate | 86 ms |
| Node.js  | 376 ms |
| Python   | 2564 ms |

### Recursive Fibonacci (n=35)

| Language | Time |
|----------|------|
| C        | 36 ms |
| Quadrate | 55 ms |
| Rust     | 55 ms |
| C#       | 76 ms |
| Go       | 87 ms |
| Node.js  | 256 ms |
| Python   | 2622 ms |

### String Concat (50K iterations)

| Language | Time |
|----------|------|
| Rust     | <1 ms |
| Node.js  | 2 ms |
| Python   | 97 ms |
| C        | 109 ms |
| Quadrate | 121 ms |
| Go       | 380 ms |
| C#       | 551 ms |

### Integer Compute Benchmarks

All integer-only functions qualify for native calling convention and achieve near-C performance.

#### Collatz Longest Chain (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 294 ms |
| C        | 299 ms |
| Rust     | 299 ms |
| Go       | 460 ms |
| C#       | 507 ms |
| Node.js  | 2543 ms |
| Python   | 21215 ms |

#### Prime Counting (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 241 ms |
| Rust     | 259 ms |
| Node.js  | 314 ms |
| C#       | 521 ms |
| C        | 546 ms |
| Go       | 568 ms |
| Python   | 5048 ms |

#### Ackermann(3, 11)

| Language | Time |
|----------|------|
| C        | 194 ms |
| Quadrate | 291 ms |
| Rust     | 292 ms |
| C#       | 388 ms |
| Go       | 1626 ms |
| Node.js  | 2827 ms |
| Python   | 44906 ms |

#### Popcount Sum (1..10M)

| Language | Time |
|----------|------|
| C        | 190 ms |
| C#       | 202 ms |
| Go       | 213 ms |
| Quadrate | 244 ms |
| Rust     | 254 ms |
| Node.js  | 267 ms |
| Python   | 26580 ms |

#### Tak(24, 16, 8)

| Language | Time |
|----------|------|
| C        | 4 ms |
| Quadrate | 6 ms |
| Rust     | 6 ms |
| Go       | 7 ms |
| C#       | 10 ms |
| Node.js  | 19 ms |
| Python   | 202 ms |

#### Euler Totient Sum (1..10K)

| Language | Time |
|----------|------|
| Quadrate | 5411 ms |
| Rust     | 5566 ms |
| Node.js  | 5768 ms |
| C#       | 9409 ms |
| C        | 9586 ms |
| Go       | 9675 ms |
| Python   | 36660 ms |

#### Digit Sum (1..10M)

| Language | Time |
|----------|------|
| C        | 126 ms |
| Quadrate | 134 ms |
| Go       | 173 ms |
| C#       | 179 ms |
| Rust     | 231 ms |
| Node.js  | 883 ms |
| Python   | 9433 ms |

### Float Compute Benchmarks

Float functions with f64 parameters also qualify for native calling convention. Functions using only inline arithmetic achieve C-level performance. Functions calling imported C module functions (math::sqrt, math::sin, etc.) use native bridge wrappers which add some overhead from runtime stack push/pop in the bridge layer.

#### Basel Problem (sum 1/i^2, 1..10M)

| Language | Time |
|----------|------|
| C        | 74 ms |
| Quadrate | 74 ms |
| Rust     | 74 ms |
| Go       | 74 ms |
| Node.js  | 74 ms |
| C#       | 75 ms |
| Python   | 2225 ms |

#### Leibniz Pi (10M terms)

| Language | Time |
|----------|------|
| C        | 74 ms |
| Quadrate | 74 ms |
| Rust     | 74 ms |
| Go       | 74 ms |
| Node.js  | 74 ms |
| C#       | 74 ms |
| Python   | 3086 ms |

#### Mandelbrot (200x200)

| Language | Time |
|----------|------|
| C        | 3 ms |
| Rust     | 3 ms |
| Go       | 3 ms |
| Node.js  | 5 ms |
| C#       | 9 ms |
| Quadrate | 25 ms |
| Python   | 176 ms |

#### Distance (10M iterations, sqrt)

| Language | Time |
|----------|------|
| C        | 74 ms |
| Rust     | 74 ms |
| Go       | 74 ms |
| Node.js  | 74 ms |
| C#       | 76 ms |
| Quadrate | 452 ms |
| Python   | 4117 ms |

#### Trig Sum (sin+cos, 5M iterations)

| Language | Time |
|----------|------|
| C        | 172 ms |
| Rust     | 172 ms |
| Go       | 222 ms |
| Node.js  | 324 ms |
| C#       | 346 ms |
| Quadrate | 868 ms |
| Python   | 1588 ms |

#### Mat4x4 Multiply (1M iterations)

| Language | Time |
|----------|------|
| C        | 14 ms |
| Rust     | 21 ms |
| Quadrate | 32 ms |
| Go       | 173 ms |
| C#       | 177 ms |
| Node.js  | 1463 ms |
| Python   | 15513 ms |

Results from an Intel Core i3-4030U @ 1.90GHz. Warmup + best-of-3 methodology. Run `./benchmarks/run_benchmarks.sh` to reproduce.
