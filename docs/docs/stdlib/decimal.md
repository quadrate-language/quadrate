# `use` decimal

<!-- doccheck: page-context use decimal -->

IEEE 754 decimal128 arithmetic beyond the operators.

`d128` is decimal floating point with 34 significant digits, so a value prints as exactly
what it holds: `0.1d 0.2d +` is `0.3`. The operators `+ - * /`, `neg` and the comparisons
work on d128 directly; this module has the rest of what IEEE 754-2008 asks of a decimal
format, and the elementary functions.

Every result is rounded to 34 digits with roundTiesToEven. `sqrt`, `exp`, `ln` and `log10`
are correctly rounded, as is `fma`, which rounds once for the product and the sum. The
trigonometric functions carry ten guard digits and reduce their argument against 6400
digits of pi, so `sin` of 1e6144 is as accurate as `sin` of 1.

Decimal values keep the exponent they were written or computed with, so `1.10d` and `1.1d`
are equal but print differently; `reduce` strips trailing zeros and `quantize` sets them.

## Functions

### `fn` abs

Absolute value. Clears the sign bit, so -0 becomes 0 and NaN stays NaN.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | \|x\| |

**Example:**

```qd
-2.5d decimal::abs print  // 2.5
```
---

### `fn` acos

Arc cosine, in radians.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value in [-1, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | acos(x) in [0, pi] |
| ` NaN outside [-1, 1]` | `` |  |

**Example:**

```qd
1d decimal::acos print  // 0
```
---

### `fn` asin

Arc sine, in radians.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value in [-1, 1] |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | asin(x) in [-pi/2, pi/2] |
| ` NaN outside [-1, 1]` | `` |  |

**Example:**

```qd
1d decimal::asin print  // 1.570796326794896619231321691639751
```
---

### `fn` atan2

Angle of the point (x, y) from the positive x axis, in radians.

**Signature:** `(y:d128 x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `y` | `d128` | Y coordinate |
| `x` | `d128` | X coordinate |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Angle in [-pi, pi] |

**Example:**

```qd
1d -1d decimal::atan2 print  // 2.356194490192344928846982537459627
```
---

### `fn` atan

Arc tangent, in radians.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Any value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | atan(x) in [-pi/2, pi/2] |

**Example:**

```qd
1d decimal::atan print  // 0.7853981633974483096156608458198757
```
---

### `fn` ceil

Round toward positive infinity to an integral value.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Smallest integer not below x |

**Example:**

```qd
2.1d decimal::ceil print  // 3
```
---

### `fn` copysign

x with the sign of sign.

**Signature:** `(x:d128 sign:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Magnitude |
| `sign` | `d128` | Value whose sign is taken |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | \|x\| with sign's sign |

**Example:**

```qd
3d -1d decimal::copysign print  // -3
```
---

### `fn` cos

Cosine of an angle in radians.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | cos(x) |

**Example:**

```qd
0d decimal::cos print  // 1
```
---

### `fn` e

Euler's number e, correctly rounded to 34 digits.

**Signature:** `( -- result:d128)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | e |

**Example:**

```qd
decimal::e print  // 2.718281828459045235360287471352662
```
---

### `fn` exp

e raised to x, correctly rounded.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Exponent |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | e^x |

**Example:**

```qd
1d decimal::exp print  // 2.718281828459045235360287471352662
```
---

### `fn` floor

Round toward negative infinity to an integral value.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Largest integer not above x |

**Example:**

```qd
-2.1d decimal::floor print  // -3
```
---

### `fn` fma

Fused multiply-add: a*b + c with a single rounding.

**Signature:** `(a:d128 b:d128 c:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `d128` | First factor |
| `b` | `d128` | Second factor |
| `c` | `d128` | Addend |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | a*b + c, rounded once |

**Example:**

```qd
2d 3d 1d decimal::fma print  // 7
```
---

### `fn` fmod

Truncated remainder: x - n*y where n is x/y truncated toward zero. It takes x's sign.

**Signature:** `(x:d128 y:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Dividend |
| `y` | `d128` | Divisor |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Remainder |

**Example:**

```qd
-7d 2d decimal::fmod print  // -1
```
---

### `fn` inf

Positive infinity, which has no literal spelling.

**Signature:** `( -- result:d128)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | inf |

**Example:**

```qd
decimal::inf print  // inf
```
---

### `fn` is_finite

Whether x is neither infinite nor NaN.

**Signature:** `(x:d128 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if finite, 0 otherwise |

**Example:**

```qd
1d decimal::is_finite print  // 1
```
---

### `fn` is_inf

Whether x is an infinity of either sign.

**Signature:** `(x:d128 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if infinite, 0 otherwise |

**Example:**

```qd
1d 0d / decimal::is_inf print  // 1
```
---

### `fn` is_nan

Whether x is NaN. NaN is unequal to everything, itself included, so `x x ==` cannot ask.

**Signature:** `(x:d128 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if NaN, 0 otherwise |

**Example:**

```qd
0d 0d / decimal::is_nan print  // 1
```
---

### `fn` is_normal

Whether x is normal: finite, non-zero, and not subnormal.

**Signature:** `(x:d128 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if normal, 0 otherwise |

**Example:**

```qd
1d decimal::is_normal print  // 1
```
---

### `fn` is_signed

Whether x's sign bit is set. True of -0 and of a negative NaN, which `0d <` is not.

**Signature:** `(x:d128 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if the sign bit is set, 0 otherwise |

**Example:**

```qd
-0d decimal::is_signed print  // 1
```
---

### `fn` is_subnormal

Whether x is subnormal: non-zero and below 1e-6143 in magnitude.

**Signature:** `(x:d128 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if subnormal, 0 otherwise |

**Example:**

```qd
1e-6150d decimal::is_subnormal print  // 1
```
---

### `fn` is_zero

Whether x is zero of either sign.

**Signature:** `(x:d128 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if zero, 0 otherwise |

**Example:**

```qd
-0d decimal::is_zero print  // 1
```
---

### `fn` ln

Natural logarithm, correctly rounded.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Positive value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | ln(x) |
| ` -inf for 0, NaN for a negative x` | `` |  |

**Example:**

```qd
10d decimal::ln print  // 2.302585092994045684017991454684364
```
---

### `fn` log10

Base-10 logarithm, correctly rounded.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Positive value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | log10(x) |

**Example:**

```qd
1000d decimal::log10 print  // 3
```
---

### `fn` logb

Exponent of x's leading digit: floor(log10(|x|)) for a finite non-zero x.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | The exponent |
| ` -inf for 0` | `` |  |

**Example:**

```qd
1234d decimal::logb print  // 3
```
---

### `fn` max

IEEE maxNum: the larger of a and b, where a quiet NaN loses to a number.

**Signature:** `(a:d128 b:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `d128` | First value |
| `b` | `d128` | Second value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Maximum |

**Example:**

```qd
3d 7d decimal::max print  // 7
```
---

### `fn` min

IEEE minNum: the smaller of a and b, where a quiet NaN loses to a number.

**Signature:** `(a:d128 b:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `d128` | First value |
| `b` | `d128` | Second value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Minimum |

**Example:**

```qd
3d 7d decimal::min print  // 3
```
---

### `fn` nan

A quiet NaN, which has no literal spelling.

**Signature:** `( -- result:d128)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | nan |

**Example:**

```qd
decimal::nan decimal::is_nan print  // 1
```
---

### `fn` next_down

The next d128 below x.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Greatest value less than x |

**Example:**

```qd
1d decimal::next_down print  // 0.9999999999999999999999999999999999
```
---

### `fn` next_up

The next d128 above x.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Least value greater than x |

**Example:**

```qd
1d decimal::next_up print  // 1.000000000000000000000000000000001
```
---

### `fn` pi

Pi, correctly rounded to 34 digits.

**Signature:** `( -- result:d128)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Pi |

**Example:**

```qd
decimal::pi print  // 3.141592653589793238462643383279503
```
---

### `fn` pow

base raised to exponent.

**Signature:** `(base:d128 exponent:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `base` | `d128` | Base |
| `exponent` | `d128` | Exponent |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | base^exponent |

**Example:**

```qd
2d 10d decimal::pow print  // 1024
```
---

### `fn` quantize

x rounded to the exponent of like: to the same number of decimal places.

**Signature:** `(x:d128 like:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |
| `like` | `d128` | Value whose exponent is taken |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | x with like's exponent |
| ` NaN if it will not fit 34 digits` | `` |  |

**Example:**

```qd
2.34567d 0.01d decimal::quantize print  // 2.35
```
---

### `fn` reduce

x without trailing zeros: the same value at the largest exponent that holds it.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | x reduced |

**Example:**

```qd
1.500d decimal::reduce print  // 1.5
```
---

### `fn` remainder

IEEE remainder: x - n*y where n is x/y rounded to the nearest integer, ties to even. The result lies in [-|y|/2, |y|/2].

**Signature:** `(x:d128 y:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Dividend |
| `y` | `d128` | Divisor |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Remainder |

**Example:**

```qd
10d 3d decimal::remainder print  // 1
```
---

### `fn` round_even

Round to an integral value, ties to even.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Nearest integer |
| ` 2.5 goes to 2` | `` |  |

**Example:**

```qd
2.5d decimal::round_even print  // 2
```
---

### `fn` round

Round to an integral value, ties away from zero.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Nearest integer |
| ` 2.5 goes to 3` | `` |  |

**Example:**

```qd
2.5d decimal::round print  // 3
```
---

### `fn` scaleb

x times ten to the n.

**Signature:** `(x:d128 n:i64 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |
| `n` | `i64` | Power of ten |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | x * 10^n |

**Example:**

```qd
1.5d 3 decimal::scaleb print  // 1.5e+3
```
---

### `fn` sin

Sine of an angle in radians.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | sin(x) |

**Example:**

```qd
0d decimal::sin print  // 0
```
---

### `fn` sqrt

Square root, correctly rounded.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Non-negative value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Square root of x |
| ` NaN for a negative x` | `` |  |

**Example:**

```qd
2d decimal::sqrt print  // 1.414213562373095048801688724209698
```
---

### `fn` tan

Tangent of an angle in radians.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | tan(x) |

**Example:**

```qd
0d decimal::tan print  // 0
```
---

### `fn` total_order

IEEE totalOrder: a total order over every d128, NaNs and signed zeros included. -0 orders before 0, and equal values written with different exponents order by exponent: 1.00d before 1.0d.

**Signature:** `(a:d128 b:d128 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `d128` | First value |
| `b` | `d128` | Second value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | -1, 0 or 1 |

**Example:**

```qd
-0d 0d decimal::total_order print  // -1
```
---

### `fn` trunc

Round toward zero to an integral value.

**Signature:** `(x:d128 -- result:d128)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `d128` | Value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `d128` | Integer part of x |

**Example:**

```qd
-2.7d decimal::trunc print  // -2
```
