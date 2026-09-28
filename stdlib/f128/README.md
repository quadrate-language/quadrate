# f128

IEEE 754 decimal128 functions (`f128::` module).

## Overview

`f128` is decimal floating point with 34 significant digits. The operators `+ - * /`,
`neg` and the comparisons work on it directly; this module has the other operations
IEEE 754-2008 defines for a decimal format, and the elementary functions.

## Key functions

- `sqrt`, `exp`, `ln`, `log10`, `pow` - Powers and logarithms
- `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2` - Trigonometry
- `fma`, `remainder`, `fmod`, `abs`, `copysign`, `min`, `max` - Arithmetic
- `round_even`, `round`, `trunc`, `floor`, `ceil` - Rounding to an integer
- `quantize`, `reduce`, `scaleb`, `logb`, `next_up`, `next_down` - Exponent and digits
- `is_nan`, `is_inf`, `is_finite`, `is_zero`, `is_normal`, `is_subnormal`, `is_signed`,
  `total_order` - Classification
- `pi`, `e`, `inf`, `nan` - Constants

## Usage

```quadrate
use f128

fn main() {
	2q f128::sqrt print nl        // 1.414213562373095048801688724209698
	0.1q 0.2q + 0.3q == print nl  // 1
}
```
