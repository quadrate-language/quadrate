using System.Diagnostics;

double SumInvSquares(long n) {
    double sum = 0.0;
    for (long i = 1; i <= n; i++) {
        double fi = (double)i;
        sum += 1.0 / (fi * fi);
    }
    return sum;
}

double LeibnizPi(long n) {
    double sum = 0.0;
    double sign = 1.0;
    for (long i = 1; i <= n; i++) {
        double denom = (double)(i * 2 - 1);
        sum += sign / denom;
        sign = -sign;
    }
    return sum * 4.0;
}

long MandelbrotIter(double cr, double ci, long maxIter) {
    double zr = 0.0, zi = 0.0;
    for (long i = 0; i < maxIter; i++) {
        double tr = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = tr;
        if (zr * zr + zi * zi > 4.0) {
            return i;
        }
    }
    return maxIter;
}

Console.WriteLine("=== C# Float Benchmarks ===");

// Benchmark 1: Basel problem
long n = 10000000;
var sw = Stopwatch.StartNew();
double resultF = SumInvSquares(n);
sw.Stop();
Console.WriteLine($"Basel sum(1/i^2, 1..{n}):");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {resultF:F10}");

// Benchmark 2: Leibniz pi
n = 10000000;
sw = Stopwatch.StartNew();
resultF = LeibnizPi(n);
sw.Stop();
Console.WriteLine($"Leibniz pi({n} terms):");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {resultF:F10}");

// Benchmark 3: Mandelbrot grid
long size = 200;
sw = Stopwatch.StartNew();
long total = 0;
for (long y = 0; y < size; y++) {
    for (long x = 0; x < size; x++) {
        double cr = ((double)x - 100.0) / 50.0;
        double ci = ((double)y - 100.0) / 50.0;
        total += MandelbrotIter(cr, ci, 100);
    }
}
sw.Stop();
Console.WriteLine($"Mandelbrot {size}x{size}:");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Result: {total}");
