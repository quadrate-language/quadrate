/**
 * @file f128.h
 * @brief IEEE 754-2008 decimal128 values for the Quadrate runtime
 *
 * `f128` is decimal128: 34 significant decimal digits, exponents from -6143 to +6144,
 * signed zeros, infinities and NaN. It is decimal rather than binary128 so that what a
 * program prints is exactly what it holds -- `0.1q 0.2q +` is `0.3`, not the nearest
 * binary fraction to it -- which is what a calculator needs.
 *
 * A value is sixteen bytes, held and passed like any number: a stack slot holds it
 * whole, and copying one is copying the bytes. Nothing is allocated for it and nothing
 * needs freeing. Every result is rounded to the decimal128 format with roundTiesToEven,
 * as IEEE 754 requires of each operation.
 *
 * Arithmetic is done by libmpdec, which is linked into the runtime and not exposed:
 * nothing here needs its headers. Running out of memory, which only the largest
 * arguments to the trigonometric functions can come near, ends the process, as an
 * allocation failure does elsewhere in the runtime.
 */

#ifndef QD_QUADRATE_RUNTIME_F128_H
#define QD_QUADRATE_RUNTIME_F128_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A decimal128 value in the IEEE 754 densely packed decimal (DPD) encoding.
 *
 * w[0] holds bits 0 to 63 of the encoding and w[1] bits 64 to 127, the sign being the
 * top bit of w[1]. Any 128 bits are a value: non-canonical encodings read as the
 * standard says, as zero.
 */
typedef struct qd_f128 {
	uint64_t w[2];
} qd_f128_t;

/**
 * @brief Buffer size that holds any value qd_f128_format() writes, terminator included.
 *
 * 34 digits, a sign, a point, and an exponent of up to five characters, or a plain
 * number with up to five leading zeros after the point.
 */
#define QD_F128_STRING_MAX 64

/**
 * @brief Read a decimal string, rounding it to 34 digits.
 *
 * Accepts the IEEE 754 decimal character sequences: an optional sign, digits with an
 * optional point, an optional exponent, or `inf`, `infinity` and `nan` in any case.
 * Returns false, leaving `*out` alone, for anything else, so a caller can tell "not a
 * number" from NaN.
 */
bool qd_f128_parse(const char* text, qd_f128_t* out);

/** @brief Convert an integer. Exact up to 34 digits, which covers every i64. */
qd_f128_t qd_f128_from_i64(int64_t v);

/**
 * @brief Convert a binary64, correctly rounded to 34 digits.
 *
 * The binary value is converted, not its shortest spelling: `0.1` as an f64 is
 * 0.1000000000000000055511151231257827.
 */
qd_f128_t qd_f128_from_f64(double v);

/**
 * @brief Convert to i64, truncating toward zero.
 *
 * Total, as `cast<i64>` must be: out-of-range values saturate at the i64 limits and
 * NaN becomes 0.
 */
int64_t qd_f128_to_i64(qd_f128_t v);

/** @brief Convert to the nearest binary64. */
double qd_f128_to_f64(qd_f128_t v);

/**
 * @brief Write the IEEE 754 to-scientific-string form: `1.10`, `1.23e+40`, `-inf`, `nan`.
 *
 * The form preserves the quantum, so trailing zeros a value carries are printed: 1.10
 * and 1.1 are equal but distinct. Infinity and NaN are spelled as f64 prints them.
 *
 * @return Characters written, excluding the terminator; `size` of QD_F128_STRING_MAX
 *         always suffices.
 */
size_t qd_f128_format(qd_f128_t v, char* buf, size_t size);

/** @name Classification @{ */
bool qd_f128_is_nan(qd_f128_t v);
bool qd_f128_is_inf(qd_f128_t v);
bool qd_f128_is_finite(qd_f128_t v);
bool qd_f128_is_zero(qd_f128_t v);
bool qd_f128_is_normal(qd_f128_t v);
bool qd_f128_is_subnormal(qd_f128_t v);
bool qd_f128_is_signed(qd_f128_t v); ///< Sign bit set, including -0 and -nan
/** @} */

/**
 * @brief IEEE comparison.
 * @return -1, 0 or 1, or 2 when either operand is NaN (unordered).
 */
