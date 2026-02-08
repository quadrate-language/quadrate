import time
import sys

sys.setrecursionlimit(20000)

RUNS = 3

def benchmark_arithmetic(iterations):
    s = 0
    for i in range(iterations):
        s = ((s + i) * i + 3) % 7
    return s

def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

def main():
    print("=== Python Benchmarks ===")

    # Benchmark 1: Arithmetic loop
    iterations = 10000000
    benchmark_arithmetic(iterations)  # warmup
    best = float('inf')
    result = 0
    for _ in range(RUNS):
        start = time.time_ns()
        r = benchmark_arithmetic(iterations)
        elapsed = time.time_ns() - start
        if elapsed < best:
            best = elapsed
            result = r
    print(f"Arithmetic loop ({iterations} iterations):")
    print(f"  Time: {best // 1000000} ms")
    print(f"  Result: {result}")

    # Benchmark 2: Recursive fibonacci
    n = 35
    fib(n)  # warmup
    best = float('inf')
    for _ in range(RUNS):
        start = time.time_ns()
        r = fib(n)
        elapsed = time.time_ns() - start
        if elapsed < best:
            best = elapsed
            result = r
    print(f"Fibonacci (n={n}):")
    print(f"  Time: {best // 1000000} ms")
    print(f"  Result: {result}")

if __name__ == "__main__":
    main()
