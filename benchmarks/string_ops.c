#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

int main() {
    int iterations = 50000;
    
    clock_t start = clock();
    
    char* s = strdup("");
    for (int i = 0; i < iterations; i++) {
        char* new_s = concat(s, "a");
        free(s);
        s = new_s;
    }
    
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC * 1000.0;
    
    printf("=== C String Benchmarks ===\n");
    printf("String Concat (%d iterations):\n", iterations);
    printf("  Time: %.0f ms\n", cpu_time_used);
    printf("  Final Length: %lu\n", strlen(s));
    
    free(s);
    return 0;
}
