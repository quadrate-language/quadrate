#include <stdio.h>
#include <stdint.h>
#include <time.h>

// Basel problem: sum of 1/i^2 for i=1..n
double sum_inv_squares(int64_t n) {
    double sum = 0.0;
    for (int64_t i = 1; i <= n; i++) {
        double fi = (double)i;
        sum += 1.0 / (fi * fi);
    }
    return sum;
}

// Leibniz formula for pi: pi/4 = 1 - 1/3 + 1/5 - 1/7 + ...
double leibniz_pi(int64_t n) {
    double sum = 0.0;
    double sign = 1.0;
    for (int64_t i = 1; i <= n; i++) {
        double denom = (double)(i * 2 - 1);
        sum += sign / denom;
        sign = -sign;
    }
    return sum * 4.0;
}

// Mandelbrot escape iteration
int64_t mandelbrot_iter(double cr, double ci, int64_t max_iter) {
    double zr = 0.0, zi = 0.0;
    for (int64_t i = 0; i < max_iter; i++) {
        double tr = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = tr;
        if (zr * zr + zi * zi > 4.0) {
            return i;
        }
    }
    return max_iter;
}

int64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

int main() {
    printf("=== C Float Benchmarks ===\n");

    // Benchmark 1: Basel problem
    int64_t n = 10000000;
    int64_t start = get_time_ns();
    double result_f = sum_inv_squares(n);
    int64_t elapsed = get_time_ns() - start;
    printf("Basel sum(1/i^2, 1..%ld):\n", n);
    printf("  Time: %ld ms\n", elapsed / 1000000);
    printf("  Result: %.10f\n", result_f);

    // Benchmark 2: Leibniz pi
    n = 10000000;
    start = get_time_ns();
    result_f = leibniz_pi(n);
    elapsed = get_time_ns() - start;
    printf("Leibniz pi(%ld terms):\n", n);
    printf("  Time: %ld ms\n", elapsed / 1000000);
    printf("  Result: %.10f\n", result_f);

    // Benchmark 3: Mandelbrot grid
    int64_t size = 200;
    start = get_time_ns();
    int64_t total = 0;
    for (int64_t y = 0; y < size; y++) {
        for (int64_t x = 0; x < size; x++) {
            double cr = ((double)x - 100.0) / 50.0;
            double ci = ((double)y - 100.0) / 50.0;
            total += mandelbrot_iter(cr, ci, 100);
        }
    }
    elapsed = get_time_ns() - start;
    printf("Mandelbrot %ldx%ld:\n", size, size);
    printf("  Time: %ld ms\n", elapsed / 1000000);
    printf("  Result: %ld\n", total);

    return 0;
}
