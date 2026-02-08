import time

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
    start = time.time_ns()
    result = sum_inv_squares(n)
    elapsed = time.time_ns() - start
    print(f"Basel sum(1/i^2, 1..{n}):")
    print(f"  Time: {elapsed // 1000000} ms")
    print(f"  Result: {result:.10f}")

    # Benchmark 2: Leibniz pi
    n = 10000000
    start = time.time_ns()
    result = leibniz_pi(n)
    elapsed = time.time_ns() - start
    print(f"Leibniz pi({n} terms):")
    print(f"  Time: {elapsed // 1000000} ms")
    print(f"  Result: {result:.10f}")

    # Benchmark 3: Mandelbrot grid
    size = 200
    start = time.time_ns()
    total = 0
    for y in range(size):
        for x in range(size):
            cr = (x - 100.0) / 50.0
            ci = (y - 100.0) / 50.0
            total += mandelbrot_iter(cr, ci, 100)
    elapsed = time.time_ns() - start
    print(f"Mandelbrot {size}x{size}:")
    print(f"  Time: {elapsed // 1000000} ms")
    print(f"  Result: {total}")

if __name__ == "__main__":
    main()
