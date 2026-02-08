use std::hint::black_box;
use std::time::Instant;

const RUNS: usize = 3;

fn benchmark_arithmetic(iterations: i64) -> i64 {
    let mut sum: i64 = 0;
    for i in 0..iterations {
        sum = ((sum + i) * i + 3) % 7;
    }
    sum
}

fn fib(n: i64) -> i64 {
    if n < 2 {
        n
    } else {
        fib(n - 1) + fib(n - 2)
    }
}

fn main() {
    println!("=== Rust Benchmarks ===");

    // Benchmark 1: Arithmetic loop
    let iterations = 10_000_000i64;
    let _ = benchmark_arithmetic(black_box(iterations)); // warmup
    let mut best = u128::MAX;
    let mut result: i64 = 0;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = benchmark_arithmetic(black_box(iterations));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Arithmetic loop ({} iterations):", iterations);
    println!("  Time: {} ms", best / 1_000_000);
    println!("  Result: {}", result);

    // Benchmark 2: Recursive fibonacci
    let n = 35i64;
    let _ = fib(black_box(n)); // warmup
    best = u128::MAX;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = fib(black_box(n));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Fibonacci (n={}):", n);
    println!("  Time: {} ms", best / 1_000_000);
    println!("  Result: {}", result);
}
