package main

import (
	"fmt"
	"time"
)

const runs = 3

func benchmarkArithmetic(iterations int64) int64 {
	var sum int64 = 0
	for i := int64(0); i < iterations; i++ {
		sum = ((sum + i) * i + 3) % 7
	}
	return sum
}

func fib(n int64) int64 {
	if n < 2 {
		return n
	}
	return fib(n-1) + fib(n-2)
}

func main() {
	fmt.Println("=== Go Benchmarks ===")

	// Benchmark 1: Arithmetic loop
	iterations := int64(10000000)
	benchmarkArithmetic(iterations) // warmup
	var best int64 = 1 << 62
	var result int64
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := benchmarkArithmetic(iterations)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			result = r
		}
	}
	fmt.Printf("Arithmetic loop (%d iterations):\n", iterations)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 2: Recursive fibonacci
	n := int64(35)
	fib(n) // warmup
	best = 1 << 62
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := fib(n)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			result = r
		}
	}
	fmt.Printf("Fibonacci (n=%d):\n", n)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %d\n", result)
}
