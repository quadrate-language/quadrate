#include <stdio.h>
#include <stdint.h>
#include <time.h>

int64_t benchmark_arithmetic(int64_t iterations) {
    int64_t sum = 0;
    for (int64_t i = 0; i < iterations; i++) {
        sum = ((sum + i) * i + 3) % 7;
    }
    return sum;
}

int64_t fib(int64_t n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

int main() {
    printf("=== C Benchmarks ===\n");
    
    // Benchmark 1: Arithmetic loop
    int64_t iterations = 10000000;
    int64_t start = get_time_ns();
    int64_t result = benchmark_arithmetic(iterations);
    int64_t elapsed = get_time_ns() - start;
    
    printf("Arithmetic loop (%ld iterations):\n", iterations);
    printf("  Time: %ld ms\n", elapsed / 1000000);
    printf("  Result: %ld\n", result);
    
    // Benchmark 2: Recursive fibonacci
    int64_t n = 35;
    start = get_time_ns();
    result = fib(n);
    elapsed = get_time_ns() - start;
    
    printf("Fibonacci (n=%ld):\n", n);
    printf("  Time: %ld ms\n", elapsed / 1000000);
    printf("  Result: %ld\n", result);
    
    return 0;
}
