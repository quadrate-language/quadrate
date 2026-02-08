using System.Diagnostics;

long CollatzLen(long n) {
    long len = 0;
    while (n > 1) {
        if (n % 2 == 0) n /= 2;
        else n = n * 3 + 1;
        len++;
    }
    return len;
}

long LongestCollatz(long limit) {
    long best = 0;
    for (long i = 1; i < limit; i++) {
        long len = CollatzLen(i);
        if (len > best) best = len;
    }
    return best;
}

bool IsPrime(long n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (long d = 3; d * d <= n; d += 2) {
        if (n % d == 0) return false;
    }
    return true;
}

long CountPrimes(long limit) {
    long count = 0;
    for (long i = 2; i < limit; i++) {
        if (IsPrime(i)) count++;
    }
    return count;
}

long Ack(long m, long n) {
    if (m == 0) return n + 1;
    if (n == 0) return Ack(m - 1, 1);
    return Ack(m - 1, Ack(m, n - 1));
}

long Popcount(long n) {
    long count = 0;
    while (n > 0) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

long TotalPopcount(long limit) {
    long total = 0;
    for (long i = 1; i <= limit; i++) {
        total += Popcount(i);
    }
    return total;
}

long Tak(long x, long y, long z) {
    if (x <= y) return z;
    return Tak(Tak(x - 1, y, z), Tak(y - 1, z, x), Tak(z - 1, x, y));
}

long Gcd(long a, long b) {
    while (b != 0) {
        long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

long EulerTotient(long n) {
    long count = 0;
    for (long i = 1; i < n; i++) {
        if (Gcd(i, n) == 1) count++;
    }
    return count;
}

long SumTotients(long limit) {
    long total = 0;
    for (long i = 1; i <= limit; i++) {
        total += EulerTotient(i);
    }
    return total;
}

long DigitSum(long n) {
    long sum = 0;
    while (n > 0) {
        sum += n % 10;
        n /= 10;
    }
    return sum;
}

long TotalDigitSum(long limit) {
    long total = 0;
    for (long i = 1; i <= limit; i++) {
        total += DigitSum(i);
    }
    return total;
}

Console.WriteLine("=== C# Compute Benchmarks ===");

// Benchmark 1: Collatz conjecture
long limit = 1000000;
var sw = Stopwatch.StartNew();
long result = LongestCollatz(limit);
sw.Stop();
Console.WriteLine($"Collatz longest chain under {limit}:");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {result}");

// Benchmark 2: Prime counting
limit = 1000000;
sw = Stopwatch.StartNew();
result = CountPrimes(limit);
sw.Stop();
Console.WriteLine($"Prime count under {limit}:");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {result}");

// Benchmark 3: Ackermann
long m = 3, n = 11;
sw = Stopwatch.StartNew();
result = Ack(m, n);
sw.Stop();
Console.WriteLine($"Ackermann({m}, {n}):");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {result}");

// Benchmark 4: Popcount
limit = 10000000;
sw = Stopwatch.StartNew();
result = TotalPopcount(limit);
sw.Stop();
Console.WriteLine($"Popcount sum 1..{limit}:");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {result}");

// Benchmark 5: Takeuchi
long x = 24, y = 16, z = 8;
sw = Stopwatch.StartNew();
result = Tak(x, y, z);
sw.Stop();
Console.WriteLine($"Tak({x}, {y}, {z}):");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {result}");

// Benchmark 6: Euler totient sum
limit = 10000;
sw = Stopwatch.StartNew();
result = SumTotients(limit);
sw.Stop();
Console.WriteLine($"Euler totient sum(1..{limit}):");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {result}");

// Benchmark 7: Digit sum
limit = 10000000;
sw = Stopwatch.StartNew();
result = TotalDigitSum(limit);
sw.Stop();
Console.WriteLine($"Digit sum(1..{limit}):");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {result}");
