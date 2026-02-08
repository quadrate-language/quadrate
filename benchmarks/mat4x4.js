const RUNS = 3;
const ITERS = 1000000;

function mat4Mul(a, b) {
    const r = new Float64Array(16);
    for (let i = 0; i < 4; i++) {
        for (let j = 0; j < 4; j++) {
            let sum = 0;
            for (let k = 0; k < 4; k++) {
                sum += a[i * 4 + k] * b[k * 4 + j];
            }
            r[i * 4 + j] = sum;
        }
    }
    return r;
}

function identity() {
    const m = new Float64Array(16);
    m[0] = 1; m[5] = 1; m[10] = 1; m[15] = 1;
    return m;
}

console.log("=== Node.js Mat4x4 Benchmarks ===");

const b = new Float64Array([
    0.8660254037844387, -0.5, 0.0, 0.0,
    0.5, 0.8660254037844387, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
]);

// Warmup
let a = identity();
for (let i = 0; i < ITERS; i++) a = mat4Mul(a, b);

// Best of 3
let best = Infinity;
let checksum = 0;
for (let run = 0; run < RUNS; run++) {
    a = identity();
    const start = process.hrtime.bigint();
    for (let i = 0; i < ITERS; i++) a = mat4Mul(a, b);
    const elapsed = Number(process.hrtime.bigint() - start);
    if (elapsed < best) {
        best = elapsed;
        checksum = 0;
        for (let i = 0; i < 16; i++) checksum += a[i];
    }
}
console.log(`Mat4x4 multiply (${ITERS} iterations):`);
console.log(`  Time: ${Math.floor(best / 1000000)} ms`);
console.log(`  Result: ${checksum.toFixed(10)}`);
