#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RUNS 3

// Naive concatenation that mimics Quadrate's current behavior
// (allocate new string, copy both, free inputs is handled by caller)
char* concat(const char* s1, const char* s2) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    char* result = malloc(len1 + len2 + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

static long long get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main() {
    int iterations = 50000;

    // Warmup
    char* s = strdup("");
    for (int i = 0; i < iterations; i++) {
        char* new_s = concat(s, "a");
        free(s);
        s = new_s;
    }
    free(s);

    // Best of 3
    long long best = 0;
    size_t final_len = 0;
    for (int run = 0; run < RUNS; run++) {
        s = strdup("");
        long long start = get_time_ns();
        for (int i = 0; i < iterations; i++) {
            char* new_s = concat(s, "a");
            free(s);
            s = new_s;
        }
        long long elapsed = get_time_ns() - start;
        if (run == 0 || elapsed < best) {
            best = elapsed;
        }
        final_len = strlen(s);
        free(s);
    }

    printf("=== C String Benchmarks ===\n");
    printf("String Concat (%d iterations):\n", iterations);
    printf("  Time: %lld ms\n", best / 1000000LL);
    printf("  Final Length: %lu\n", final_len);

    return 0;
}
