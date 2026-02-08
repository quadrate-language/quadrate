package main

import (
	"fmt"
	"time"
)

const runs = 3

func sumInvSquares(n int64) float64 {
	var sum float64 = 0.0
	for i := int64(1); i <= n; i++ {
		fi := float64(i)
		sum += 1.0 / (fi * fi)
	}
	return sum
}

func leibnizPi(n int64) float64 {
	var sum float64 = 0.0
	var sign float64 = 1.0
	for i := int64(1); i <= n; i++ {
		denom := float64(i*2 - 1)
		sum += sign / denom
		sign = -sign
	}
	return sum * 4.0
}

func mandelbrotIter(cr, ci float64, maxIter int64) int64 {
	var zr, zi float64 = 0.0, 0.0
	for i := int64(0); i < maxIter; i++ {
		tr := zr*zr - zi*zi + cr
		zi = 2.0*zr*zi + ci
		zr = tr
		if zr*zr+zi*zi > 4.0 {
			return i
		}
	}
	return maxIter
}

func main() {
	fmt.Println("=== Go Float Benchmarks ===")

	// Benchmark 1: Basel problem
	n := int64(10000000)
	sumInvSquares(n) // warmup
	var best int64 = 1 << 62
	var resultF float64
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := sumInvSquares(n)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			resultF = r
		}
	}
	fmt.Printf("Basel sum(1/i^2, 1..%d):\n", n)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %.10f\n", resultF)

	// Benchmark 2: Leibniz pi
	n = 10000000
	leibnizPi(n) // warmup
	best = 1 << 62
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := leibnizPi(n)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			resultF = r
		}
	}
	fmt.Printf("Leibniz pi(%d terms):\n", n)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %.10f\n", resultF)

	// Benchmark 3: Mandelbrot grid
	size := int64(200)
	// warmup
	{
		var t int64 = 0
		for y := int64(0); y < size; y++ {
			for x := int64(0); x < size; x++ {
				cr := (float64(x) - 100.0) / 50.0
				ci := (float64(y) - 100.0) / 50.0
				t += mandelbrotIter(cr, ci, 100)
			}
		}
		_ = t
	}
	best = 1 << 62
	var total int64
	for run := 0; run < runs; run++ {
		start := time.Now()
		var t int64 = 0
		for y := int64(0); y < size; y++ {
			for x := int64(0); x < size; x++ {
				cr := (float64(x) - 100.0) / 50.0
				ci := (float64(y) - 100.0) / 50.0
				t += mandelbrotIter(cr, ci, 100)
			}
		}
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			total = t
		}
	}
	fmt.Printf("Mandelbrot %dx%d:\n", size, size)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %d\n", total)
}
