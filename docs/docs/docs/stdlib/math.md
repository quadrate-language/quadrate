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
| `Phi` | `1.618033988749894848` | Golden ratio φ = (1 + √5) / 2. |
| `Pi` | `3.141592653589793238` | Mathematical constant Pi (π). |
| `Sqrt2` | `1.414213562373095049` | Square root of 2. |
| `Sqrt3` | `1.732050807568877294` | Square root of 3. |
| `Tau` | `6.283185307179586477` | Tau = 2π (full circle in radians). |

## Functions

### `fn` abs

Absolute value.

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` |  |

| Error | Description |
|-------|-------------|
| `math::x` |  |

**Example:**

```qd
||-5.0 math::abs print  // 5.0
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

### `fn` acos

Arc cosine (inverse cosine).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value in range [-1, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [0, π] |

**Example:**

```qd
1.0 math::acos print  // 0.0
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

### `fn` asin

Arc sine (inverse sine).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Value in range [-1, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [-π/2, π/2] |

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
| `result` | `f64` | Angle in radians [-π, π] |

**Example:**

```qd
1.0 1.0 math::atan2 print  // ~0.785 (π/4)
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

### `fn` atan

Arc tangent (inverse tangent).

**Signature:** `(x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [-π/2, π/2] |

**Example:**

```qd
0.0 math::atan print  // 0.0
```
---

### `fn` bezier2

Evaluate quadratic Bezier curve.

**Signature:** `(p0:f64 p1:f64 p2:f64 t:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `p0` | `f64` | Start point |
| `p1` | `f64` | Control point |
| `p2` | `f64` | End point |
| `t` | `f64` | Parameter [0, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Point on curve |

**Example:**

```qd
0.0 1.0 2.0 0.5 math::bezier2 print  // 1.0
```
---

### `fn` bezier3

Evaluate cubic Bezier curve.

**Signature:** `(p0:f64 p1:f64 p2:f64 p3:f64 t:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `p0` | `f64` | Start point |
| `p1` | `f64` | First control point |
| `p2` | `f64` | Second control point |
| `p3` | `f64` | End point |
| `t` | `f64` | Parameter [0, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Point on curve |

**Example:**

```qd
0.0 0.5 0.5 1.0 0.5 math::bezier3 print
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

### `fn` cubic_hermite

Cubic Hermite spline interpolation. Interpolates between p1 and p2 using tangents m1 and m2.

**Signature:** `(p0:f64 p1:f64 p2:f64 p3:f64 t:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `p0` | `f64` | Point before p1 (used to compute tangent) |
| `p1` | `f64` | Start point |
| `p2` | `f64` | End point |
| `p3` | `f64` | Point after p2 (used to compute tangent) |
| `t` | `f64` | Interpolation factor [0, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Interpolated value |

**Example:**

```qd
0.0 1.0 4.0 5.0 0.5 math::cubic_hermite print
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

Hypotenuse (Euclidean distance). Computes sqrt(x² + y²) without intermediate overflow or underflow.

**Signature:** `(x:f64 y:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | First value |
| `y` | `f64` | Second value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | sqrt(x² + y²) |

**Example:**

```qd
3.0 4.0 math::hypot print  // 5.0
```
---

### `fn` inverse_lerp

Inverse lerp - find t given a value between a and b.

**Signature:** `(a:f64 b:f64 v:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `f64` | Start value |
| `b` | `f64` | End value |
| `v` | `f64` | Value to find t for |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Interpolation factor t where lerp(a, b, t) = v |

**Example:**

```qd
0.0 10.0 5.0 math::inverse_lerp print  // 0.5
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

### `fn` remap

Remap a value from one range to another.

**Signature:** `(in_min:f64 in_max:f64 out_min:f64 out_max:f64 v:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `in_min` | `f64` | Input range minimum |
| `in_max` | `f64` | Input range maximum |
| `out_min` | `f64` | Output range minimum |
| `out_max` | `f64` | Output range maximum |
| `v` | `f64` | Value to remap |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Remapped value |

**Example:**

```qd
0.0 10.0 0.0 100.0 5.0 math::remap print  // 50.0
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

### `fn` smootherstep

Smoother step interpolation (quintic, zero first and second derivatives at edges).

**Signature:** `(edge0:f64 edge1:f64 x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `edge0` | `f64` | Lower edge |
| `edge1` | `f64` | Upper edge |
| `x` | `f64` | Value to interpolate |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Smoothly interpolated value [0, 1] |

**Example:**

```qd
0.0 1.0 0.5 math::smootherstep print  // 0.5
```
---

### `fn` smoothstep

Smooth step interpolation (cubic Hermite, zero derivatives at edges).

**Signature:** `(edge0:f64 edge1:f64 x:f64 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `edge0` | `f64` | Lower edge |
| `edge1` | `f64` | Upper edge |
| `x` | `f64` | Value to interpolate |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Smoothly interpolated value [0, 1] |

**Example:**

```qd
0.0 1.0 0.5 math::smoothstep print  // 0.5
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
## Mat4

4x4 matrix in column-major order (OpenGL compatible).

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `m00` | `f64` | Element at row 0, column 0 |
| `m01` | `f64` | Element at row 1, column 0 |
| `m02` | `f64` | Element at row 2, column 0 |
| `m03` | `f64` | Element at row 3, column 0 |
| `m10` | `f64` | Element at row 0, column 1 |
| `m11` | `f64` | Element at row 1, column 1 |
| `m12` | `f64` | Element at row 2, column 1 |
| `m13` | `f64` | Element at row 3, column 1 |
| `m20` | `f64` | Element at row 0, column 2 |
| `m21` | `f64` | Element at row 1, column 2 |
| `m22` | `f64` | Element at row 2, column 2 |
| `m23` | `f64` | Element at row 3, column 2 |
| `m30` | `f64` | Element at row 0, column 3 |
| `m31` | `f64` | Element at row 1, column 3 |
| `m32` | `f64` | Element at row 2, column 3 |
| `m33` | `f64` | Element at row 3, column 3 |

### Constructors

#### `fn` mat4_from_quat

Create rotation matrix from quaternion.

**Signature:** `(q:Quat -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `q` | `Quat` | Rotation quaternion |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Rotation matrix |

**Example:**

```qd
q mat4_from_quat  // rot_mat
```
---

#### `fn` mat4_identity

Create identity Mat4.

**Signature:** `( -- m:Mat4)`

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Identity matrix |

**Example:**

```qd
math::mat4_identity  // m
```
---

#### `fn` mat4_look_at

Create a look-at view matrix.

**Signature:** `(eye:Vec3 target:Vec3 up:Vec3 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `eye` | `Vec3` | Camera position |
| `target` | `Vec3` | Point to look at |
| `up` | `Vec3` | Up direction |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | View matrix |

**Example:**

```qd
eye target up mat4_look_at  // view
```
---

#### `fn` mat4_orthographic

Create an orthographic projection matrix.

**Signature:** `(left:f64 right:f64 bottom:f64 top:f64 near:f64 far:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `left` | `f64` | Left clipping plane |
| `right` | `f64` | Right clipping plane |
| `bottom` | `f64` | Bottom clipping plane |
| `top` | `f64` | Top clipping plane |
| `near` | `f64` | Near clipping plane |
| `far` | `f64` | Far clipping plane |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Orthographic projection matrix |

**Example:**

```qd
-1.0 1.0 -1.0 1.0 0.1 100.0 mat4_orthographic  // proj
```
---

#### `fn` mat4_perspective

Create a perspective projection matrix.

**Signature:** `(fov:f64 aspect:f64 near:f64 far:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `fov` | `f64` | Field of view in radians |
| `aspect` | `f64` | Aspect ratio (width/height) |
| `near` | `f64` | Near clipping plane |
| `far` | `f64` | Far clipping plane |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Perspective projection matrix |

**Example:**

```qd
1.047 1.777 0.1 100.0 mat4_perspective  // proj
```
---

#### `fn` mat4_rotation_x

Create rotation Mat4 around X axis.

**Signature:** `(angle:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `angle` | `f64` | Rotation angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Rotation matrix |

**Example:**

```qd
1.57 math::mat4_rotation_x  // m
```
---

#### `fn` mat4_rotation_y

Create rotation Mat4 around Y axis.

**Signature:** `(angle:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `angle` | `f64` | Rotation angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Rotation matrix |

**Example:**

```qd
1.57 math::mat4_rotation_y  // m
```
---

#### `fn` mat4_rotation_z

Create rotation Mat4 around Z axis.

**Signature:** `(angle:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `angle` | `f64` | Rotation angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Rotation matrix |

**Example:**

```qd
1.57 math::mat4_rotation_z  // m
```
---

#### `fn` mat4_scaling

Create scale Mat4.

**Signature:** `(sx:f64 sy:f64 sz:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sx` | `f64` | X scale factor |
| `sy` | `f64` | Y scale factor |
| `sz` | `f64` | Z scale factor |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Scale matrix |

**Example:**

```qd
2.0 2.0 2.0 math::mat4_scaling  // m
```
---

#### `fn` mat4_translation

Create translation Mat4.

**Signature:** `(x:f64 y:f64 z:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | X translation |
| `y` | `f64` | Y translation |
| `z` | `f64` | Z translation |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Translation matrix |

**Example:**

```qd
1.0 2.0 3.0 math::mat4_translation  // m
```
---

#### `fn` mat4_zero

Create zero Mat4.

**Signature:** `( -- m:Mat4)`

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Zero matrix |

**Example:**

```qd
math::mat4_zero  // m
```

### Methods

#### `fn` determinant

Matrix determinant.

**Signature:** `(m:Mat4) determinant( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Determinant of matrix |

**Example:**

```qd
m determinant  // det
```
---

#### `fn` inverse

Matrix inverse.

**Signature:** `(m:Mat4) inverse( -- result:Mat4)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Mat4` | Inverse matrix (returns identity if singular) |

**Example:**

```qd
m inverse  // m_inv
```
---

#### `fn` mul

Matrix multiplication.

**Signature:** `(m:Mat4) mul(other:Mat4 -- result:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Mat4` | Matrix to multiply with |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Mat4` | Product of matrices (m * other) |

**Example:**

```qd
other m mul  // product
```
---

#### `fn` transform_point

Transform a Vec3 point by this matrix (w=1).

**Signature:** `(m:Mat4) transform_point(v:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec3` | Point to transform |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Transformed point |

**Example:**

```qd
point m transform_point  // transformed
```
---

#### `fn` transform_vec4

Transform a Vec4 by this matrix.

**Signature:** `(m:Mat4) transform_vec4(v:Vec4 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec4` | Vector to transform |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Transformed vector |

**Example:**

```qd
vec m transform_vec4  // transformed
```
---

#### `fn` transpose

Transpose this matrix.

**Signature:** `(m:Mat4) transpose( -- result:Mat4)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Mat4` | Transposed matrix |

**Example:**

```qd
m transpose  // m_t
```

## Quat

Quaternion for representing rotations. Stored as (w, x, y, z) where w is the scalar part.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `w` | `f64` | Scalar (real) component |
| `x` | `f64` | X component of vector part |
| `y` | `f64` | Y component of vector part |
| `z` | `f64` | Z component of vector part |

### Constructors

#### `fn` quat_from_axis_angle

Create quaternion from axis and angle.

**Signature:** `(axis:Vec3 angle:f64 -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `axis` | `Vec3` | Rotation axis (should be normalized) |
| `angle` | `f64` | Rotation angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Rotation quaternion |

**Example:**

```qd
axis 1.57 quat_from_axis_angle  // q
```
---

#### `fn` quat_from_euler

Create quaternion from Euler angles (XYZ order).

**Signature:** `(pitch:f64 yaw:f64 roll:f64 -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `pitch` | `f64` | Rotation around X axis (radians) |
| `yaw` | `f64` | Rotation around Y axis (radians) |
| `roll` | `f64` | Rotation around Z axis (radians) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Rotation quaternion |

**Example:**

```qd
0.0 1.57 0.0 quat_from_euler  // q
```
---

#### `fn` quat_identity

Create identity quaternion (no rotation).

**Signature:** `( -- q:Quat)`

| Output | Type | Description |
|--------|------|-------------|
| `q` | `Quat` | Identity quaternion |

**Example:**

```qd
math::quat_identity  // q
```

### Methods

#### `fn` conjugate

Conjugate of this quaternion.

**Signature:** `(q:Quat) conjugate( -- result:Quat)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Conjugate quaternion |

**Example:**

```qd
q conjugate  // q_conj
```
---

#### `fn` dot

Dot product with another quaternion.

**Signature:** `(q:Quat) dot(other:Quat -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Quat` | Other quaternion |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
other q dot  // d
```
---

#### `fn` inverse

Inverse of this quaternion.

**Signature:** `(q:Quat) inverse( -- result:Quat)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Inverse quaternion |

**Example:**

```qd
q inverse  // q_inv
```
---

#### `fn` length

Length (magnitude) of this quaternion.

**Signature:** `(q:Quat) length( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of quaternion |

**Example:**

```qd
q length  // len
```
---

#### `fn` length_sq

Squared length of quaternion.

**Signature:** `(q:Quat) length_sq( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Squared length |
---

#### `fn` mul

Quaternion multiplication.

**Signature:** `(q:Quat) mul(other:Quat -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Quat` | Quaternion to multiply with |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Product of this and other |

**Example:**

```qd
other q mul  // product
```
---

#### `fn` normalize

Normalize this quaternion to unit length.

**Signature:** `(q:Quat) normalize( -- result:Quat)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Unit quaternion |

**Example:**

```qd
q normalize  // unit_q
```
---

#### `fn` rotate_vec3

Rotate a Vec3 by this quaternion.

**Signature:** `(q:Quat) rotate_vec3(v:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec3` | Vector to rotate |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Rotated vector |

**Example:**

```qd
v q rotate_vec3  // rotated
```
---

#### `fn` slerp

Spherical linear interpolation to another quaternion.

**Signature:** `(q:Quat) slerp(other:Quat t:f64 -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Quat` | End quaternion |
| `t` | `f64` | Interpolation factor (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Interpolated quaternion |

**Example:**

```qd
other 0.5 q slerp  // mid
```

## Vec2

2D vector with x and y components.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `x` | `f64` | X component |
| `y` | `f64` | Y component |

### Constructors

#### `fn` bezier2_vec2

Evaluate quadratic Bezier curve for Vec2.

**Signature:** `(p0:Vec2 p1:Vec2 p2:Vec2 t:f64 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `p0` | `Vec2` | Start point |
| `p1` | `Vec2` | Control point |
| `p2` | `Vec2` | End point |
| `t` | `f64` | Parameter [0, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Point on curve |
---

#### `fn` bezier3_vec2

Evaluate cubic Bezier curve for Vec2.

**Signature:** `(p0:Vec2 p1:Vec2 p2:Vec2 p3:Vec2 t:f64 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `p0` | `Vec2` | Start point |
| `p1` | `Vec2` | First control point |
| `p2` | `Vec2` | Second control point |
| `p3` | `Vec2` | End point |
| `t` | `f64` | Parameter [0, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Point on curve |
---

#### `fn` vec2_one

Create a Vec2 with all components set to one.

**Signature:** `( -- v:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec2` | One vector (1, 1) |

**Example:**

```qd
math::vec2_one  // v
```
---

#### `fn` vec2_zero

Create a Vec2 with all components set to zero.

**Signature:** `( -- v:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec2` | Zero vector (0, 0) |

**Example:**

```qd
math::vec2_zero  // v
```

### Methods

#### `fn` add

Add another Vec2 to this vector.

**Signature:** `(v:Vec2) add(other:Vec2 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec2` | Vector to add |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Sum of vectors |

**Example:**

```qd
other v add  // sum
```
---

#### `fn` angle

Angle of this vector from positive X axis.

**Signature:** `(v:Vec2) angle( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [-π, π] |

**Example:**

```qd
v angle  // a
```
---

#### `fn` distance

Distance to another Vec2.

**Signature:** `(v:Vec2) distance(other:Vec2 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec2` | Other point |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Distance between points |

**Example:**

```qd
other v distance  // d
```
---

#### `fn` dot

Dot product with another Vec2.

**Signature:** `(v:Vec2) dot(other:Vec2 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec2` | Other vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
other v dot  // d
```
---

#### `fn` length_sq

Squared length of this vector (avoids sqrt).

**Signature:** `(v:Vec2) length_sq( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Squared length of vector |

**Example:**

```qd
v length_sq  // len_sq
```
---

#### `fn` length

Length (magnitude) of this vector.

**Signature:** `(v:Vec2) length( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of vector |

**Example:**

```qd
v length  // len
```
---

#### `fn` lerp

Linear interpolation to another Vec2.

**Signature:** `(v:Vec2) lerp(other:Vec2 t:f64 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec2` | End vector |
| `t` | `f64` | Interpolation factor (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Interpolated vector |

**Example:**

```qd
other 0.5 v lerp  // mid
```
---

#### `fn` neg

Negate this vector.

**Signature:** `(v:Vec2) neg( -- result:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Negated vector |

**Example:**

```qd
v neg  // neg_v
```
---

#### `fn` normalize

Normalize this vector to unit length.

**Signature:** `(v:Vec2) normalize( -- result:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Unit vector in same direction |

**Example:**

```qd
v normalize  // unit
```
---

#### `fn` perpendicular

Get perpendicular vector (90° counterclockwise rotation).

**Signature:** `(v:Vec2) perpendicular( -- result:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Perpendicular vector |

**Example:**

```qd
v perpendicular  // perp
```
---

#### `fn` rotate

Rotate this vector by an angle.

**Signature:** `(v:Vec2) rotate(angle:f64 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `angle` | `f64` | Rotation angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Rotated vector |

**Example:**

```qd
1.57 v rotate  // rotated
```
---

#### `fn` scale

Multiply this vector by a scalar.

**Signature:** `(v:Vec2) scale(s:f64 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `f64` | Scalar multiplier |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Scaled vector |

**Example:**

```qd
2.0 v scale  // scaled
```
---

#### `fn` subtract

Subtract another Vec2 from this vector.

**Signature:** `(v:Vec2) subtract(other:Vec2 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec2` | Vector to subtract |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Difference of vectors |

**Example:**

```qd
other v subtract  // diff
```

## Vec3

3D vector with x, y, and z components.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `x` | `f64` | X component |
| `y` | `f64` | Y component |
| `z` | `f64` | Z component |

### Constructors

#### `fn` bezier3_vec3

Evaluate cubic Bezier curve for Vec3.

**Signature:** `(p0:Vec3 p1:Vec3 p2:Vec3 p3:Vec3 t:f64 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `p0` | `Vec3` | Start point |
| `p1` | `Vec3` | First control point |
| `p2` | `Vec3` | Second control point |
| `p3` | `Vec3` | End point |
| `t` | `f64` | Parameter [0, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Point on curve |
---

#### `fn` vec3_one

Create a Vec3 with all components set to one.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | One vector (1, 1, 1) |

**Example:**

```qd
math::vec3_one  // v
```
---

#### `fn` vec3_unit_x

Unit vector along the X axis.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Unit X vector (1, 0, 0) |

**Example:**

```qd
math::vec3_unit_x  // v
```
---

#### `fn` vec3_unit_y

Unit vector along the Y axis.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Unit Y vector (0, 1, 0) |

**Example:**

```qd
math::vec3_unit_y  // v
```
---

#### `fn` vec3_unit_z

Unit vector along the Z axis.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Unit Z vector (0, 0, 1) |

**Example:**

```qd
math::vec3_unit_z  // v
```
---

#### `fn` vec3_zero

Create a Vec3 with all components set to zero.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Zero vector (0, 0, 0) |

**Example:**

```qd
math::vec3_zero  // v
```

### Methods

#### `fn` add

Add another Vec3 to this vector.

**Signature:** `(v:Vec3) add(other:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Vector to add |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Sum of vectors |

**Example:**

```qd
other v add  // sum
```
---

#### `fn` angle_between

Angle between this vector and another.

**Signature:** `(v:Vec3) angle_between(other:Vec3 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Other vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [0, π] |

**Example:**

```qd
other v angle_between  // a
```
---

#### `fn` cross

Cross product with another Vec3.

**Signature:** `(v:Vec3) cross(other:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Other vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Cross product (v x other) |

**Example:**

```qd
other v cross  // c
```
---

#### `fn` distance

Distance to another Vec3.

**Signature:** `(v:Vec3) distance(other:Vec3 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Other point |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Distance between points |

**Example:**

```qd
other v distance  // d
```
---

#### `fn` dot

Dot product with another Vec3.

**Signature:** `(v:Vec3) dot(other:Vec3 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Other vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
other v dot  // d
```
---

#### `fn` length_sq

Squared length of this vector (avoids sqrt).

**Signature:** `(v:Vec3) length_sq( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Squared length of vector |

**Example:**

```qd
v length_sq  // len_sq
```
---

#### `fn` length

Length (magnitude) of this vector.

**Signature:** `(v:Vec3) length( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of vector |

**Example:**

```qd
v length  // len
```
---

#### `fn` lerp

Linear interpolation to another Vec3.

**Signature:** `(v:Vec3) lerp(other:Vec3 t:f64 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | End vector |
| `t` | `f64` | Interpolation factor (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Interpolated vector |

**Example:**

```qd
other 0.5 v lerp  // mid
```
---

#### `fn` neg

Negate this vector.

**Signature:** `(v:Vec3) neg( -- result:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Negated vector |

**Example:**

```qd
v neg  // neg_v
```
---

#### `fn` normalize

Normalize this vector to unit length.

**Signature:** `(v:Vec3) normalize( -- result:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Unit vector in same direction |

**Example:**

```qd
v normalize  // unit
```
---

#### `fn` reflect

Reflect this vector around a normal.

**Signature:** `(v:Vec3) reflect(n:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `Vec3` | Normal vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Reflected vector |

**Example:**

```qd
normal v reflect  // reflected
```
---

#### `fn` scale

Multiply this vector by a scalar.

**Signature:** `(v:Vec3) scale(s:f64 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `f64` | Scalar multiplier |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Scaled vector |

**Example:**

```qd
2.0 v scale  // scaled
```
---

#### `fn` subtract

Subtract another Vec3 from this vector.

**Signature:** `(v:Vec3) subtract(other:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Vector to subtract |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Difference of vectors |

**Example:**

```qd
other v subtract  // diff
```

## Vec4

4D vector with x, y, z, and w components.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `x` | `f64` | X component |
| `y` | `f64` | Y component |
| `z` | `f64` | Z component |
| `w` | `f64` | W component |

### Constructors

#### `fn` vec4_one

Create a Vec4 with all components set to one.

**Signature:** `( -- v:Vec4)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec4` | One vector (1, 1, 1, 1) |

**Example:**

```qd
math::vec4_one  // v
```
---

#### `fn` vec4_zero

Create a Vec4 with all components set to zero.

**Signature:** `( -- v:Vec4)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec4` | Zero vector (0, 0, 0, 0) |

**Example:**

```qd
math::vec4_zero  // v
```

### Methods

#### `fn` add

Add another Vec4 to this vector.

**Signature:** `(v:Vec4) add(other:Vec4 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec4` | Vector to add |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Sum of vectors |

**Example:**

```qd
other v add  // sum
```
---

#### `fn` dot

Dot product with another Vec4.

**Signature:** `(v:Vec4) dot(other:Vec4 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec4` | Other vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
other v dot  // d
```
---

#### `fn` length_sq

Squared length of this vector (avoids sqrt).

**Signature:** `(v:Vec4) length_sq( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Squared length of vector |

**Example:**

```qd
v length_sq  // len_sq
```
---

#### `fn` length

Length (magnitude) of this vector.

**Signature:** `(v:Vec4) length( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of vector |

**Example:**

```qd
v length  // len
```
---

#### `fn` lerp

Linear interpolation to another Vec4.

**Signature:** `(v:Vec4) lerp(other:Vec4 t:f64 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec4` | End vector |
| `t` | `f64` | Interpolation factor (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Interpolated vector |

**Example:**

```qd
other 0.5 v lerp  // mid
```
---

#### `fn` neg

Negate this vector.

**Signature:** `(v:Vec4) neg( -- result:Vec4)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Negated vector |

**Example:**

```qd
v neg  // neg_v
```
---

#### `fn` normalize

Normalize this vector to unit length.

**Signature:** `(v:Vec4) normalize( -- result:Vec4)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Unit vector in same direction |

**Example:**

```qd
v normalize  // unit
```
---

#### `fn` scale

Multiply this vector by a scalar.

**Signature:** `(v:Vec4) scale(s:f64 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `f64` | Scalar multiplier |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Scaled vector |

**Example:**

```qd
2.0 v scale  // scaled
```
---

#### `fn` subtract

Subtract another Vec4 from this vector.

**Signature:** `(v:Vec4) subtract(other:Vec4 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec4` | Vector to subtract |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Difference of vectors |

**Example:**

```qd
other v subtract  // diff
```

