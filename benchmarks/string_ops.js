console.log("=== Node.js String Benchmarks ===");
const iterations = 50000;
const RUNS = 3;

// Warmup
let s = "";
for (let i = 0; i < iterations; i++) {
    s += "a";
}
s = "";

// Best of 3
let best = BigInt("9999999999999999");
let finalLen = 0;
for (let run = 0; run < RUNS; run++) {
    s = "";
    const start = process.hrtime.bigint();
    for (let i = 0; i < iterations; i++) {
        s += "a";
    }
    const elapsed = process.hrtime.bigint() - start;
    if (elapsed < best) {
        best = elapsed;
    }
    finalLen = s.length;
}

console.log(`String Concat (${iterations} iterations):`);
console.log(`  Time: ${best / 1000000n} ms`);
console.log(`  Final Length: ${finalLen}`);
