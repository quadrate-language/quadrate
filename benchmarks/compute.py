import time
import sys

sys.setrecursionlimit(20000)

def collatz_len(n):
    length = 0
    while n > 1:
        if n % 2 == 0:
            n = n // 2
        else:
            n = n * 3 + 1
        length += 1
    return length

def longest_collatz(limit):
    best = 0
    for i in range(1, limit):
        length = collatz_len(i)
        if length > best:
            best = length
    return best

def is_prime(n):
    if n < 2:
        return False
    if n == 2:
        return True
    if n % 2 == 0:
        return False
    d = 3
    while d * d <= n:
        if n % d == 0:
            return False
        d += 2
    return True

def count_primes(limit):
    count = 0
    for i in range(2, limit):
        if is_prime(i):
            count += 1
    return count

def ack(m, n):
    if m == 0:
        return n + 1
    if n == 0:
        return ack(m - 1, 1)
    return ack(m - 1, ack(m, n - 1))

def popcount(n):
    count = 0
    while n > 0:
        count += n & 1
        n >>= 1
    return count

def total_popcount(limit):
    total = 0
    for i in range(1, limit + 1):
        total += popcount(i)
    return total

def tak(x, y, z):
    if x <= y:
        return z
    return tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y))

def main():
    print("=== Python Compute Benchmarks ===")

    # Benchmark 1: Collatz conjecture
    limit = 1000000
    start = time.time_ns()
    result = longest_collatz(limit)
    elapsed = time.time_ns() - start
    print(f"Collatz longest chain under {limit}:")
    print(f"  Time: {elapsed // 1000000} ms")
    print(f"  Result: {result}")

    # Benchmark 2: Prime counting
    limit = 1000000
    start = time.time_ns()
    result = count_primes(limit)
    elapsed = time.time_ns() - start
    print(f"Prime count under {limit}:")
    print(f"  Time: {elapsed // 1000000} ms")
    print(f"  Result: {result}")

    # Benchmark 3: Ackermann
    m, n = 3, 11
    start = time.time_ns()
    result = ack(m, n)
    elapsed = time.time_ns() - start
    print(f"Ackermann({m}, {n}):")
    print(f"  Time: {elapsed // 1000000} ms")
    print(f"  Result: {result}")

    # Benchmark 4: Popcount
    limit = 10000000
    start = time.time_ns()
    result = total_popcount(limit)
    elapsed = time.time_ns() - start
    print(f"Popcount sum 1..{limit}:")
    print(f"  Time: {elapsed // 1000000} ms")
    print(f"  Result: {result}")

    # Benchmark 5: Takeuchi
    x, y, z = 24, 16, 8
    start = time.time_ns()
    result = tak(x, y, z)
    elapsed = time.time_ns() - start
    print(f"Tak({x}, {y}, {z}):")
    print(f"  Time: {elapsed // 1000000} ms")
    print(f"  Result: {result}")

if __name__ == "__main__":
    main()
