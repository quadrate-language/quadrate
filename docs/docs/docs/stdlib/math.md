# math

Mathematical functions and constants.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `E` | `2.718281828459045` | Euler's number (e ≈ 2.71828). |
| `Pi` | `3.141592653589793` | Mathematical constant Pi (π ≈ 3.14159). |
| `Tau` | `6.283185307179586` | Tau = 2π (full circle in radians). |

## Functions

### abs

Absolute value.

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | |x| |

**Example:**

```qd
-5.0 math::abs .  // 5.0
```

---

### acos

Arc cosine (inverse cosine).

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value in range [-1, 1] |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [0, π] |

**Example:**

```qd
1.0 math::acos .  // 0.0
```

---

### asin

Arc sine (inverse sine).

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value in range [-1, 1] |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [-π/2, π/2] |

**Example:**

```qd
0.0 math::asin .  // 0.0
```

---

### atan

Arc tangent (inverse tangent).

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [-π/2, π/2] |

**Example:**

```qd
0.0 math::atan .  // 0.0
```

---

### cb

Cube a number.

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value to cube |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | x * x * x |

**Example:**

```qd
2.0 math::cb .  // 8.0
```

---

### cbrt

Cube root.

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Cube root of x |

**Example:**

```qd
8.0 math::cbrt .  // 2.0
```

---

### ceil

Ceiling (round up).

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Smallest integer >= x |

**Example:**

```qd
2.3 math::ceil .  // 3.0
```

---

### clamp

Clamp value to range.

**Signature:** `( x:f64 min_val:f64 max_val:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value to clamp |
| `min_val` | `f64` | Minimum bound |
| `max_val` | `f64` | Maximum bound |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | x clamped to [min_val, max_val] |

**Example:**

```qd
15.0 0.0 10.0 math::clamp .  // 10.0
```

---

### cos

Cosine of angle in radians.

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Angle in radians |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Cosine value [-1, 1] |

**Example:**

```qd
0.0 math::cos .  // 1.0
```

---

### dec

Decrement integer by 1.

**Signature:** `( x:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Value to decrement |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | x - 1 |

**Example:**

```qd
5 math::dec .  // 4
```

---

### deg_to_rad

Convert degrees to radians.

**Signature:** `( degrees:f64 -- radians:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `degrees` | `f64` | Angle in degrees |

| Return | Type | Description |
|--------|------|-------------|
| `radians` | `f64` | Angle in radians |

**Example:**

```qd
180.0 math::deg_to_rad .  // ~3.14159
```

---

### fac

Factorial (n!).

**Signature:** `( n:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Non-negative integer |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | n! |

**Example:**

```qd
5 math::fac .  // 120
```

---

### floor

Floor (round down).

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Largest integer <= x |

**Example:**

```qd
2.7 math::floor .  // 2.0
```

---

### inc

Increment integer by 1.

**Signature:** `( x:i64 -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `i64` | Value to increment |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | x + 1 |

**Example:**

```qd
5 math::inc .  // 6
```

---

### inv

Reciprocal (1/x).

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Non-zero value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | 1/x |

**Example:**

```qd
4.0 math::inv .  // 0.25
```

---

### lerp

Linear interpolation between two values.

**Signature:** `( a:f64 b:f64 t:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `f64` | Start value |
| `b` | `f64` | End value |
| `t` | `f64` | Interpolation factor [0, 1] |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | a + (b - a) * t |

**Example:**

```qd
0.0 10.0 0.5 math::lerp .  // 5.0
```

---

### ln

Natural logarithm (base e).

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Positive value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Natural log of x |

**Example:**

```qd
2.718281828 math::ln .  // ~1.0
```

---

### log

Logarithm with arbitrary base.

**Signature:** `( x:f64 base:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Positive value |
| `base` | `f64` | Logarithm base |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | log_base(x) |

**Example:**

```qd
8.0 2.0 math::log .  // 3.0
```

---

### log10

Base-10 logarithm.

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Positive value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Log base 10 of x |

**Example:**

```qd
100.0 math::log10 .  // 2.0
```

---

### max

Maximum of two values.

**Signature:** `( a:any b:any -- result:any )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `any` | First value |
| `b` | `any` | Second value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `any` | Larger of a and b |

**Example:**

```qd
3 7 math::max .  // 7
```

---

### min

Minimum of two values.

**Signature:** `( a:any b:any -- result:any )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `any` | First value |
| `b` | `any` | Second value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `any` | Smaller of a and b |

**Example:**

```qd
3 7 math::min .  // 3
```

---

### pow

Power function.

**Signature:** `( base:f64 exp:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `base` | `f64` | Base value |
| `exp` | `f64` | Exponent |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | base raised to exp |

**Example:**

```qd
2.0 3.0 math::pow .  // 8.0
```

---

### rad_to_deg

Convert radians to degrees.

**Signature:** `( radians:f64 -- degrees:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `radians` | `f64` | Angle in radians |

| Return | Type | Description |
|--------|------|-------------|
| `degrees` | `f64` | Angle in degrees |

**Example:**

```qd
3.141592653589793 math::rad_to_deg .  // 180.0
```

---

### round

Round to nearest integer.

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Nearest integer to x |

**Example:**

```qd
2.5 math::round .  // 3.0
```

---

### sin

Sine of angle in radians.

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Angle in radians |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Sine value [-1, 1] |

**Example:**

```qd
0.0 math::sin .  // 0.0
```

---

### sq

Square a number.

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value to square |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | x * x |

**Example:**

```qd
3.0 math::sq .  // 9.0
```

---

### sqrt

Square root.

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Non-negative value |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Square root of x |

**Example:**

```qd
4.0 math::sqrt .  // 2.0
```

---

### tan

Tangent of angle in radians.

**Signature:** `( x:f64 -- result:f64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Angle in radians |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Tangent value |

**Example:**

```qd
0.0 math::tan .  // 0.0
```

---

### within

Check if value is within range (inclusive).

**Signature:** `( x:any min_val:any max_val:any -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `any` | Value to check |
| `min_val` | `any` | Minimum bound |
| `max_val` | `any` | Maximum bound |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if in range, 0 otherwise |

**Example:**

```qd
5 0 10 math::within .  // 1
```
