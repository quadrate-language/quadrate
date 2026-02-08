import time

RUNS = 3
ITERS = 1000000

def mat4_mul(a, b):
    r = [0.0] * 16
    for i in range(4):
        for j in range(4):
            s = 0.0
            for k in range(4):
                s += a[i * 4 + k] * b[k * 4 + j]
            r[i * 4 + j] = s
    return r

def identity():
    m = [0.0] * 16
    m[0] = 1.0; m[5] = 1.0; m[10] = 1.0; m[15] = 1.0
    return m

print("=== Python Mat4x4 Benchmarks ===")

b = [
    0.8660254037844387, -0.5, 0.0, 0.0,
    0.5, 0.8660254037844387, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
]

# Warmup
a = identity()
for i in range(ITERS):
    a = mat4_mul(a, b)

# Best of 3
best = float('inf')
checksum = 0
for run in range(RUNS):
    a = identity()
    start = time.perf_counter_ns()
    for i in range(ITERS):
        a = mat4_mul(a, b)
    elapsed = time.perf_counter_ns() - start
    if elapsed < best:
        best = elapsed
        checksum = sum(a)

print(f"Mat4x4 multiply ({ITERS} iterations):")
print(f"  Time: {best // 1000000} ms")
print(f"  Result: {checksum:.10f}")
