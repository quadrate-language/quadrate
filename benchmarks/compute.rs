use std::hint::black_box;
use std::time::Instant;

const RUNS: usize = 3;

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

fn gcd(mut a: i64, mut b: i64) -> i64 {
    while b != 0 {
        let t = a % b;
        a = b;
        b = t;
    }
    a
}

fn euler_totient(n: i64) -> i64 {
    let mut count: i64 = 0;
    for i in 1..n {
        if gcd(i, n) == 1 { count += 1; }
    }
    count
}

fn sum_totients(limit: i64) -> i64 {
    let mut total: i64 = 0;
    for i in 1..=limit {
        total += euler_totient(i);
    }
    total
}

fn digit_sum(mut n: i64) -> i64 {
    let mut sum: i64 = 0;
    while n > 0 {
        sum += n % 10;
        n /= 10;
    }
    sum
}

fn total_digit_sum(limit: i64) -> i64 {
    let mut total: i64 = 0;
    for i in 1..=limit {
        total += digit_sum(i);
    }
    total
}

fn main() {
    println!("=== Rust Compute Benchmarks ===");

    // Benchmark 1: Collatz conjecture
    let limit: i64 = 1000000;
    longest_collatz(black_box(limit)); // warmup
    let mut best: u128 = u128::MAX;
    let mut result: i64 = 0;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = longest_collatz(black_box(limit));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Collatz longest chain under {}:", limit);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {}", result);

    // Benchmark 2: Prime counting
    let limit: i64 = 1000000;
    count_primes(black_box(limit)); // warmup
    best = u128::MAX;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = count_primes(black_box(limit));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Prime count under {}:", limit);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {}", result);

    // Benchmark 3: Ackermann
    let (m, n) = (3i64, 11i64);
    ack(black_box(m), black_box(n)); // warmup
    best = u128::MAX;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = ack(black_box(m), black_box(n));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Ackermann({}, {}):", m, n);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {}", result);

    // Benchmark 4: Popcount
    let limit: i64 = 10000000;
    total_popcount(black_box(limit)); // warmup
    best = u128::MAX;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = total_popcount(black_box(limit));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Popcount sum 1..{}:", limit);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {}", result);

    // Benchmark 5: Takeuchi
    let (x, y, z) = (24i64, 16i64, 8i64);
    tak(black_box(x), black_box(y), black_box(z)); // warmup
    best = u128::MAX;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = tak(black_box(x), black_box(y), black_box(z));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Tak({}, {}, {}):", x, y, z);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {}", result);

    // Benchmark 6: Euler totient sum
    let limit: i64 = 10000;
    sum_totients(black_box(limit)); // warmup
    best = u128::MAX;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = sum_totients(black_box(limit));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Euler totient sum(1..{}):", limit);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {}", result);

    // Benchmark 7: Digit sum
    let limit: i64 = 10000000;
    total_digit_sum(black_box(limit)); // warmup
    best = u128::MAX;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = total_digit_sum(black_box(limit));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Digit sum(1..{}):", limit);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {}", result);
}
