# `use` rand

Random number generation using xorshift64* algorithm.
Fast, high-quality PRNG suitable for most applications.

## Functions

### `fn` new

Create a new RNG seeded from current time.

**Signature:** `( -- rng:Rng)`

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | New random number generator |

**Example:**

```qd
rand::new  // rng
```
---

### `fn` with_seed

Create a new RNG with specific seed.

**Signature:** `(seed:i64 -- rng:Rng)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `seed` | `i64` | Initial seed value (must be non-zero) |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | New random number generator |

**Example:**

```qd
12345 rand::with_seed  // rng
```
## Rng

Random number generator state.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `state` | `i64` | Internal RNG state |

### Methods

#### `fn` boolean

Generate random boolean.

**Signature:** `(rng:Rng) boolean( -- rng2:Rng b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `b` | `i64` | Random boolean (0 or 1) |

**Example:**

```qd
rng rand::boolean  // rng b
```
---

#### `fn` choice

Pick a random element from array.

**Signature:** `(rng:Rng) choice(arr:ptr len:i64 -- rng2:Rng val:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |
| `arr` | `ptr` | Array of i64 values |
| `len` | `i64` | Array length |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `val` | `i64` | Random element from array |

**Example:**

```qd
rng arr len rand::choice  // rng val
```
---

#### `fn` exponential

Generate random f64 from exponential distribution.

**Signature:** `(rng:Rng) exponential(lambda:f64 -- rng2:Rng n:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |
| `lambda` | `f64` | Rate parameter (1/mean) |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `n` | `f64` | Random value from Exp(lambda) |

**Example:**

```qd
rng 1.0 rand::exponential  // rng n
```
---

#### `fn` float_range

Generate random f64 in range [min, max).

**Signature:** `(rng:Rng) float_range(min:f64 max:f64 -- rng2:Rng f:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |
| `min` | `f64` | Lower bound (inclusive) |
| `max` | `f64` | Upper bound (exclusive) |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `f` | `f64` | Random float in [min, max) |

**Example:**

```qd
rng 0.0 10.0 rand::float_range  // rng f
```
---

#### `fn` float

Generate random f64 in range [0.0, 1.0).

**Signature:** `(rng:Rng) float( -- rng2:Rng f:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `f` | `f64` | Random float in [0.0, 1.0) |

**Example:**

```qd
rng rand::float  // rng f
```
---

#### `fn` int

Generate random i64 in range [0, max).

**Signature:** `(rng:Rng) int(max:i64 -- rng2:Rng n:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |
| `max` | `i64` | Upper bound (exclusive) |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `n` | `i64` | Random integer in [0, max) |

**Example:**

```qd
rng 100 rand::int  // rng n
```
---

#### `fn` next

Generate next random i64.

**Signature:** `(rng:Rng) next( -- rng2:Rng n:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `n` | `i64` | Random 64-bit integer |

**Example:**

```qd
rng rand::next  // rng n
```
---

#### `fn` normal_params

Generate random f64 from normal distribution with given mean and stddev.

**Signature:** `(rng:Rng) normal_params(mean:f64 stddev:f64 -- rng2:Rng n:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |
| `mean` | `f64` | Distribution mean |
| `stddev` | `f64` | Distribution standard deviation |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `n` | `f64` | Random value from N(mean, stddev) |

**Example:**

```qd
rng 100.0 15.0 rand::normal_params  // rng n
```
---

#### `fn` normal

Generate random f64 from standard normal distribution (mean=0, stddev=1). Uses Box-Muller transform.

**Signature:** `(rng:Rng) normal( -- rng2:Rng n:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `n` | `f64` | Random value from N(0,1) |

**Example:**

```qd
rng rand::normal  // rng n
```
---

#### `fn` range

Generate random i64 in range [min, max).

**Signature:** `(rng:Rng) range(min:i64 max:i64 -- rng2:Rng n:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |
| `min` | `i64` | Lower bound (inclusive) |
| `max` | `i64` | Upper bound (exclusive) |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |
| `n` | `i64` | Random integer in [min, max) |

**Example:**

```qd
rng 10 20 rand::range  // rng n
```
---

#### `fn` sample

Pick n random elements from array without replacement. Uses partial Fisher-Yates algorithm. Output array must have space for n elements.

**Signature:** `(rng:Rng) sample(src:ptr src_len:i64 dst:ptr n:i64 -- rng2:Rng)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |
| `src` | `ptr` | Source array of i64 values |
| `src_len` | `i64` | Source array length |
| `dst` | `ptr` | Destination array (must have space for n i64 values) |
| `n` | `i64` | Number of elements to pick |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |

**Example:**

```qd
rng src 10 dst 3 rand::sample  // rng
```
---

#### `fn` shuffle

Shuffle array in place using Fisher-Yates algorithm.

**Signature:** `(rng:Rng) shuffle(arr:ptr len:i64 -- rng2:Rng)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `rng` | `ptr` | RNG state |
| `arr` | `ptr` | Array of i64 values |
| `len` | `i64` | Array length |

| Output | Type | Description |
|--------|------|-------------|
| `rng` | `ptr` | Updated RNG state |

**Example:**

```qd
rng arr len rand::shuffle  // rng
```

