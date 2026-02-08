const RUNS = 3;

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

function gcd(a, b) {
    while (b !== 0) {
        const t = a % b;
        a = b;
        b = t;
    }
    return a;
}

function eulerTotient(n) {
    let count = 0;
    for (let i = 1; i < n; i++) {
        if (gcd(i, n) === 1) count++;
    }
    return count;
}

function sumTotients(limit) {
    let total = 0;
    for (let i = 1; i <= limit; i++) {
        total += eulerTotient(i);
    }
    return total;
}

function digitSum(n) {
    let sum = 0;
    while (n > 0) {
        sum += n % 10;
        n = Math.floor(n / 10);
    }
    return sum;
}

function totalDigitSum(limit) {
    let total = 0;
    for (let i = 1; i <= limit; i++) {
        total += digitSum(i);
    }
    return total;
}

function main() {
    console.log("=== Node.js Compute Benchmarks ===");

    // Benchmark 1: Collatz conjecture
    const limit1 = 1000000;
    longestCollatz(limit1); // warmup
    let best = BigInt(Number.MAX_SAFE_INTEGER);
    let result;
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let r = longestCollatz(limit1);
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    console.log(`Collatz longest chain under ${limit1}:`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 2: Prime counting
    const limit2 = 1000000;
    countPrimes(limit2); // warmup
    best = BigInt(Number.MAX_SAFE_INTEGER);
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let r = countPrimes(limit2);
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    console.log(`Prime count under ${limit2}:`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 3: Ackermann
    const m = 3, n = 11;
    ack(m, n); // warmup
    best = BigInt(Number.MAX_SAFE_INTEGER);
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let r = ack(m, n);
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    console.log(`Ackermann(${m}, ${n}):`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 4: Popcount
    const limit3 = 10000000;
    totalPopcount(limit3); // warmup
    best = BigInt(Number.MAX_SAFE_INTEGER);
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let r = totalPopcount(limit3);
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    console.log(`Popcount sum 1..${limit3}:`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 5: Takeuchi
    const x = 24, y = 16, z = 8;
    tak(x, y, z); // warmup
    best = BigInt(Number.MAX_SAFE_INTEGER);
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let r = tak(x, y, z);
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    console.log(`Tak(${x}, ${y}, ${z}):`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 6: Euler totient sum
    const limit6 = 10000;
    sumTotients(limit6); // warmup
    best = BigInt(Number.MAX_SAFE_INTEGER);
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let r = sumTotients(limit6);
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    console.log(`Euler totient sum(1..${limit6}):`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${result}`);

    // Benchmark 7: Digit sum
    const limit7 = 10000000;
    totalDigitSum(limit7); // warmup
    best = BigInt(Number.MAX_SAFE_INTEGER);
    for (let run = 0; run < RUNS; run++) {
        let start = process.hrtime.bigint();
        let r = totalDigitSum(limit7);
        let elapsed = process.hrtime.bigint() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    console.log(`Digit sum(1..${limit7}):`);
    console.log(`  Time: ${best / 1000000n} ms`);
    console.log(`  Result: ${result}`);
}

main();
