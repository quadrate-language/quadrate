/**
 * @file decimal.h
 * @brief IEEE 754 decimal128 functions for Quadrate (decimal:: module)
 *
 * The stack words over the runtime's decimal128 arithmetic; see quadrate/rt/d128.h for the
 * functions themselves and what each one rounds.
 */

#ifndef QD_QDDECIMAL_DECIMAL_H
#define QD_QDDECIMAL_DECIMAL_H

#include <quadrate/rt/context.h>
#include <quadrate/rt/exec_result.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief decimal::sqrt - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_sqrt(qd_context* ctx);

/** @brief decimal::exp - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_exp(qd_context* ctx);

/** @brief decimal::ln - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_ln(qd_context* ctx);

/** @brief decimal::log10 - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_log10(qd_context* ctx);

/** @brief decimal::pow - Stack Effect: ( base:d128 exponent:d128 -- result:d128 ) */
int usr_decimal_pow(qd_context* ctx);

/** @brief decimal::sin - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_sin(qd_context* ctx);

/** @brief decimal::cos - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_cos(qd_context* ctx);

/** @brief decimal::tan - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_tan(qd_context* ctx);

/** @brief decimal::asin - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_asin(qd_context* ctx);

/** @brief decimal::acos - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_acos(qd_context* ctx);

/** @brief decimal::atan - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_atan(qd_context* ctx);

/** @brief decimal::atan2 - Stack Effect: ( y:d128 x:d128 -- result:d128 ) */
int usr_decimal_atan2(qd_context* ctx);

/** @brief decimal::fma - Stack Effect: ( a:d128 b:d128 c:d128 -- result:d128 ) */
int usr_decimal_fma(qd_context* ctx);

/** @brief decimal::abs - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_abs(qd_context* ctx);

/** @brief decimal::copysign - Stack Effect: ( x:d128 sign:d128 -- result:d128 ) */
int usr_decimal_copysign(qd_context* ctx);

/** @brief decimal::remainder - Stack Effect: ( x:d128 y:d128 -- result:d128 ) */
int usr_decimal_remainder(qd_context* ctx);

/** @brief decimal::fmod - Stack Effect: ( x:d128 y:d128 -- result:d128 ) */
int usr_decimal_fmod(qd_context* ctx);

/** @brief decimal::min - Stack Effect: ( a:d128 b:d128 -- result:d128 ) */
int usr_decimal_min(qd_context* ctx);

/** @brief decimal::max - Stack Effect: ( a:d128 b:d128 -- result:d128 ) */
int usr_decimal_max(qd_context* ctx);

/** @brief decimal::scaleb - Stack Effect: ( x:d128 n:i64 -- result:d128 ) */
int usr_decimal_scaleb(qd_context* ctx);

/** @brief decimal::logb - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_logb(qd_context* ctx);

/** @brief decimal::next_up - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_next_up(qd_context* ctx);

/** @brief decimal::next_down - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_next_down(qd_context* ctx);

/** @brief decimal::quantize - Stack Effect: ( x:d128 like:d128 -- result:d128 ) */
int usr_decimal_quantize(qd_context* ctx);

/** @brief decimal::reduce - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_reduce(qd_context* ctx);

/** @brief decimal::round_even - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_round_even(qd_context* ctx);

/** @brief decimal::round - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_round(qd_context* ctx);

/** @brief decimal::trunc - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_trunc(qd_context* ctx);

/** @brief decimal::floor - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_floor(qd_context* ctx);

/** @brief decimal::ceil - Stack Effect: ( x:d128 -- result:d128 ) */
int usr_decimal_ceil(qd_context* ctx);

/** @brief decimal::is_nan - Stack Effect: ( x:d128 -- result:i64 ) */
int usr_decimal_is_nan(qd_context* ctx);

/** @brief decimal::is_inf - Stack Effect: ( x:d128 -- result:i64 ) */
int usr_decimal_is_inf(qd_context* ctx);

/** @brief decimal::is_finite - Stack Effect: ( x:d128 -- result:i64 ) */
int usr_decimal_is_finite(qd_context* ctx);

/** @brief decimal::is_zero - Stack Effect: ( x:d128 -- result:i64 ) */
int usr_decimal_is_zero(qd_context* ctx);

/** @brief decimal::is_normal - Stack Effect: ( x:d128 -- result:i64 ) */
int usr_decimal_is_normal(qd_context* ctx);

/** @brief decimal::is_subnormal - Stack Effect: ( x:d128 -- result:i64 ) */
int usr_decimal_is_subnormal(qd_context* ctx);

/** @brief decimal::is_signed - Stack Effect: ( x:d128 -- result:i64 ) */
int usr_decimal_is_signed(qd_context* ctx);

/** @brief decimal::total_order - Stack Effect: ( a:d128 b:d128 -- result:i64 ) */
int usr_decimal_total_order(qd_context* ctx);

/** @brief decimal::pi - Stack Effect: ( -- result:d128 ) */
int usr_decimal_pi(qd_context* ctx);

/** @brief decimal::e - Stack Effect: ( -- result:d128 ) */
int usr_decimal_e(qd_context* ctx);

/** @brief decimal::inf - Stack Effect: ( -- result:d128 ) */
int usr_decimal_inf(qd_context* ctx);

/** @brief decimal::nan - Stack Effect: ( -- result:d128 ) */
int usr_decimal_nan(qd_context* ctx);

/**
 * @brief Register every decimal:: word on a context, for the interpreter
 * @return false if ctx is NULL or registration fails
 */
bool qd_decimal_register(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_QDDECIMAL_DECIMAL_H
