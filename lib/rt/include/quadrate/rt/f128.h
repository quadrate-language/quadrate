/**
 * @file f128.h
 * @brief IEEE 754-2008 decimal128 values for the Quadrate runtime
 *
 * `f128` is decimal128: 34 significant decimal digits, exponents from -6143 to +6144,
 * signed zeros, infinities and NaN. It is decimal rather than binary128 so that what a
 * program prints is exactly what it holds -- `0.1q 0.2q +` is `0.3`, not the nearest
 * binary fraction to it -- which is what a calculator needs.
 *
 * A value is a pointer to an immutable, reference-counted box, handled on the stack the
 * way strings are: a slot owns one reference, and every function below that returns a
 * `qd_f128_t*` hands the caller a reference of its own. Every result is rounded to the
 * decimal128 format with roundTiesToEven, as IEEE 754 requires of each operation.
 *
 * The functions that return a new value return NULL only when memory runs out.
 *
 * Arithmetic is done by libmpdec, which is linked into the runtime and not exposed:
 * nothing here needs its headers.
 */

#ifndef QD_QUADRATE_RUNTIME_F128_H
#define QD_QUADRATE_RUNTIME_F128_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief An immutable, reference-counted decimal128 value. */
typedef struct qd_f128 qd_f128_t;

/**
 * @brief Buffer size that holds any value qd_f128_format() writes, terminator included.
 *
 * 34 digits, a sign, a point, and an exponent of up to five characters, or a plain
 * number with up to five leading zeros after the point.
 */
#define QD_F128_STRING_MAX 64

/** @brief Take a reference. NULL is passed through. */
qd_f128_t* qd_f128_retain(qd_f128_t* v);

/** @brief Drop a reference, freeing the value with the last one. NULL is ignored. */
void qd_f128_release(qd_f128_t* v);

/** @brief Current reference count (for tests). */
size_t qd_f128_refcount(const qd_f128_t* v);

/**
 * @brief Read a decimal string, rounding it to 34 digits.
 *
 * Accepts the IEEE 754 decimal character sequences: an optional sign, digits with an
 * optional point, an optional exponent, or `inf`, `infinity` and `nan` in any case.
 * Returns NULL for anything else, so a caller can tell "not a number" from NaN.
 */
qd_f128_t* qd_f128_parse(const char* text);

/** @brief Convert an integer. Exact up to 34 digits, which covers every i64. */
qd_f128_t* qd_f128_from_i64(int64_t v);

/**
 * @brief Convert a binary64, correctly rounded to 34 digits.
 *
 * The binary value is converted, not its shortest spelling: `0.1` as an f64 is
 * 0.1000000000000000055511151231257827.
 */
qd_f128_t* qd_f128_from_f64(double v);

/**
 * @brief Convert to i64, truncating toward zero.
 *
 * Total, as `cast<i64>` must be: out-of-range values saturate at the i64 limits and
 * NaN becomes 0.
 */
int64_t qd_f128_to_i64(const qd_f128_t* v);

/** @brief Convert to the nearest binary64. */
double qd_f128_to_f64(const qd_f128_t* v);

/**
 * @brief Write the IEEE 754 to-scientific-string form: `1.10`, `1.23e+40`, `-inf`, `nan`.
 *
 * The form preserves the quantum, so trailing zeros a value carries are printed: 1.10
 * and 1.1 are equal but distinct. Infinity and NaN are spelled as f64 prints them.
 *
 * @return Characters written, excluding the terminator; `size` of QD_F128_STRING_MAX
 *         always suffices.
 */
size_t qd_f128_format(const qd_f128_t* v, char* buf, size_t size);

/** @name Classification @{ */
bool qd_f128_is_nan(const qd_f128_t* v);
bool qd_f128_is_inf(const qd_f128_t* v);
bool qd_f128_is_finite(const qd_f128_t* v);
bool qd_f128_is_zero(const qd_f128_t* v);
bool qd_f128_is_normal(const qd_f128_t* v);
bool qd_f128_is_subnormal(const qd_f128_t* v);
bool qd_f128_is_signed(const qd_f128_t* v); ///< Sign bit set, including -0 and -nan
/** @} */

/**
 * @brief IEEE comparison.
 * @return -1, 0 or 1, or 2 when either operand is NaN (unordered).
 */
int qd_f128_compare(const qd_f128_t* a, const qd_f128_t* b);

