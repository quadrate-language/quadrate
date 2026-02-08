using System.Diagnostics;

int iterations = 50000;

var sw = Stopwatch.StartNew();
string s = "";
for (int i = 0; i < iterations; i++) {
    s += "a";
}
sw.Stop();

Console.WriteLine("=== C# String Benchmarks ===");
Console.WriteLine($"String Concat ({iterations} iterations):");
Console.WriteLine($"  Time: {sw.ElapsedMilliseconds} ms");
Console.WriteLine($"  Final Length: {s.Length}");
