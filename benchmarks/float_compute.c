#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#define RUNS 3

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

// Euclidean distance using sqrt
double distance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

// Sum of sin(x) + cos(x) for x = 0..n*0.001
double trig_sum(int64_t n) {
    double sum = 0.0;
    for (int64_t i = 0; i < n; i++) {
        double x = (double)i * 0.001;
        sum += sin(x) + cos(x);
    }
    return sum;
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
    sum_inv_squares(n); // warmup
    int64_t best = INT64_MAX;
    double result_f;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        double r = sum_inv_squares(n);
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result_f = r; }
    }
    printf("Basel sum(1/i^2, 1..%ld):\n", n);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %.10f\n", result_f);

    // Benchmark 2: Leibniz pi
    n = 10000000;
    leibniz_pi(n); // warmup
    best = INT64_MAX;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        double r = leibniz_pi(n);
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result_f = r; }
    }
    printf("Leibniz pi(%ld terms):\n", n);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %.10f\n", result_f);

    // Benchmark 3: Mandelbrot grid
    int64_t size = 200;
    // warmup
    {
        int64_t t = 0;
        for (int64_t y = 0; y < size; y++)
            for (int64_t x = 0; x < size; x++)
                t += mandelbrot_iter(((double)x - 100.0) / 50.0, ((double)y - 100.0) / 50.0, 100);
    }
    best = INT64_MAX;
    int64_t total;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        int64_t t = 0;
        for (int64_t y = 0; y < size; y++) {
            for (int64_t x = 0; x < size; x++) {
                double cr = ((double)x - 100.0) / 50.0;
                double ci = ((double)y - 100.0) / 50.0;
                t += mandelbrot_iter(cr, ci, 100);
            }
        }
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; total = t; }
    }
    printf("Mandelbrot %ldx%ld:\n", size, size);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %ld\n", total);

    // Benchmark 4: Distance computation
    n = 10000000;
    // warmup
    {
        double s = 0.0;
        for (int64_t i = 0; i < n; i++) {
            double x = (double)i * 0.001;
            s += distance(x, 0.0, 0.0, x);
        }
    }
    best = INT64_MAX;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        double s = 0.0;
        for (int64_t i = 0; i < n; i++) {
            double x = (double)i * 0.001;
            s += distance(x, 0.0, 0.0, x);
        }
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result_f = s; }
    }
    printf("Distance (%ld iterations):\n", n);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %.10f\n", result_f);

    // Benchmark 5: Trig computation
    n = 5000000;
    trig_sum(n); // warmup
    best = INT64_MAX;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        double r = trig_sum(n);
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result_f = r; }
    }
    printf("Trig sum(%ld):\n", n);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %.10f\n", result_f);

    return 0;
}
