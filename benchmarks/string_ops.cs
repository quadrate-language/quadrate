using System.Diagnostics;

const int Runs = 3;
int iterations = 50000;

// Warmup
string s = "";
for (int i = 0; i < iterations; i++) {
    s += "a";
}
s = "";

// Best of 3
long bestNs = long.MaxValue;
int finalLen = 0;
for (int run = 0; run < Runs; run++) {
    s = "";
    var sw = Stopwatch.StartNew();
    for (int i = 0; i < iterations; i++) {
        s += "a";
    }
    sw.Stop();
    long ns = sw.Elapsed.Ticks * 100;
    if (ns < bestNs) {
        bestNs = ns;
    }
    finalLen = s.Length;
}

Console.WriteLine("=== C# String Benchmarks ===");
Console.WriteLine($"String Concat ({iterations} iterations):");
Console.WriteLine($"  Time: {bestNs / 1000000} ms");
Console.WriteLine($"  Final Length: {finalLen}");