/** @brief IEEE totalOrder: -1, 0 or 1 over every value, NaNs and signed zeros included. */
int qd_f128_total_order(const qd_f128_t* a, const qd_f128_t* b);

/** @name Arithmetic @{ */
qd_f128_t* qd_f128_add(const qd_f128_t* a, const qd_f128_t* b);
qd_f128_t* qd_f128_sub(const qd_f128_t* a, const qd_f128_t* b);
qd_f128_t* qd_f128_mul(const qd_f128_t* a, const qd_f128_t* b);
qd_f128_t* qd_f128_div(const qd_f128_t* a, const qd_f128_t* b);
qd_f128_t* qd_f128_fma(const qd_f128_t* a, const qd_f128_t* b, const qd_f128_t* c); ///< a*b+c, rounded once
qd_f128_t* qd_f128_neg(const qd_f128_t* a);
qd_f128_t* qd_f128_abs(const qd_f128_t* a);
qd_f128_t* qd_f128_copysign(const qd_f128_t* a, const qd_f128_t* sign);
qd_f128_t* qd_f128_remainder(const qd_f128_t* a, const qd_f128_t* b); ///< IEEE remainder: a - n*b, n nearest a/b
qd_f128_t* qd_f128_fmod(const qd_f128_t* a, const qd_f128_t* b);	  ///< a - n*b, n truncated a/b
qd_f128_t* qd_f128_min(const qd_f128_t* a, const qd_f128_t* b);		  ///< minNum: a NaN loses to a number
qd_f128_t* qd_f128_max(const qd_f128_t* a, const qd_f128_t* b);		  ///< maxNum: a NaN loses to a number
qd_f128_t* qd_f128_scaleb(const qd_f128_t* a, int64_t n);			  ///< a * 10^n
qd_f128_t* qd_f128_logb(const qd_f128_t* a);						  ///< Exponent of the leading digit
qd_f128_t* qd_f128_next_up(const qd_f128_t* a);
qd_f128_t* qd_f128_next_down(const qd_f128_t* a);
qd_f128_t* qd_f128_quantize(const qd_f128_t* a, const qd_f128_t* like); ///< a with like's exponent
qd_f128_t* qd_f128_reduce(const qd_f128_t* a);							///< Strip trailing zeros
/** @} */

/** @name Rounding to an integral value @{ */
qd_f128_t* qd_f128_round_even(const qd_f128_t* a); ///< roundToIntegralTiesToEven
qd_f128_t* qd_f128_round(const qd_f128_t* a);	   ///< roundToIntegralTiesToAway
qd_f128_t* qd_f128_trunc(const qd_f128_t* a);	   ///< roundToIntegralTowardZero
qd_f128_t* qd_f128_floor(const qd_f128_t* a);	   ///< roundToIntegralTowardNegative
qd_f128_t* qd_f128_ceil(const qd_f128_t* a);	   ///< roundToIntegralTowardPositive
/** @} */

/**
 * @name Elementary functions
 *
 * sqrt, exp, ln and log10 are correctly rounded. pow is computed from the correctly
 * rounded exp and ln, and is almost always correctly rounded -- libmpdec does not promise
 * more. The trigonometric functions are evaluated with ten guard digits and rounded once.
 * @{
 */
qd_f128_t* qd_f128_sqrt(const qd_f128_t* a);
qd_f128_t* qd_f128_exp(const qd_f128_t* a);
qd_f128_t* qd_f128_ln(const qd_f128_t* a);
qd_f128_t* qd_f128_log10(const qd_f128_t* a);
qd_f128_t* qd_f128_pow(const qd_f128_t* base, const qd_f128_t* exponent);
qd_f128_t* qd_f128_sin(const qd_f128_t* a);
qd_f128_t* qd_f128_cos(const qd_f128_t* a);
qd_f128_t* qd_f128_tan(const qd_f128_t* a);
qd_f128_t* qd_f128_asin(const qd_f128_t* a);
qd_f128_t* qd_f128_acos(const qd_f128_t* a);
qd_f128_t* qd_f128_atan(const qd_f128_t* a);
qd_f128_t* qd_f128_atan2(const qd_f128_t* y, const qd_f128_t* x);
qd_f128_t* qd_f128_pi(void);
qd_f128_t* qd_f128_e(void);
qd_f128_t* qd_f128_inf(void);
qd_f128_t* qd_f128_nan(void);
/** @} */

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_F128_H
