using System.Diagnostics;

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
var sw = Stopwatch.StartNew();
long result = BenchmarkArithmetic(iterations);
sw.Stop();
Console.WriteLine($"Arithmetic loop ({iterations} iterations):");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {result}");

// Benchmark 2: Recursive fibonacci
long n = 35;
sw = Stopwatch.StartNew();
result = Fib(n);
sw.Stop();
Console.WriteLine($"Fibonacci (n={n}):");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {result}");
