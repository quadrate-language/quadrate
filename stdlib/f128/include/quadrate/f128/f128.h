/**
 * @file f128.h
 * @brief IEEE 754 decimal128 functions for Quadrate (f128:: module)
 *
 * The stack words over the runtime's decimal128 arithmetic; see quadrate/rt/f128.h for the
 * functions themselves and what each one rounds.
 */

#ifndef QD_QDF128_F128_H
#define QD_QDF128_F128_H

#include <quadrate/rt/context.h>
#include <quadrate/rt/exec_result.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief f128::sqrt - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_sqrt(qd_context* ctx);

/** @brief f128::exp - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_exp(qd_context* ctx);

/** @brief f128::ln - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_ln(qd_context* ctx);

/** @brief f128::log10 - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_log10(qd_context* ctx);

/** @brief f128::pow - Stack Effect: ( base:f128 exponent:f128 -- result:f128 ) */
int usr_f128_pow(qd_context* ctx);

/** @brief f128::sin - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_sin(qd_context* ctx);

/** @brief f128::cos - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_cos(qd_context* ctx);

/** @brief f128::tan - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_tan(qd_context* ctx);

/** @brief f128::asin - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_asin(qd_context* ctx);

/** @brief f128::acos - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_acos(qd_context* ctx);

/** @brief f128::atan - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_atan(qd_context* ctx);

/** @brief f128::atan2 - Stack Effect: ( y:f128 x:f128 -- result:f128 ) */
int usr_f128_atan2(qd_context* ctx);

/** @brief f128::fma - Stack Effect: ( a:f128 b:f128 c:f128 -- result:f128 ) */
int usr_f128_fma(qd_context* ctx);

/** @brief f128::abs - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_abs(qd_context* ctx);

/** @brief f128::copysign - Stack Effect: ( x:f128 sign:f128 -- result:f128 ) */
int usr_f128_copysign(qd_context* ctx);

/** @brief f128::remainder - Stack Effect: ( x:f128 y:f128 -- result:f128 ) */
int usr_f128_remainder(qd_context* ctx);

/** @brief f128::fmod - Stack Effect: ( x:f128 y:f128 -- result:f128 ) */
int usr_f128_fmod(qd_context* ctx);

/** @brief f128::min - Stack Effect: ( a:f128 b:f128 -- result:f128 ) */
int usr_f128_min(qd_context* ctx);

/** @brief f128::max - Stack Effect: ( a:f128 b:f128 -- result:f128 ) */
int usr_f128_max(qd_context* ctx);

/** @brief f128::scaleb - Stack Effect: ( x:f128 n:i64 -- result:f128 ) */
int usr_f128_scaleb(qd_context* ctx);

/** @brief f128::logb - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_logb(qd_context* ctx);

/** @brief f128::next_up - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_next_up(qd_context* ctx);

/** @brief f128::next_down - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_next_down(qd_context* ctx);

/** @brief f128::quantize - Stack Effect: ( x:f128 like:f128 -- result:f128 ) */
int usr_f128_quantize(qd_context* ctx);

/** @brief f128::reduce - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_reduce(qd_context* ctx);

/** @brief f128::round_even - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_round_even(qd_context* ctx);

/** @brief f128::round - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_round(qd_context* ctx);

/** @brief f128::trunc - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_trunc(qd_context* ctx);

/** @brief f128::floor - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_floor(qd_context* ctx);

/** @brief f128::ceil - Stack Effect: ( x:f128 -- result:f128 ) */
int usr_f128_ceil(qd_context* ctx);

/** @brief f128::is_nan - Stack Effect: ( x:f128 -- result:i64 ) */
int usr_f128_is_nan(qd_context* ctx);

/** @brief f128::is_inf - Stack Effect: ( x:f128 -- result:i64 ) */
int usr_f128_is_inf(qd_context* ctx);

/** @brief f128::is_finite - Stack Effect: ( x:f128 -- result:i64 ) */
int usr_f128_is_finite(qd_context* ctx);

/** @brief f128::is_zero - Stack Effect: ( x:f128 -- result:i64 ) */
int usr_f128_is_zero(qd_context* ctx);

/** @brief f128::is_normal - Stack Effect: ( x:f128 -- result:i64 ) */
int usr_f128_is_normal(qd_context* ctx);

/** @brief f128::is_subnormal - Stack Effect: ( x:f128 -- result:i64 ) */
int usr_f128_is_subnormal(qd_context* ctx);

/** @brief f128::is_signed - Stack Effect: ( x:f128 -- result:i64 ) */
int usr_f128_is_signed(qd_context* ctx);

/** @brief f128::total_order - Stack Effect: ( a:f128 b:f128 -- result:i64 ) */
int usr_f128_total_order(qd_context* ctx);

/** @brief f128::pi - Stack Effect: ( -- result:f128 ) */
int usr_f128_pi(qd_context* ctx);

/** @brief f128::e - Stack Effect: ( -- result:f128 ) */
int usr_f128_e(qd_context* ctx);

/** @brief f128::inf - Stack Effect: ( -- result:f128 ) */
int usr_f128_inf(qd_context* ctx);

/** @brief f128::nan - Stack Effect: ( -- result:f128 ) */
int usr_f128_nan(qd_context* ctx);

/**
 * @brief Register every f128:: word on a context, for the interpreter
 * @return false if ctx is NULL or registration fails
 */
bool qd_f128_register(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_QDF128_F128_H
