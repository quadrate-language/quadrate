# rand

Random number generation using xorshift64* algorithm.
Fast, high-quality PRNG suitable for most applications.

## Structs

### Rng

RNG state struct.

## Functions

### bool

Generate random boolean.

**Signature:** `( rng:ptr -- rng:ptr b:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |

| Return | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `b` | `i64` | Random boolean (0 or 1) |

**Example:**

```qd
rng rand::bool  // rng b
```

---

### int

Generate random i64 in range [0, max).

**Signature:** `( rng:ptr max:i64 -- rng:ptr n:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |
| `max` | `i64` | Upper bound (exclusive) |

| Return | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `n` | `i64` | Random integer in [0, max) |

**Example:**

```qd
rng 100 rand::int  // rng n
```

---

### new

Create a new RNG seeded from current time.

**Signature:** `( -- rng:ptr )`

| Return | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | New random number generator |

**Example:**

```qd
rand::new  // rng
```

---

### next

Generate next random i64.

**Signature:** `( rng:ptr -- rng:ptr n:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |

| Return | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `n` | `i64` | Random 64-bit integer |

**Example:**

```qd
rng rand::next  // rng n
```

---

### range

Generate random i64 in range [min, max).

**Signature:** `( rng:ptr min:i64 max:i64 -- rng:ptr n:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |
| `min` | `i64` | Lower bound (inclusive) |
| `max` | `i64` | Upper bound (exclusive) |

| Return | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `n` | `i64` | Random integer in [min, max) |

**Example:**

```qd
rng 10 20 rand::range  // rng n
```

---

### with_seed

Create a new RNG with specific seed.

**Signature:** `( seed:i64 -- rng:ptr )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `seed` | `i64` | Initial seed value (must be non-zero) |

| Return | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | New random number generator |

**Example:**

```qd
12345 rand::with_seed  // rng
```
