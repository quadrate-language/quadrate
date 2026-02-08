use std::time::Instant;

fn collatz_len(mut n: i64) -> i64 {
    let mut length: i64 = 0;
    while n > 1 {
        if n % 2 == 0 {
            n /= 2;
        } else {
            n = n * 3 + 1;
        }
        length += 1;
    }
    length
}

fn longest_collatz(limit: i64) -> i64 {
    let mut best: i64 = 0;
    for i in 1..limit {
        let length = collatz_len(i);
        if length > best {
            best = length;
        }
    }
    best
}

fn is_prime(n: i64) -> bool {
    if n < 2 { return false; }
    if n == 2 { return true; }
    if n % 2 == 0 { return false; }
    let mut d: i64 = 3;
    while d * d <= n {
        if n % d == 0 { return false; }
        d += 2;
    }
    true
}

fn count_primes(limit: i64) -> i64 {
    let mut count: i64 = 0;
    for i in 2..limit {
        if is_prime(i) { count += 1; }
    }
    count
}

fn ack(m: i64, n: i64) -> i64 {
    if m == 0 { return n + 1; }
    if n == 0 { return ack(m - 1, 1); }
    ack(m - 1, ack(m, n - 1))
}

fn popcount(mut n: i64) -> i64 {
    let mut count: i64 = 0;
    while n > 0 {
        count += n & 1;
        n >>= 1;
    }
    count
}

fn total_popcount(limit: i64) -> i64 {
    let mut total: i64 = 0;
    for i in 1..=limit {
        total += popcount(i);
    }
    total
}

fn tak(x: i64, y: i64, z: i64) -> i64 {
    if x <= y { return z; }
    tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y))
}

fn main() {
    println!("=== Rust Compute Benchmarks ===");

    // Benchmark 1: Collatz conjecture
    let limit: i64 = 1000000;
    let start = Instant::now();
    let result = longest_collatz(limit);
    let elapsed = start.elapsed();
    println!("Collatz longest chain under {}:", limit);
    println!("  Time: {} ms", elapsed.as_millis());
    println!("  Result: {}", result);

    // Benchmark 2: Prime counting
    let limit: i64 = 1000000;
    let start = Instant::now();
    let result = count_primes(limit);
    let elapsed = start.elapsed();
    println!("Prime count under {}:", limit);
    println!("  Time: {} ms", elapsed.as_millis());
    println!("  Result: {}", result);

    // Benchmark 3: Ackermann
    let (m, n) = (3i64, 11i64);
    let start = Instant::now();
    let result = ack(m, n);
    let elapsed = start.elapsed();
    println!("Ackermann({}, {}):", m, n);
    println!("  Time: {} ms", elapsed.as_millis());
    println!("  Result: {}", result);

    // Benchmark 4: Popcount
    let limit: i64 = 10000000;
    let start = Instant::now();
    let result = total_popcount(limit);
    let elapsed = start.elapsed();
    println!("Popcount sum 1..{}:", limit);
    println!("  Time: {} ms", elapsed.as_millis());
    println!("  Result: {}", result);

    // Benchmark 5: Takeuchi
    let (x, y, z) = (24i64, 16i64, 8i64);
    let start = Instant::now();
    let result = tak(x, y, z);
    let elapsed = start.elapsed();
    println!("Tak({}, {}, {}):", x, y, z);
    println!("  Time: {} ms", elapsed.as_millis());
    println!("  Result: {}", result);
}
