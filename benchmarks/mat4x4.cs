using System.Diagnostics;

const int RUNS = 3;
const int ITERS = 1000000;

static double[] Mat4Mul(double[] a, double[] b) {
    double[] r = new double[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double sum = 0;
            for (int k = 0; k < 4; k++)
                sum += a[i * 4 + k] * b[k * 4 + j];
            r[i * 4 + j] = sum;
        }
    return r;
}

static double[] Identity() {
    double[] m = new double[16];
    m[0] = 1; m[5] = 1; m[10] = 1; m[15] = 1;
    return m;
}

Console.WriteLine("=== C# Mat4x4 Benchmarks ===");

double[] b = {
    0.8660254037844387, -0.5, 0.0, 0.0,
    0.5, 0.8660254037844387, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
};

// Warmup
double[] a = Identity();
for (int i = 0; i < ITERS; i++) a = Mat4Mul(a, b);

// Best of 3
long best = long.MaxValue;
double checksum = 0;
for (int run = 0; run < RUNS; run++) {
    a = Identity();
    var sw = Stopwatch.StartNew();
    for (int i = 0; i < ITERS; i++) a = Mat4Mul(a, b);
    sw.Stop();
    long ns = sw.Elapsed.Ticks * 100;
    if (ns < best) {
        best = ns;
        checksum = 0;
        for (int i = 0; i < 16; i++) checksum += a[i];
    }
}
Console.WriteLine($"Mat4x4 multiply ({ITERS} iterations):");
Console.WriteLine($"  Time: {best / 1000000} ms");
Console.WriteLine($"  Result: {checksum:F10}");
