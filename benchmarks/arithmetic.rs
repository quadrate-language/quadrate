use std::time::Instant;

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
    let start = Instant::now();
    let result = benchmark_arithmetic(iterations);
    let elapsed = start.elapsed();

    println!("Arithmetic loop ({} iterations):", iterations);
    println!("  Time: {} ms", elapsed.as_millis());
    println!("  Result: {}", result);

    // Benchmark 2: Recursive fibonacci
    let n = 35i64;
    let start = Instant::now();
    let result = fib(n);
    let elapsed = start.elapsed();

    println!("Fibonacci (n={}):", n);
    println!("  Time: {} ms", elapsed.as_millis());
    println!("  Result: {}", result);
}
