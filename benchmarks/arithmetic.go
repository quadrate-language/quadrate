package main

import (
	"fmt"
	"time"
)

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
	start := time.Now()
	result := benchmarkArithmetic(iterations)
	elapsed := time.Since(start)

	fmt.Printf("Arithmetic loop (%d iterations):\n", iterations)
	fmt.Printf("  Time: %d ms\n", elapsed.Milliseconds())
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 2: Recursive fibonacci
	n := int64(35)
	start = time.Now()
	result = fib(n)
	elapsed = time.Since(start)

	fmt.Printf("Fibonacci (n=%d):\n", n)
	fmt.Printf("  Time: %d ms\n", elapsed.Milliseconds())
	fmt.Printf("  Result: %d\n", result)
}
