using System.Diagnostics;

const int Runs = 3;

long BenchmarkArithmetic(long iterations) {
    long sum = 0;
    for (long i = 0; i < iterations; i++) {
        sum = ((sum + i) * i + 3) % 7;
    }
    return sum;
}

long Fib(long n) {
    if (n < 2) return n;
    return Fib(n - 1) + Fib(n - 2);
}

Console.WriteLine("=== C# Benchmarks ===");

// Benchmark 1: Arithmetic loop
long iterations = 10000000;
BenchmarkArithmetic(iterations); // warmup
long bestNs = long.MaxValue;
long result = 0;
for (int run = 0; run < Runs; run++) {
    var sw = Stopwatch.StartNew();
    long r = BenchmarkArithmetic(iterations);
    sw.Stop();
    long ns = sw.Elapsed.Ticks * 100;
    if (ns < bestNs) { bestNs = ns; result = r; }
}
Console.WriteLine($"Arithmetic loop ({iterations} iterations):");
Console.WriteLine($"  Time: {bestNs / 1000000} ms");
Console.WriteLine($"  Result: {result}");

// Benchmark 2: Recursive fibonacci
long n = 35;
Fib(n); // warmup
bestNs = long.MaxValue;
for (int run = 0; run < Runs; run++) {
    var sw = Stopwatch.StartNew();
    long r = Fib(n);
    sw.Stop();
    long ns = sw.Elapsed.Ticks * 100;
    if (ns < bestNs) { bestNs = ns; result = r; }
}
Console.WriteLine($"Fibonacci (n={n}):");
Console.WriteLine($"  Time: {bestNs / 1000000} ms");
Console.WriteLine($"  Result: {result}");
