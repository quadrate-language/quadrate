import time

print("=== Python String Benchmarks ===")
iterations = 50000
RUNS = 3

# Warmup
s = ""
for _ in range(iterations):
    s += "a"
del s

# Best of 3
best_ns = float("inf")
final_len = 0
for run in range(RUNS):
    s = ""
    start = time.time_ns()
    for _ in range(iterations):
        s += "a"
    elapsed = time.time_ns() - start
    if elapsed < best_ns:
        best_ns = elapsed
    final_len = len(s)

print(f"String Concat ({iterations} iterations):")
print(f"  Time: {best_ns // 1000000} ms")
print(f"  Final Length: {final_len}")
