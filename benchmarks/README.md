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
- Node.js
- Python
- Quadrate (quadc -O3)

## Results

All compiled languages use -O3 (or equivalent) optimization. Quadrate compiles to native code via LLVM and achieves C-level performance for both integer and float functions thanks to the native calling convention optimization.

### Arithmetic Loop (10M iterations)

| Language | Time |
|----------|------|
| C        | 75 ms |
| Go       | 84 ms |
| Rust     | 85 ms |
| Quadrate | 87 ms |
| Node.js  | 378 ms |
| Python   | 2648 ms |

### Recursive Fibonacci (n=35)

| Language | Time |
|----------|------|
| C        | 39 ms |
| Quadrate | 51 ms |
| Rust     | 52 ms |
| Go       | 89 ms |
| Node.js  | 266 ms |
| Python   | 2707 ms |

### String Concat (50K iterations)

| Language | Time |
|----------|------|
| Rust     | <1 ms |
| Node.js  | 8 ms |
| Python   | 98 ms |
| C        | 109 ms |
| Quadrate | 118 ms |
| Go       | 396 ms |

### Integer Compute Benchmarks

All integer-only functions qualify for native calling convention and achieve near-C performance.

#### Collatz Longest Chain (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 297 ms |
| Rust     | 311 ms |
| C        | 340 ms |
| Go       | 575 ms |
| Node.js  | 2434 ms |
| Python   | 21125 ms |

#### Prime Counting (under 1M)

| Language | Time |
|----------|------|
| Quadrate | 260 ms |
| Rust     | 261 ms |
| Node.js  | 317 ms |
| C        | 556 ms |
| Go       | 608 ms |
| Python   | 5097 ms |

#### Ackermann(3, 11)

| Language | Time |
|----------|------|
| C        | 216 ms |
| Quadrate | 252 ms |
| Rust     | 302 ms |
| Go       | 1659 ms |
| Node.js  | 2913 ms |
| Python   | 45207 ms |

#### Popcount Sum (1..10M)

| Language | Time |
|----------|------|
| C        | 170 ms |
| Go       | 207 ms |
| Quadrate | 256 ms |
| Rust     | 270 ms |
| Node.js  | 274 ms |
| Python   | 27111 ms |

#### Tak(24, 16, 8)

| Language | Time |
|----------|------|
| C        | 4 ms |
| Quadrate | 6 ms |
| Rust     | 6 ms |
| Go       | 7 ms |
| Node.js  | 20 ms |
| Python   | 204 ms |

#### Euler Totient Sum (1..10K)

| Language | Time |
|----------|------|
| Quadrate | 5344 ms |
| Rust     | 5614 ms |
| Node.js  | 5851 ms |
| C        | 9401 ms |
| Go       | 9822 ms |
| Python   | 37515 ms |

#### Digit Sum (1..10M)

| Language | Time |
|----------|------|
| C        | 126 ms |
| Quadrate | 142 ms |
| Rust     | 143 ms |
| Go       | 163 ms |
| Node.js  | 897 ms |
| Python   | 9676 ms |

### Float Compute Benchmarks

Float functions with f64 parameters also qualify for native calling convention and achieve C-level performance.

#### Basel Problem (sum 1/i^2, 1..10M)

| Language | Time |
|----------|------|
| C        | 74 ms |
| Quadrate | 74 ms |
| Rust     | 74 ms |
| Go       | 74 ms |
| Node.js  | 80 ms |
| Python   | 2265 ms |

#### Leibniz Pi (10M terms)

| Language | Time |
|----------|------|
| C        | 74 ms |
| Quadrate | 74 ms |
| Rust     | 74 ms |
| Go       | 74 ms |
| Node.js  | 76 ms |
| Python   | 3181 ms |

#### Mandelbrot (200x200)

| Language | Time |
|----------|------|
| C        | 3 ms |
| Rust     | 3 ms |
| Go       | 3 ms |
| Node.js  | 7 ms |
| Quadrate | 11 ms |
| Python   | 176 ms |

Results from an Intel Core i3-4030U @ 1.90GHz. Run `./benchmarks/run_benchmarks.sh` to reproduce.
