import time

RUNS = 3

def sum_inv_squares(n):
    s = 0.0
    for i in range(1, n + 1):
        fi = float(i)
        s += 1.0 / (fi * fi)
    return s

def leibniz_pi(n):
    s = 0.0
    sign = 1.0
    for i in range(1, n + 1):
        denom = float(i * 2 - 1)
        s += sign / denom
        sign = -sign
    return s * 4.0

def mandelbrot_iter(cr, ci, max_iter):
    zr = 0.0
    zi = 0.0
    for i in range(max_iter):
        tr = zr * zr - zi * zi + cr
        zi = 2.0 * zr * zi + ci
        zr = tr
        if zr * zr + zi * zi > 4.0:
            return i
    return max_iter

def main():
    print("=== Python Float Benchmarks ===")

    # Benchmark 1: Basel problem
    n = 10000000
    sum_inv_squares(n)  # warmup
    best = float('inf')
    result = 0.0
    for run in range(RUNS):
        start = time.time_ns()
        r = sum_inv_squares(n)
        elapsed = time.time_ns() - start
        if elapsed < best:
            best = elapsed
            result = r
    print(f"Basel sum(1/i^2, 1..{n}):")
    print(f"  Time: {best // 1000000} ms")
    print(f"  Result: {result:.10f}")

    # Benchmark 2: Leibniz pi
    n = 10000000
    leibniz_pi(n)  # warmup
    best = float('inf')
    for run in range(RUNS):
        start = time.time_ns()
        r = leibniz_pi(n)
        elapsed = time.time_ns() - start
        if elapsed < best:
            best = elapsed
            result = r
    print(f"Leibniz pi({n} terms):")
    print(f"  Time: {best // 1000000} ms")
    print(f"  Result: {result:.10f}")

    # Benchmark 3: Mandelbrot grid
    size = 200
    # warmup
    t = 0
    for y in range(size):
        for x in range(size):
            cr = (x - 100.0) / 50.0
            ci = (y - 100.0) / 50.0
            t += mandelbrot_iter(cr, ci, 100)
    best = float('inf')
    total = 0
    for run in range(RUNS):
        start = time.time_ns()
        t = 0
        for y in range(size):
            for x in range(size):
                cr = (x - 100.0) / 50.0
                ci = (y - 100.0) / 50.0
                t += mandelbrot_iter(cr, ci, 100)
        elapsed = time.time_ns() - start
        if elapsed < best:
            best = elapsed
            total = t
    print(f"Mandelbrot {size}x{size}:")
    print(f"  Time: {best // 1000000} ms")
    print(f"  Result: {total}")

if __name__ == "__main__":
    main()
