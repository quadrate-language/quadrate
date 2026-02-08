use std::time::Instant;

const RUNS: usize = 3;

fn main() {
    println!("=== Rust String Benchmarks ===");
    let iterations = 50000;

    // Warmup
    let mut s = String::new();
    for _ in 0..iterations {
        s.push('a');
    }
    drop(s);

    // Best of 3
    let mut best = u128::MAX;
    let mut final_len = 0;
    for _ in 0..RUNS {
        let mut s = String::new();
        let start = Instant::now();
        for _ in 0..iterations {
            s.push('a');
        }
        let elapsed = start.elapsed().as_millis();
        if elapsed < best {
            best = elapsed;
        }
        final_len = s.len();
    }

    println!("String Concat ({} iterations):", iterations);
    println!("  Time: {} ms", best);
    println!("  Final Length: {}", final_len);
}