int qd_f128_compare(qd_f128_t a, qd_f128_t b);

/** @brief IEEE totalOrder: -1, 0 or 1 over every value, NaNs and signed zeros included. */
int qd_f128_total_order(qd_f128_t a, qd_f128_t b);

/** @name Arithmetic @{ */
qd_f128_t qd_f128_add(qd_f128_t a, qd_f128_t b);
qd_f128_t qd_f128_sub(qd_f128_t a, qd_f128_t b);
qd_f128_t qd_f128_mul(qd_f128_t a, qd_f128_t b);
qd_f128_t qd_f128_div(qd_f128_t a, qd_f128_t b);
qd_f128_t qd_f128_fma(qd_f128_t a, qd_f128_t b, qd_f128_t c); ///< a*b+c, rounded once
qd_f128_t qd_f128_neg(qd_f128_t a);
qd_f128_t qd_f128_abs(qd_f128_t a);
qd_f128_t qd_f128_copysign(qd_f128_t a, qd_f128_t sign);
qd_f128_t qd_f128_remainder(qd_f128_t a, qd_f128_t b); ///< IEEE remainder: a - n*b, n nearest a/b
qd_f128_t qd_f128_fmod(qd_f128_t a, qd_f128_t b);	   ///< a - n*b, n truncated a/b
qd_f128_t qd_f128_min(qd_f128_t a, qd_f128_t b);	   ///< minNum: a NaN loses to a number
qd_f128_t qd_f128_max(qd_f128_t a, qd_f128_t b);	   ///< maxNum: a NaN loses to a number
qd_f128_t qd_f128_scaleb(qd_f128_t a, int64_t n);	   ///< a * 10^n
qd_f128_t qd_f128_logb(qd_f128_t a);				   ///< Exponent of the leading digit
qd_f128_t qd_f128_next_up(qd_f128_t a);
qd_f128_t qd_f128_next_down(qd_f128_t a);
qd_f128_t qd_f128_quantize(qd_f128_t a, qd_f128_t like); ///< a with like's exponent
qd_f128_t qd_f128_reduce(qd_f128_t a);					 ///< Strip trailing zeros
/** @} */

/** @name Rounding to an integral value @{ */
qd_f128_t qd_f128_round_even(qd_f128_t a); ///< roundToIntegralTiesToEven
qd_f128_t qd_f128_round(qd_f128_t a);	   ///< roundToIntegralTiesToAway
qd_f128_t qd_f128_trunc(qd_f128_t a);	   ///< roundToIntegralTowardZero
qd_f128_t qd_f128_floor(qd_f128_t a);	   ///< roundToIntegralTowardNegative
qd_f128_t qd_f128_ceil(qd_f128_t a);	   ///< roundToIntegralTowardPositive
/** @} */

/**
 * @name Elementary functions
 *
 * sqrt, exp, ln and log10 are correctly rounded. pow is too, except in the rare case its
 * fast evaluation cannot settle the rounding and libmpdec's pow decides, which is almost
 * always correctly rounded -- libmpdec does not promise more. The trigonometric functions
 * are evaluated with ten guard digits and rounded once. An inexact result has all 34
 * digits, as IEEE 754 asks.
 * @{
 */
qd_f128_t qd_f128_sqrt(qd_f128_t a);
qd_f128_t qd_f128_exp(qd_f128_t a);
qd_f128_t qd_f128_ln(qd_f128_t a);
qd_f128_t qd_f128_log10(qd_f128_t a);
qd_f128_t qd_f128_pow(qd_f128_t base, qd_f128_t exponent);
qd_f128_t qd_f128_sin(qd_f128_t a);
qd_f128_t qd_f128_cos(qd_f128_t a);
qd_f128_t qd_f128_tan(qd_f128_t a);
qd_f128_t qd_f128_asin(qd_f128_t a);
qd_f128_t qd_f128_acos(qd_f128_t a);
qd_f128_t qd_f128_atan(qd_f128_t a);
qd_f128_t qd_f128_atan2(qd_f128_t y, qd_f128_t x);
qd_f128_t qd_f128_pi(void);
qd_f128_t qd_f128_e(void);
qd_f128_t qd_f128_inf(void);
qd_f128_t qd_f128_nan(void);
/** @} */

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_F128_H
