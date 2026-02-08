package main

import (
	"fmt"
	"time"
)

func collatzLen(n int64) int64 {
	var length int64 = 0
	for n > 1 {
		if n%2 == 0 {
			n = n / 2
		} else {
			n = n*3 + 1
		}
		length++
	}
	return length
}

func longestCollatz(limit int64) int64 {
	var best int64 = 0
	for i := int64(1); i < limit; i++ {
		length := collatzLen(i)
		if length > best {
			best = length
		}
	}
	return best
}

func isPrime(n int64) bool {
	if n < 2 {
		return false
	}
	if n == 2 {
		return true
	}
	if n%2 == 0 {
		return false
	}
	for d := int64(3); d*d <= n; d += 2 {
		if n%d == 0 {
			return false
		}
	}
	return true
}

func countPrimes(limit int64) int64 {
	var count int64 = 0
	for i := int64(2); i < limit; i++ {
		if isPrime(i) {
			count++
		}
	}
	return count
}

func ack(m, n int64) int64 {
	if m == 0 {
		return n + 1
	}
	if n == 0 {
		return ack(m-1, 1)
	}
	return ack(m-1, ack(m, n-1))
}

func popcount(n int64) int64 {
	var count int64 = 0
	for n > 0 {
		count += n & 1
		n >>= 1
	}
	return count
}

func totalPopcount(limit int64) int64 {
	var total int64 = 0
	for i := int64(1); i <= limit; i++ {
		total += popcount(i)
	}
	return total
}

func tak(x, y, z int64) int64 {
	if x <= y {
		return z
	}
	return tak(tak(x-1, y, z), tak(y-1, z, x), tak(z-1, x, y))
}

func main() {
	fmt.Println("=== Go Compute Benchmarks ===")

	// Benchmark 1: Collatz conjecture
	limit := int64(1000000)
	start := time.Now()
	result := longestCollatz(limit)
	elapsed := time.Since(start)
	fmt.Printf("Collatz longest chain under %d:\n", limit)
	fmt.Printf("  Time: %d ms\n", elapsed.Milliseconds())
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 2: Prime counting
	limit = 1000000
	start = time.Now()
	result = countPrimes(limit)
	elapsed = time.Since(start)
	fmt.Printf("Prime count under %d:\n", limit)
	fmt.Printf("  Time: %d ms\n", elapsed.Milliseconds())
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 3: Ackermann
	m, n := int64(3), int64(11)
	start = time.Now()
	result = ack(m, n)
	elapsed = time.Since(start)
	fmt.Printf("Ackermann(%d, %d):\n", m, n)
	fmt.Printf("  Time: %d ms\n", elapsed.Milliseconds())
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 4: Popcount
	limit = 10000000
	start = time.Now()
	result = totalPopcount(limit)
	elapsed = time.Since(start)
	fmt.Printf("Popcount sum 1..%d:\n", limit)
	fmt.Printf("  Time: %d ms\n", elapsed.Milliseconds())
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 5: Takeuchi
	x, y, z := int64(24), int64(16), int64(8)
	start = time.Now()
	result = tak(x, y, z)
	elapsed = time.Since(start)
	fmt.Printf("Tak(%d, %d, %d):\n", x, y, z)
	fmt.Printf("  Time: %d ms\n", elapsed.Milliseconds())
	fmt.Printf("  Result: %d\n", result)
}
