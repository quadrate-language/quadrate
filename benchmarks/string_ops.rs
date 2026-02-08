use std::time::Instant;

fn main() {
    println!("=== Rust String Benchmarks ===");
    let iterations = 50000;

    let start = Instant::now();
    let mut s = String::new();
    for _ in 0..iterations {
        s.push('a');
    }
    let elapsed = start.elapsed();

    println!("String Concat ({} iterations):", iterations);
    println!("  Time: {} ms", elapsed.as_millis());
    println!("  Final Length: {}", s.len());
}
