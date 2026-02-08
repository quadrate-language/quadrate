function collatzLen(n) {
    let length = 0;
    while (n > 1) {
        if (n % 2 === 0) {
            n = Math.floor(n / 2);
        } else {
            n = n * 3 + 1;
        }
        length++;
    }
    return length;
}

function longestCollatz(limit) {
    let best = 0;
    for (let i = 1; i < limit; i++) {
        const length = collatzLen(i);
        if (length > best) {
            best = length;
        }
    }
    return best;
}

function isPrime(n) {
    if (n < 2) return false;
    if (n === 2) return true;
    if (n % 2 === 0) return false;
    for (let d = 3; d * d <= n; d += 2) {
        if (n % d === 0) return false;
    }
    return true;
}

function countPrimes(limit) {
    let count = 0;
    for (let i = 2; i < limit; i++) {
        if (isPrime(i)) count++;
    }
    return count;
}

function ack(m, n) {
    if (m === 0) return n + 1;
    if (n === 0) return ack(m - 1, 1);
    return ack(m - 1, ack(m, n - 1));
}

function popcount(n) {
    let count = 0;
    while (n > 0) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

function totalPopcount(limit) {
    let total = 0;
    for (let i = 1; i <= limit; i++) {
        total += popcount(i);
    }
    return total;
}

function tak(x, y, z) {
    if (x <= y) return z;
    return tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y));
}

function main() {
    console.log("=== Node.js Compute Benchmarks ===");

    // Benchmark 1: Collatz conjecture
    const limit1 = 1000000;
    let start = process.hrtime.bigint();
    let result = longestCollatz(limit1);
    let elapsed = process.hrtime.bigint() - start;
    console.log(`Collatz longest chain under ${limit1}:`);
    console.log(`  Time: ${elapsed / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 2: Prime counting
    const limit2 = 1000000;
    start = process.hrtime.bigint();
    result = countPrimes(limit2);
    elapsed = process.hrtime.bigint() - start;
    console.log(`Prime count under ${limit2}:`);
    console.log(`  Time: ${elapsed / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 3: Ackermann
    const m = 3, n = 11;
    start = process.hrtime.bigint();
    result = ack(m, n);
    elapsed = process.hrtime.bigint() - start;
    console.log(`Ackermann(${m}, ${n}):`);
    console.log(`  Time: ${elapsed / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 4: Popcount
    const limit3 = 10000000;
    start = process.hrtime.bigint();
    result = totalPopcount(limit3);
    elapsed = process.hrtime.bigint() - start;
    console.log(`Popcount sum 1..${limit3}:`);
    console.log(`  Time: ${elapsed / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 5: Takeuchi
    const x = 24, y = 16, z = 8;
    start = process.hrtime.bigint();
    result = tak(x, y, z);
    elapsed = process.hrtime.bigint() - start;
    console.log(`Tak(${x}, ${y}, ${z}):`);
    console.log(`  Time: ${elapsed / 1000000n} ms`);
    console.log(`  Result: ${result}`);
}

main();
