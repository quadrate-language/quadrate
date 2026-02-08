# Benchmarks

Performance benchmarks comparing Quadrate to other languages.

## Run

```bash
./benchmarks/run_benchmarks.sh
```

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
- Rust (rustc -O)
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
| C        | 76 ms |
| C#       | 78 ms |
| Go       | 80 ms |
| Rust     | 85 ms |
| Quadrate | 86 ms |
| Node.js  | 378 ms |
| Python   | 2579 ms |

### Recursive Fibonacci (n=35)

| Language | Time |
|----------|------|
| C        | 38 ms |
| Rust     | 52 ms |
| Quadrate | 56 ms |
| Go       | 88 ms |
| C#       | 108 ms |
| Node.js  | 258 ms |
| Python   | 2570 ms |

### String Concat (50K iterations)

| Language | Time |
|----------|------|
| Rust     | <1 ms |
| Node.js  | 8 ms |
| Python   | 99 ms |
| C        | 108 ms |
| Quadrate | 125 ms |
| Go       | 399 ms |
| C#       | 607 ms |

### Integer Compute Benchmarks

All integer-only functions qualify for native calling convention and achieve near-C performance.

#### Collatz Longest Chain (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 295 ms |
| Rust     | 303 ms |
| C        | 338 ms |
| C#       | 534 ms |
| Go       | 591 ms |
| Node.js  | 2347 ms |
| Python   | 20672 ms |

#### Prime Counting (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 246 ms |
| Rust     | 259 ms |
| Node.js  | 315 ms |
| C        | 546 ms |
| Go       | 596 ms |
| C#       | 642 ms |
| Python   | 5005 ms |

#### Ackermann(3, 11)

| Language | Time |
|----------|------|
| C        | 208 ms |
| Quadrate | 294 ms |
| Rust     | 295 ms |
| C#       | 1005 ms |
| Go       | 1624 ms |
| Node.js  | 2868 ms |
| Python   | 43879 ms |

#### Popcount Sum (1..10M)

| Language | Time |
|----------|------|
| C        | 164 ms |
| C#       | 206 ms |
| Go       | 209 ms |
| Quadrate | 245 ms |
| Rust     | 261 ms |
| Node.js  | 272 ms |
| Python   | 26106 ms |

#### Tak(24, 16, 8)

| Language | Time |
|----------|------|
| C        | 4 ms |
| Quadrate | 6 ms |
| Rust     | 7 ms |
| Go       | 7 ms |
| C#       | 11 ms |
| Node.js  | 20 ms |
| Python   | 207 ms |

#### Euler Totient Sum (1..10K)

| Language | Time |
|----------|------|
| Quadrate | 5341 ms |
| Rust     | 5593 ms |
| Node.js  | 5779 ms |
| C        | 9363 ms |
| C#       | 9675 ms |
| Go       | 9775 ms |
| Python   | 36244 ms |

#### Digit Sum (1..10M)

| Language | Time |
|----------|------|
| C        | 126 ms |
| Rust     | 141 ms |
| Quadrate | 156 ms |
| Go       | 161 ms |
| C#       | 184 ms |
| Node.js  | 888 ms |
| Python   | 9204 ms |

### Float Compute Benchmarks

Float functions with f64 parameters also qualify for native calling convention and achieve C-level performance.

#### Basel Problem (sum 1/i^2, 1..10M)

| Language | Time |
|----------|------|
| Quadrate | 74 ms |
| Rust     | 74 ms |
| Go       | 74 ms |
| C        | 75 ms |
| C#       | 75 ms |
| Node.js  | 79 ms |
| Python   | 2188 ms |

#### Leibniz Pi (10M terms)

| Language | Time |
|----------|------|
| C        | 74 ms |
| Quadrate | 74 ms |
| Rust     | 74 ms |
| Go       | 74 ms |
| C#       | 75 ms |
| Node.js  | 76 ms |
| Python   | 3032 ms |

#### Mandelbrot (200x200)

| Language | Time |
|----------|------|
| C        | 3 ms |
| Rust     | 3 ms |
| Go       | 3 ms |
| Node.js  | 7 ms |
| Quadrate | 23 ms |
| C#       | 39 ms |
| Python   | 176 ms |

Results from an Intel Core i3-4030U @ 1.90GHz. Run `./benchmarks/run_benchmarks.sh` to reproduce.
