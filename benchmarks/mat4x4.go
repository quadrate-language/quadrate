package main

import (
	"fmt"
	"math"
	"time"
)

const RUNS = 3
const ITERS = 1000000

type Mat4 [4][4]float64

func mat4Mul(a, b Mat4) Mat4 {
	var r Mat4
	for i := 0; i < 4; i++ {
		for j := 0; j < 4; j++ {
			for k := 0; k < 4; k++ {
				r[i][j] += a[i][k] * b[k][j]
			}
		}
	}
	return r
}

func main() {
	fmt.Println("=== Go Mat4x4 Benchmarks ===")

	b := Mat4{
		{0.8660254037844387, -0.5, 0.0, 0.0},
		{0.5, 0.8660254037844387, 0.0, 0.0},
		{0.0, 0.0, 1.0, 0.0},
		{0.0, 0.0, 0.0, 1.0},
	}

	// Warmup
	a := Mat4{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}
	for i := 0; i < ITERS; i++ {
		a = mat4Mul(a, b)
	}
	_ = a

	// Best of 3
	best := int64(math.MaxInt64)
	checksum := 0.0
	for run := 0; run < RUNS; run++ {
		a = Mat4{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}
		start := time.Now()
		for i := 0; i < ITERS; i++ {
			a = mat4Mul(a, b)
		}
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			checksum = 0
			for i := 0; i < 4; i++ {
				for j := 0; j < 4; j++ {
					checksum += a[i][j]
				}
			}
		}
	}
	fmt.Printf("Mat4x4 multiply (%d iterations):\n", ITERS)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %.10f\n", checksum)
}
