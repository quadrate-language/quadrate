package main

import (
	"fmt"
	"time"
)

const runs = 3

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

func gcd(a, b int64) int64 {
	for b != 0 {
		a, b = b, a%b
	}
	return a
}

func eulerTotient(n int64) int64 {
	var count int64 = 0
	for i := int64(1); i < n; i++ {
		if gcd(i, n) == 1 {
			count++
		}
	}
	return count
}

func sumTotients(limit int64) int64 {
	var total int64 = 0
	for i := int64(1); i <= limit; i++ {
		total += eulerTotient(i)
	}
	return total
}

func digitSum(n int64) int64 {
	var sum int64 = 0
	for n > 0 {
		sum += n % 10
		n /= 10
	}
	return sum
}

func totalDigitSum(limit int64) int64 {
	var total int64 = 0
	for i := int64(1); i <= limit; i++ {
		total += digitSum(i)
	}
	return total
}

func main() {
	fmt.Println("=== Go Compute Benchmarks ===")

	// Benchmark 1: Collatz conjecture
	limit := int64(1000000)
	longestCollatz(limit) // warmup
	var best int64 = 1 << 62
	var result int64
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := longestCollatz(limit)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			result = r
		}
	}
	fmt.Printf("Collatz longest chain under %d:\n", limit)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 2: Prime counting
	limit = 1000000
	countPrimes(limit) // warmup
	best = 1 << 62
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := countPrimes(limit)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			result = r
		}
	}
	fmt.Printf("Prime count under %d:\n", limit)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 3: Ackermann
	m, n := int64(3), int64(11)
	ack(m, n) // warmup
	best = 1 << 62
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := ack(m, n)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			result = r
		}
	}
	fmt.Printf("Ackermann(%d, %d):\n", m, n)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 4: Popcount
	limit = 10000000
	totalPopcount(limit) // warmup
	best = 1 << 62
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := totalPopcount(limit)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			result = r
		}
	}
	fmt.Printf("Popcount sum 1..%d:\n", limit)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 5: Takeuchi
	x, y, z := int64(24), int64(16), int64(8)
	tak(x, y, z) // warmup
	best = 1 << 62
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := tak(x, y, z)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			result = r
		}
	}
	fmt.Printf("Tak(%d, %d, %d):\n", x, y, z)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 6: Euler totient sum
	limit = 10000
	sumTotients(limit) // warmup
	best = 1 << 62
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := sumTotients(limit)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			result = r
		}
	}
	fmt.Printf("Euler totient sum(1..%d):\n", limit)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %d\n", result)

	// Benchmark 7: Digit sum
	limit = 10000000
	totalDigitSum(limit) // warmup
	best = 1 << 62
	for run := 0; run < runs; run++ {
		start := time.Now()
		r := totalDigitSum(limit)
		elapsed := time.Since(start).Nanoseconds()
		if elapsed < best {
			best = elapsed
			result = r
		}
	}
	fmt.Printf("Digit sum(1..%d):\n", limit)
	fmt.Printf("  Time: %d ms\n", best/1000000)
	fmt.Printf("  Result: %d\n", result)
}
