package main

import (
	"fmt"
	"time"
)

func main() {
	fmt.Println("=== Go String Benchmarks ===")
	iterations := 50000

	start := time.Now()
	s := ""
	for i := 0; i < iterations; i++ {
		s += "a"
	}
	elapsed := time.Since(start)

	fmt.Printf("String Concat (%d iterations):\n", iterations)
	fmt.Printf("  Time: %d ms\n", elapsed.Milliseconds())
	fmt.Printf("  Final Length: %d\n", len(s))
}
