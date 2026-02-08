use std::hint::black_box;
use std::time::Instant;

const RUNS: usize = 3;

fn sum_inv_squares(n: i64) -> f64 {
    let mut sum: f64 = 0.0;
    for i in 1..=n {
        let fi = i as f64;
        sum += 1.0 / (fi * fi);
    }
    sum
}

fn leibniz_pi(n: i64) -> f64 {
    let mut sum: f64 = 0.0;
    let mut sign: f64 = 1.0;
    for i in 1..=n {
        let denom = (i * 2 - 1) as f64;
        sum += sign / denom;
        sign = -sign;
    }
    sum * 4.0
}

fn mandelbrot_iter(cr: f64, ci: f64, max_iter: i64) -> i64 {
    let mut zr: f64 = 0.0;
    let mut zi: f64 = 0.0;
    for i in 0..max_iter {
        let tr = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = tr;
        if zr * zr + zi * zi > 4.0 {
            return i;
        }
    }
    max_iter
}

fn main() {
    println!("=== Rust Float Benchmarks ===");

    // Benchmark 1: Basel problem
    let n: i64 = 10000000;
    let _ = sum_inv_squares(black_box(n)); // warmup
    let mut best: u128 = u128::MAX;
    let mut result: f64 = 0.0;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = sum_inv_squares(black_box(n));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Basel sum(1/i^2, 1..{}):", n);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {:.10}", result);

    // Benchmark 2: Leibniz pi
    let n: i64 = 10000000;
    let _ = leibniz_pi(black_box(n)); // warmup
    best = u128::MAX;
    for _ in 0..RUNS {
        let start = Instant::now();
        let r = leibniz_pi(black_box(n));
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; result = r; }
    }
    println!("Leibniz pi({} terms):", n);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {:.10}", result);

    // Benchmark 3: Mandelbrot grid
    let size: i64 = 200;
    // warmup
    {
        let mut t: i64 = 0;
        for y in 0..size {
            for x in 0..size {
                let cr = (x as f64 - 100.0) / 50.0;
                let ci = (y as f64 - 100.0) / 50.0;
                t += mandelbrot_iter(cr, ci, 100);
            }
        }
        let _ = t;
    }
    best = u128::MAX;
    let mut total: i64 = 0;
    for _ in 0..RUNS {
        let start = Instant::now();
        let mut t: i64 = 0;
        for y in 0..size {
            for x in 0..size {
                let cr = (x as f64 - 100.0) / 50.0;
                let ci = (y as f64 - 100.0) / 50.0;
                t += mandelbrot_iter(cr, ci, 100);
            }
        }
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best { best = elapsed; total = t; }
    }
    println!("Mandelbrot {}x{}:", size, size);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {}", total);
}
