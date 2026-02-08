use std::hint::black_box;
use std::time::Instant;

const RUNS: usize = 3;
const ITERS: usize = 1000000;

type Mat4 = [[f64; 4]; 4];

fn mat4_mul(a: &Mat4, b: &Mat4) -> Mat4 {
    let mut r = [[0.0f64; 4]; 4];
    for i in 0..4 {
        for j in 0..4 {
            for k in 0..4 {
                r[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    r
}

fn main() {
    println!("=== Rust Mat4x4 Benchmarks ===");

    let b: Mat4 = [
        [0.8660254037844387, -0.5, 0.0, 0.0],
        [0.5, 0.8660254037844387, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ];

    // Warmup
    let mut a: Mat4 = [[0.0; 4]; 4];
    a[0][0] = 1.0; a[1][1] = 1.0; a[2][2] = 1.0; a[3][3] = 1.0;
    for _ in 0..ITERS {
        a = mat4_mul(&a, black_box(&b));
    }
    let _ = black_box(a);

    // Best of 3
    let mut best = u128::MAX;
    let mut checksum: f64 = 0.0;
    for _ in 0..RUNS {
        a = [[0.0; 4]; 4];
        a[0][0] = 1.0; a[1][1] = 1.0; a[2][2] = 1.0; a[3][3] = 1.0;
        let start = Instant::now();
        for _ in 0..ITERS {
            a = mat4_mul(&a, black_box(&b));
        }
        let elapsed = start.elapsed().as_nanos();
        if elapsed < best {
            best = elapsed;
            checksum = 0.0;
            for i in 0..4 {
                for j in 0..4 {
                    checksum += a[i][j];
                }
            }
        }
    }
    println!("Mat4x4 multiply ({} iterations):", ITERS);
    println!("  Time: {} ms", best / 1000000);
    println!("  Result: {:.10}", checksum);
}
