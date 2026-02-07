# `use` math

Mathematical functions and constants.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `E` | `2.718281828459045235` | Euler's number (e). |
| `Ln10` | `2.302585092994045684` | Natural logarithm of 10. |
| `Ln2` | `0.693147180559945309` | Natural logarithm of 2. |
| `Log10E` | `0.434294481903251828` | Base-10 logarithm of e. |
| `Log2E` | `1.442695040888963407` | Base-2 logarithm of e. |
| `Phi` | `1.618033988749894848` | Golden ratio (1 + sqrt(5)) / 2. |
| `Pi` | `3.141592653589793238` | Mathematical constant Pi. |
| `Sqrt2` | `1.414213562373095049` | Square root of 2. |
| `Sqrt3` | `1.732050807568877294` | Square root of 3. |
| `Tau` | `6.283185307179586477` | Tau = 2*Pi (full circle in radians). |

## Functions

### `fn` abs

Absolute value.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Absolute value of x |

**Example:**

```qd
-5.0 math::abs print  // 5.0
```
---

### `fn` acos

Arc cosine (inverse cosine).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value in range [-1, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [0, Pi] |

**Example:**

```qd
1.0 math::acos print  // 0.0
```
---

### `fn` asin

Arc sine (inverse sine).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value in range [-1, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [-Pi/2, Pi/2] |

**Example:**

```qd
0.0 math::asin print  // 0.0
```
---

### `fn` atan2

Two-argument arc tangent. Returns the angle in radians between the positive x-axis and the point (x, y). More useful than atan for computing angles because it handles all quadrants.

**Signature:** `(y:f64 x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `y` | `f64` | Y coordinate |
| `x` | `f64` | X coordinate |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [-Pi, Pi] |

**Example:**

```qd
1.0 1.0 math::atan2 print  // ~0.785 (Pi/4)
```
---

### `fn` atan

Arc tangent (inverse tangent).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [-Pi/2, Pi/2] |

**Example:**

```qd
0.0 math::atan print  // 0.0
```
---

### `fn` cbrt

Cube root.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Cube root of x |

**Example:**

```qd
8.0 math::cbrt print  // 2.0
```
---

### `fn` cb

Cube a number.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value to cube |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | x * x * x |

**Example:**

```qd
2.0 math::cb print  // 8.0
```
---

### `fn` ceil

Ceiling (round up).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Smallest integer >= x |

**Example:**

```qd
2.3 math::ceil print  // 3.0
```
---

### `fn` clamp

Clamp value to range.

**Signature:** `(x:f64 min_val:f64 max_val:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value to clamp |
| `min_val` | `f64` | Minimum bound |
| `max_val` | `f64` | Maximum bound |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | x clamped to [min_val, max_val] |

**Example:**

```qd
15.0 0.0 10.0 math::clamp print  // 10.0
```
---

### `fn` cos

Cosine of angle in radians.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Cosine value [-1, 1] |

**Example:**

```qd
0.0 math::cos print  // 1.0
```
---

### `fn` deg_to_rad

Convert degrees to radians.

**Signature:** `(degrees:f64 -- radians:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `degrees` | `f64` | Angle in degrees |

| Output | Type | Description |
|--------|------|-------------|
| `radians` | `f64` | Angle in radians |

**Example:**

```qd
180.0 math::deg_to_rad print  // ~3.14159
```
---

### `fn` exp

Exponential function (e^x).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Exponent value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | e raised to x |

**Example:**

```qd
1.0 math::exp print  // ~2.718 (e)
```
---

### `fn` fac

Factorial (n!).

**Signature:** `(n:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Non-negative integer |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | n! |

**Example:**

```qd
5 math::fac print  // 120
```
---

### `fn` floor

Floor (round down).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Largest integer <= x |

**Example:**

```qd
2.7 math::floor print  // 2.0
```
---

### `fn` fmod

Floating-point modulo (remainder). Returns the remainder of x/y with the same sign as x.

**Signature:** `(x:f64 y:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Dividend |
| `y` | `f64` | Divisor (non-zero) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Remainder of x/y |

**Example:**

```qd
5.5 2.0 math::fmod print  // 1.5
```
---

### `fn` hypot

Hypotenuse (Euclidean distance). Computes sqrt(x*x + y*y) without intermediate overflow or underflow.

**Signature:** `(x:f64 y:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | First value |
| `y` | `f64` | Second value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | sqrt(x*x + y*y) |

**Example:**

```qd
3.0 4.0 math::hypot print  // 5.0
```
---

### `fn` inv

Reciprocal (1/x).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Non-zero value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | 1/x |

**Example:**

```qd
4.0 math::inv print  // 0.25
```
---

### `fn` lerp

Linear interpolation between two values.

**Signature:** `(a:f64 b:f64 t:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `f64` | Start value |
| `b` | `f64` | End value |
| `t` | `f64` | Interpolation factor [0, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | a + (b - a) * t |

**Example:**

```qd
0.0 10.0 0.5 math::lerp print  // 5.0
```
---

### `fn` ln

Natural logarithm (base e).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Positive value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Natural log of x |

**Example:**

```qd
2.718281828 math::ln print  // ~1.0
```
---

### `fn` log10

Base-10 logarithm.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Positive value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Log base 10 of x |

**Example:**

```qd
100.0 math::log10 print  // 2.0
```
---

### `fn` log

Logarithm with arbitrary base.

**Signature:** `(x:f64 base:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Positive value |
| `base` | `f64` | Logarithm base |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | log_base(x) |

**Example:**

```qd
8.0 2.0 math::log print  // 3.0
```
---

### `fn` max

Maximum of two values.

**Signature:** `(a:any b:any -- result:any)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `any` | First value |
| `b` | `any` | Second value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `any` | Larger of a and b |

**Example:**

```qd
3 7 math::max print  // 7
```
---

### `fn` min

Minimum of two values.

**Signature:** `(a:any b:any -- result:any)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `any` | First value |
| `b` | `any` | Second value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `any` | Smaller of a and b |

**Example:**

```qd
3 7 math::min print  // 3
```
---

### `fn` pow

Power function.

**Signature:** `(base:f64 exp:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `base` | `f64` | Base value |
| `exp` | `f64` | Exponent |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | base raised to exp |

**Example:**

```qd
2.0 3.0 math::pow print  // 8.0
```
---

### `fn` rad_to_deg

Convert radians to degrees.

**Signature:** `(radians:f64 -- degrees:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `radians` | `f64` | Angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `degrees` | `f64` | Angle in degrees |

**Example:**

```qd
3.141592653589793 math::rad_to_deg print  // 180.0
```
---

### `fn` round

Round to nearest integer.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Nearest integer to x |

**Example:**

```qd
2.5 math::round print  // 3.0
```
---

### `fn` sin

Sine of angle in radians.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Sine value [-1, 1] |

**Example:**

```qd
0.0 math::sin print  // 0.0
```
---

### `fn` sqrt

Square root.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Non-negative value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Square root of x |

**Example:**

```qd
4.0 math::sqrt print  // 2.0
```
---

### `fn` sq

Square a number.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value to square |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | x * x |

**Example:**

```qd
3.0 math::sq print  // 9.0
```
---

### `fn` tan

Tangent of angle in radians.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Tangent value |

**Example:**

```qd
0.0 math::tan print  // 0.0
```
---

### `fn` within

Check if value is within range (inclusive).

**Signature:** `(x:any min_val:any max_val:any -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `any` | Value to check |
| `min_val` | `any` | Minimum bound |
| `max_val` | `any` | Maximum bound |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if in range, 0 otherwise |

**Example:**

```qd
5 0 10 math::within print  // 1
```
---

### `fn` sinh

Hyperbolic sine.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Hyperbolic sine of x |

**Example:**

```qd
1.0 math::sinh print  // ~1.175
```
---

### `fn` cosh

Hyperbolic cosine.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Hyperbolic cosine of x |

**Example:**

```qd
0.0 math::cosh print  // 1.0
```
---

### `fn` tanh

Hyperbolic tangent.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Hyperbolic tangent of x [-1, 1] |

**Example:**

```qd
0.0 math::tanh print  // 0.0
```
---

### `fn` asinh

Inverse hyperbolic sine.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Inverse hyperbolic sine of x |

**Example:**

```qd
0.0 math::asinh print  // 0.0
```
---

### `fn` acosh

Inverse hyperbolic cosine.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value >= 1 |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Inverse hyperbolic cosine of x |

**Example:**

```qd
1.0 math::acosh print  // 0.0
```
---

### `fn` atanh

Inverse hyperbolic tangent.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value in range (-1, 1) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Inverse hyperbolic tangent of x |

**Example:**

```qd
0.0 math::atanh print  // 0.0
```
---

### `fn` log2

Base-2 logarithm.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Positive value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Log base 2 of x |

**Example:**

```qd
8.0 math::log2 print  // 3.0
```
---

### `fn` exp2

Exponential base 2 (2^x).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Exponent |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | 2 raised to x |

**Example:**

```qd
3.0 math::exp2 print  // 8.0
```
---

### `fn` trunc

Truncate toward zero.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Integer part of x (toward zero) |

**Example:**

```qd
-2.7 math::trunc print  // -2.0
```
---

### `fn` sign

Sign function.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Input value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | -1 if x < 0, 0 if x == 0, 1 if x > 0 |

**Example:**

```qd
-5.0 math::sign print  // -1.0
```

---

See also: [Vectors](math-vectors.md) (Vec2, Vec3, Vec4) | [Matrices & Quaternions](math-matrices.md) (Mat4, Quat)
