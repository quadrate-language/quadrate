package main

import (
	"fmt"
	"time"
)

const runs = 3

func main() {
	fmt.Println("=== Go String Benchmarks ===")
	iterations := 50000

	// Warmup
	s := ""
	for i := 0; i < iterations; i++ {
		s += "a"
	}
	_ = s

	// Best of 3
	var best int64 = -1
	finalLen := 0
	for run := 0; run < runs; run++ {
		s = ""
		start := time.Now()
		for i := 0; i < iterations; i++ {
			s += "a"
		}
		elapsed := time.Since(start)
		ms := elapsed.Milliseconds()
		if best < 0 || ms < best {
			best = ms
		}
		finalLen = len(s)
	}

	fmt.Printf("String Concat (%d iterations):\n", iterations)
	fmt.Printf("  Time: %d ms\n", best)
	fmt.Printf("  Final Length: %d\n", finalLen)
}
