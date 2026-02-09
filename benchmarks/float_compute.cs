using System.Diagnostics;

const int Runs = 3;

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

double Distance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return Math.Sqrt(dx * dx + dy * dy);
}

double TrigSum(long n) {
    double sum = 0.0;
    for (long i = 0; i < n; i++) {
        double x = (double)i * 0.001;
        sum += Math.Sin(x) + Math.Cos(x);
    }
    return sum;
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
SumInvSquares(n); // warmup
long best = long.MaxValue;
double resultF = 0.0;
for (int run = 0; run < Runs; run++) {
    var sw = Stopwatch.StartNew();
    double r = SumInvSquares(n);
    sw.Stop();
    long elapsed = sw.Elapsed.Ticks * 100;
    if (elapsed < best) { best = elapsed; resultF = r; }
}
Console.WriteLine($"Basel sum(1/i^2, 1..{n}):");
Console.WriteLine($"  Time: {best / 1000000} ms");
Console.WriteLine($"  Result: {resultF:F10}");

// Benchmark 2: Leibniz pi
n = 10000000;
LeibnizPi(n); // warmup
best = long.MaxValue;
for (int run = 0; run < Runs; run++) {
    var sw = Stopwatch.StartNew();
    double r = LeibnizPi(n);
    sw.Stop();
    long elapsed = sw.Elapsed.Ticks * 100;
    if (elapsed < best) { best = elapsed; resultF = r; }
}
Console.WriteLine($"Leibniz pi({n} terms):");
Console.WriteLine($"  Time: {best / 1000000} ms");
Console.WriteLine($"  Result: {resultF:F10}");

// Benchmark 3: Mandelbrot grid
long size = 200;
// warmup
{
    long t = 0;
    for (long y = 0; y < size; y++)
        for (long x = 0; x < size; x++)
            t += MandelbrotIter(((double)x - 100.0) / 50.0, ((double)y - 100.0) / 50.0, 100);
}
best = long.MaxValue;
long total = 0;
for (int run = 0; run < Runs; run++) {
    var sw = Stopwatch.StartNew();
    long t = 0;
    for (long y = 0; y < size; y++) {
        for (long x = 0; x < size; x++) {
            double cr = ((double)x - 100.0) / 50.0;
            double ci = ((double)y - 100.0) / 50.0;
            t += MandelbrotIter(cr, ci, 100);
        }
    }
    sw.Stop();
    long elapsed = sw.Elapsed.Ticks * 100;
    if (elapsed < best) { best = elapsed; total = t; }
}
Console.WriteLine($"Mandelbrot {size}x{size}:");
Console.WriteLine($"  Time: {best / 1000000} ms");
Console.WriteLine($"  Result: {total}");

// Benchmark 4: Distance computation
n = 10000000;
// warmup
{
    double s = 0.0;
    for (long i = 0; i < n; i++) {
        double x = (double)i * 0.001;
        s += Distance(x, 0.0, 0.0, x);
    }
}
best = long.MaxValue;
for (int run = 0; run < Runs; run++) {
    var sw = Stopwatch.StartNew();
    double s = 0.0;
    for (long i = 0; i < n; i++) {
        double x = (double)i * 0.001;
        s += Distance(x, 0.0, 0.0, x);
    }
    sw.Stop();
    long elapsed = sw.Elapsed.Ticks * 100;
    if (elapsed < best) { best = elapsed; resultF = s; }
}
Console.WriteLine($"Distance ({n} iterations):");
Console.WriteLine($"  Time: {best / 1000000} ms");
Console.WriteLine($"  Result: {resultF:F10}");

// Benchmark 5: Trig computation
n = 5000000;
TrigSum(n); // warmup
best = long.MaxValue;
for (int run = 0; run < Runs; run++) {
    var sw = Stopwatch.StartNew();
    double r = TrigSum(n);
    sw.Stop();
    long elapsed = sw.Elapsed.Ticks * 100;
    if (elapsed < best) { best = elapsed; resultF = r; }
}
Console.WriteLine($"Trig sum({n}):");
Console.WriteLine($"  Time: {best / 1000000} ms");
Console.WriteLine($"  Result: {resultF:F10}");
