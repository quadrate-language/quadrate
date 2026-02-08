console.log("=== Node.js String Benchmarks ===");
const iterations = 50000;

const start = process.hrtime.bigint();
let s = "";
for (let i = 0; i < iterations; i++) {
    s += "a";
}
const elapsed = process.hrtime.bigint() - start;

console.log(`String Concat (${iterations} iterations):`);
console.log(`  Time: ${elapsed / 1000000n} ms`);
console.log(`  Final Length: ${s.length}`);
