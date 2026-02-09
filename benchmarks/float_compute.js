const RUNS = 3;

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

function distance(x1, y1, x2, y2) {
    const dx = x2 - x1;
    const dy = y2 - y1;
    return Math.sqrt(dx * dx + dy * dy);
}

function trigSum(n) {
    let sum = 0.0;
    for (let i = 0; i < n; i++) {
        const x = i * 0.001;
        sum += Math.sin(x) + Math.cos(x);
    }
    return sum;
}

function main() {
    console.log("=== Node.js Float Benchmarks ===");

    // Benchmark 1: Basel problem
    const n1 = 10000000;
    sumInvSquares(n1); // warmup
    let best = BigInt(Number.MAX_SAFE_INTEGER);
    let resultF;
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let r = sumInvSquares(n1);
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; resultF = r; }
    }
    console.log(`Basel sum(1/i^2, 1..${n1}):`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${resultF.toFixed(10)}`);

    // Benchmark 2: Leibniz pi
    const n2 = 10000000;
    leibnizPi(n2); // warmup
    best = BigInt(Number.MAX_SAFE_INTEGER);
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let r = leibnizPi(n2);
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; resultF = r; }
    }
    console.log(`Leibniz pi(${n2} terms):`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${resultF.toFixed(10)}`);

    // Benchmark 3: Mandelbrot grid
    const size = 200;
    // warmup
    {
        let t = 0;
        for (let y = 0; y < size; y++)
            for (let x = 0; x < size; x++)
                t += mandelbrotIter((x - 100.0) / 50.0, (y - 100.0) / 50.0, 100);
    }
    best = BigInt(Number.MAX_SAFE_INTEGER);
    let total;
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let t = 0;
        for (let y = 0; y < size; y++) {
            for (let x = 0; x < size; x++) {
                const cr = (x - 100.0) / 50.0;
                const ci = (y - 100.0) / 50.0;
                t += mandelbrotIter(cr, ci, 100);
            }
        }
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; total = t; }
    }
    console.log(`Mandelbrot ${size}x${size}:`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${total}`);

    // Benchmark 4: Distance computation
    const n4 = 10000000;
    // warmup
    {
        let s = 0.0;
        for (let i = 0; i < n4; i++) {
            const x = i * 0.001;
            s += distance(x, 0.0, 0.0, x);
        }
    }
    best = BigInt(Number.MAX_SAFE_INTEGER);
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let s = 0.0;
        for (let i = 0; i < n4; i++) {
            const x = i * 0.001;
            s += distance(x, 0.0, 0.0, x);
        }
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; resultF = s; }
    }
    console.log(`Distance (${n4} iterations):`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${resultF.toFixed(10)}`);

    // Benchmark 5: Trig computation
    const n5 = 5000000;
    trigSum(n5); // warmup
    best = BigInt(Number.MAX_SAFE_INTEGER);
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let r = trigSum(n5);
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; resultF = r; }
    }
    console.log(`Trig sum(${n5}):`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${resultF.toFixed(10)}`);
}

main();
