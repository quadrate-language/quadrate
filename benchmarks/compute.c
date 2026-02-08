#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define RUNS 3

int64_t collatz_len(int64_t n) {
    int64_t len = 0;
    while (n > 1) {
        if (n % 2 == 0) {
            n = n / 2;
        } else {
            n = n * 3 + 1;
        }
        len++;
    }
    return len;
}

int64_t longest_collatz(int64_t limit) {
    int64_t best = 0;
    for (int64_t i = 1; i < limit; i++) {
        int64_t len = collatz_len(i);
        if (len > best) {
            best = len;
        }
    }
    return best;
}

int64_t is_prime(int64_t n) {
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
    for (int64_t d = 3; d * d <= n; d += 2) {
        if (n % d == 0) return 0;
    }
    return 1;
}

int64_t count_primes(int64_t limit) {
    int64_t count = 0;
    for (int64_t i = 2; i < limit; i++) {
        if (is_prime(i)) count++;
    }
    return count;
}

int64_t ack(int64_t m, int64_t n) {
    if (m == 0) return n + 1;
    if (n == 0) return ack(m - 1, 1);
    return ack(m - 1, ack(m, n - 1));
}

int64_t popcount(int64_t n) {
    int64_t count = 0;
    while (n > 0) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

int64_t total_popcount(int64_t limit) {
    int64_t total = 0;
    for (int64_t i = 1; i <= limit; i++) {
        total += popcount(i);
    }
    return total;
}

int64_t tak(int64_t x, int64_t y, int64_t z) {
    if (x <= y) return z;
    return tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y));
}

int64_t gcd(int64_t a, int64_t b) {
    while (b != 0) {
        int64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int64_t euler_totient(int64_t n) {
    int64_t count = 0;
    for (int64_t i = 1; i < n; i++) {
        if (gcd(i, n) == 1) count++;
    }
    return count;
}

int64_t sum_totients(int64_t limit) {
    int64_t total = 0;
    for (int64_t i = 1; i <= limit; i++) {
        total += euler_totient(i);
    }
    return total;
}

int64_t digit_sum(int64_t n) {
    int64_t sum = 0;
    while (n > 0) {
        sum += n % 10;
        n /= 10;
    }
    return sum;
}

int64_t total_digit_sum(int64_t limit) {
    int64_t total = 0;
    for (int64_t i = 1; i <= limit; i++) {
        total += digit_sum(i);
    }
    return total;
}

int64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

int main() {
    printf("=== C Compute Benchmarks ===\n");

    // Benchmark 1: Collatz conjecture
    int64_t limit = 1000000;
    longest_collatz(limit); // warmup
    int64_t best = INT64_MAX, result;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        int64_t r = longest_collatz(limit);
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    printf("Collatz longest chain under %ld:\n", limit);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %ld\n", result);

    // Benchmark 2: Prime counting
    limit = 1000000;
    count_primes(limit); // warmup
    best = INT64_MAX;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        int64_t r = count_primes(limit);
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    printf("Prime count under %ld:\n", limit);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %ld\n", result);

    // Benchmark 3: Ackermann
    int64_t m = 3, n = 11;
    ack(m, n); // warmup
    best = INT64_MAX;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        int64_t r = ack(m, n);
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    printf("Ackermann(%ld, %ld):\n", m, n);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %ld\n", result);

    // Benchmark 4: Popcount
    limit = 10000000;
    total_popcount(limit); // warmup
    best = INT64_MAX;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        int64_t r = total_popcount(limit);
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    printf("Popcount sum 1..%ld:\n", limit);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %ld\n", result);

    // Benchmark 5: Takeuchi
    int64_t x = 24, y = 16, z = 8;
    tak(x, y, z); // warmup
    best = INT64_MAX;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        int64_t r = tak(x, y, z);
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    printf("Tak(%ld, %ld, %ld):\n", x, y, z);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %ld\n", result);

    // Benchmark 6: Euler totient sum
    limit = 10000;
    sum_totients(limit); // warmup
    best = INT64_MAX;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        int64_t r = sum_totients(limit);
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    printf("Euler totient sum(1..%ld):\n", limit);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %ld\n", result);

    // Benchmark 7: Digit sum
    limit = 10000000;
    total_digit_sum(limit); // warmup
    best = INT64_MAX;
    for (int run = 0; run < RUNS; run++) {
        int64_t start = get_time_ns();
        int64_t r = total_digit_sum(limit);
        int64_t elapsed = get_time_ns() - start;
        if (elapsed < best) { best = elapsed; result = r; }
    }
    printf("Digit sum(1..%ld):\n", limit);
    printf("  Time: %ld ms\n", best / 1000000);
    printf("  Result: %ld\n", result);

    return 0;
}
