const RUNS = 3;

function benchmarkArithmetic(iterations) {
    let sum = 0;
    for (let i = 0; i < iterations; i++) {
        sum = ((sum + i) * i + 3) % 7;
    }
    return sum;
}

function fib(n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

function main() {
    console.log("=== Node.js Benchmarks ===");

    // Benchmark 1: Arithmetic loop
    const iterations = 10000000;
    benchmarkArithmetic(iterations); // warmup
    let best = BigInt(Number.MAX_SAFE_INTEGER);
    let result;
    for (let run = 0; run < RUNS; run++) {
        const start = process.hrtime.bigint();
        const r = benchmarkArithmetic(iterations);
        const elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    console.log(`Arithmetic loop (${iterations} iterations):`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 2: Recursive fibonacci
    const n = 35;
    fib(n); // warmup
    best = BigInt(Number.MAX_SAFE_INTEGER);
    for (let run = 0; run < RUNS; run++) {
        const start = process.hrtime.bigint();
        const r = fib(n);
        const elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    console.log(`Fibonacci (n=${n}):`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${result}`);
}

main();
