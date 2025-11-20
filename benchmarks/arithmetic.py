import time

def benchmark_arithmetic(iterations):
    sum_val = 0
    for i in range(iterations):
        sum_val = ((sum_val + i) * i + 3) % 7
    return sum_val

def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

def main():
    print("=== Python Benchmarks ===")
    
    # Benchmark 1: Arithmetic loop
    iterations = 10000000
    start = time.time_ns()
    result = benchmark_arithmetic(iterations)
    elapsed = time.time_ns() - start
    
    print(f"Arithmetic loop ({iterations} iterations):")
    print(f"  Time: {elapsed // 1000000} ms")
    print(f"  Result: {result}")
    
    # Benchmark 2: Recursive fibonacci
    n = 35
    start = time.time_ns()
    result = fib(n)
    elapsed = time.time_ns() - start
    
    print(f"Fibonacci (n={n}):")
    print(f"  Time: {elapsed // 1000000} ms")
    print(f"  Result: {result}")

if __name__ == "__main__":
    main()
