import time

print("=== Python String Benchmarks ===")
iterations = 50000

start = time.time()
s = ""
for _ in range(iterations):
    s += "a"
end = time.time()

print(f"String Concat ({iterations} iterations):")
print(f"  Time: {(end - start) * 1000:.0f} ms")
print(f"  Final Length: {len(s)}")
