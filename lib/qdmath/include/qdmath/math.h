/**
 * @file math.h
 * @brief Mathematical functions for Quadrate (math:: module)
 *
 * Provides trigonometric, logarithmic, power, and utility mathematical functions.
 */

#ifndef QD_STDQD_MATH_H
#define QD_STDQD_MATH_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Trigonometric Trigonometric Functions
 * @brief Trigonometric functions operating on radians
 * @{
 */

/** @brief Sine function - Stack Effect: ( x:f -- sin(x):f ) */
qd_exec_result usr_math_sin(qd_context* ctx);

/** @brief Cosine function - Stack Effect: ( x:f -- cos(x):f ) */
qd_exec_result usr_math_cos(qd_context* ctx);

/** @brief Tangent function - Stack Effect: ( x:f -- tan(x):f ) */
qd_exec_result usr_math_tan(qd_context* ctx);

/** @brief Arcsine function - Stack Effect: ( x:f -- asin(x):f ) */
qd_exec_result usr_math_asin(qd_context* ctx);

/** @brief Arccosine function - Stack Effect: ( x:f -- acos(x):f ) */
qd_exec_result usr_math_acos(qd_context* ctx);

/** @brief Arctangent function - Stack Effect: ( x:f -- atan(x):f ) */
qd_exec_result usr_math_atan(qd_context* ctx);

/** @} */ // end of Trigonometric group

/**
 * @defgroup PowerRoot Power and Root Functions
 * @brief Exponentiation and root extraction
 * @{
 */

/** @brief Square root - Stack Effect: ( x:f -- sqrt(x):f ) */
qd_exec_result usr_math_sqrt(qd_context* ctx);

/** @brief Square (x²) - Stack Effect: ( x:f -- x²:f ) */
qd_exec_result usr_math_sq(qd_context* ctx);

/** @brief Cube (x³) - Stack Effect: ( x:f -- x³:f ) */
qd_exec_result usr_math_cb(qd_context* ctx);

/** @brief Cube root - Stack Effect: ( x:f -- ∛x:f ) */
qd_exec_result usr_math_cbrt(qd_context* ctx);

/** @brief Power function - Stack Effect: ( base:f exp:f -- base^exp:f ) */
qd_exec_result usr_math_pow(qd_context* ctx);

/** @} */ // end of PowerRoot group

/**
 * @defgroup Logarithmic Logarithmic Functions
 * @brief Natural and base-10 logarithms
 * @{
 */

/** @brief Natural logarithm (ln) - Stack Effect: ( x:f -- ln(x):f ) */
qd_exec_result usr_math_ln(qd_context* ctx);

/** @brief Base-10 logarithm - Stack Effect: ( x:f -- log10(x):f ) */
qd_exec_result usr_math_log10(qd_context* ctx);

/** @} */ // end of Logarithmic group

/**
 * @defgroup Rounding Rounding Functions
 * @brief Functions for rounding floating-point numbers
 * @{
 */

/** @brief Ceiling function - Stack Effect: ( x:f -- ceil(x):f ) */
qd_exec_result usr_math_ceil(qd_context* ctx);

/** @brief Floor function - Stack Effect: ( x:f -- floor(x):f ) */
qd_exec_result usr_math_floor(qd_context* ctx);

/** @brief Round to nearest integer - Stack Effect: ( x:f -- round(x):f ) */
qd_exec_result usr_math_round(qd_context* ctx);

/** @} */ // end of Rounding group

/**
 * @defgroup MathUtility Utility Functions
 * @brief General mathematical utility functions
 * @{
 */

/** @brief Absolute value - Stack Effect: ( x:f -- |x|:f ) */
qd_exec_result usr_math_abs(qd_context* ctx);

/** @brief Minimum of two values - Stack Effect: ( a:f b:f -- min(a,b):f ) */
qd_exec_result usr_math_min(qd_context* ctx);

/** @brief Maximum of two values - Stack Effect: ( a:f b:f -- max(a,b):f ) */
qd_exec_result usr_math_max(qd_context* ctx);

/** @brief Factorial - Stack Effect: ( n:i -- n!:i ) */
qd_exec_result usr_math_fac(qd_context* ctx);

/** @brief Reciprocal (1/x) - Stack Effect: ( x:f -- 1/x:f ) */
qd_exec_result usr_math_inv(qd_context* ctx);

/** @} */ // end of MathUtility group

#ifdef __cplusplus
}
#endif

#endif // QD_STDQD_MATH_H
