function benchmarkArithmetic(iterations) {
    let sum = 0;
    for (let i = 0; i < iterations; i++) {
        sum = ((sum + i) * i + 3) % 7;
    }
    return sum;
}

function fib(n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

function main() {
    console.log("=== Node.js Benchmarks ===");

    // Benchmark 1: Arithmetic loop
    const iterations = 10000000;
    let start = process.hrtime.bigint();
    let result = benchmarkArithmetic(iterations);
    let elapsed = process.hrtime.bigint() - start;

    console.log(`Arithmetic loop (${iterations} iterations):`);
    console.log(`  Time: ${elapsed / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 2: Recursive fibonacci
    const n = 35;
    start = process.hrtime.bigint();
    result = fib(n);
    elapsed = process.hrtime.bigint() - start;

    console.log(`Fibonacci (n=${n}):`);
    console.log(`  Time: ${elapsed / 1000000n} ms`);
    console.log(`  Result: ${result}`);
}

main();
