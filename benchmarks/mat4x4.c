#include <stdio.h>
#include <time.h>

#define RUNS 3
#define ITERS 1000000

typedef struct { double m[4][4]; } Mat4;

static Mat4 mat4_mul(Mat4 a, Mat4 b) {
	Mat4 r;
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++) {
			r.m[i][j] = 0;
			for (int k = 0; k < 4; k++)
				r.m[i][j] += a.m[i][k] * b.m[k][j];
		}
	return r;
}

int main() {
	printf("=== C Mat4x4 Benchmarks ===\n");

	Mat4 b = {{
		{0.8660254037844387, -0.5, 0.0, 0.0},
		{0.5, 0.8660254037844387, 0.0, 0.0},
		{0.0, 0.0, 1.0, 0.0},
		{0.0, 0.0, 0.0, 1.0}
	}};

	// Warmup
	Mat4 a = {{
		{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}
	}};
	for (int i = 0; i < ITERS; i++) a = mat4_mul(a, b);

	// Best of 3
	long best = __LONG_MAX__;
	double checksum = 0;
	for (int run = 0; run < RUNS; run++) {
		a = (Mat4){{
			{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}
		}};
		struct timespec t0, t1;
		clock_gettime(CLOCK_MONOTONIC, &t0);
		for (int i = 0; i < ITERS; i++) a = mat4_mul(a, b);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		long ns = (t1.tv_sec - t0.tv_sec) * 1000000000L + (t1.tv_nsec - t0.tv_nsec);
		if (ns < best) {
			best = ns;
			checksum = 0;
			for (int i = 0; i < 4; i++)
				for (int j = 0; j < 4; j++)
					checksum += a.m[i][j];
		}
	}
	printf("Mat4x4 multiply (%d iterations):\n", ITERS);
	printf("  Time: %ld ms\n", best / 1000000);
	printf("  Result: %.10f\n", checksum);
	return 0;
}
