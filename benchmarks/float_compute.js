function sumInvSquares(n) {
    let sum = 0.0;
    for (let i = 1; i <= n; i++) {
        sum += 1.0 / (i * i);
    }
    return sum;
}

function leibnizPi(n) {
    let sum = 0.0;
    let sign = 1.0;
    for (let i = 1; i <= n; i++) {
        const denom = i * 2 - 1;
        sum += sign / denom;
        sign = -sign;
    }
    return sum * 4.0;
}

function mandelbrotIter(cr, ci, maxIter) {
    let zr = 0.0, zi = 0.0;
    for (let i = 0; i < maxIter; i++) {
        const tr = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = tr;
        if (zr * zr + zi * zi > 4.0) {
            return i;
        }
    }
    return maxIter;
}

function main() {
    console.log("=== Node.js Float Benchmarks ===");

    // Benchmark 1: Basel problem
    const n1 = 10000000;
    let start = process.hrtime.bigint();
    let resultF = sumInvSquares(n1);
    let elapsed = process.hrtime.bigint() - start;
    console.log(`Basel sum(1/i^2, 1..${n1}):`);
    console.log(`  Time: ${elapsed / 1000000n} ms`);
    console.log(`  Result: ${resultF.toFixed(10)}`);

    // Benchmark 2: Leibniz pi
    const n2 = 10000000;
    start = process.hrtime.bigint();
    resultF = leibnizPi(n2);
    elapsed = process.hrtime.bigint() - start;
    console.log(`Leibniz pi(${n2} terms):`);
    console.log(`  Time: ${elapsed / 1000000n} ms`);
    console.log(`  Result: ${resultF.toFixed(10)}`);

    // Benchmark 3: Mandelbrot grid
    const size = 200;
    start = process.hrtime.bigint();
    let total = 0;
    for (let y = 0; y < size; y++) {
        for (let x = 0; x < size; x++) {
            const cr = (x - 100.0) / 50.0;
            const ci = (y - 100.0) / 50.0;
            total += mandelbrotIter(cr, ci, 100);
        }
    }
    elapsed = process.hrtime.bigint() - start;
    console.log(`Mandelbrot ${size}x${size}:`);
    console.log(`  Time: ${elapsed / 1000000n} ms`);
    console.log(`  Result: ${total}`);
}

main();
