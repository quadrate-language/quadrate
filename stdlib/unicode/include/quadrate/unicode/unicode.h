/**
 * @file unicode.h
 * @brief Quadrate standard library - unicode module
 *
 * Classification and case mapping for a single Unicode codepoint. The `strings` module
 * answers the same questions about a whole string and shares this module's tables through
 * <quadrate/unicode/codepoint.h>, so the two cannot disagree about a character.
 *
 * The module's character-code constants, and the functions that are legitimately ASCII
 * (hex digits, identifier characters, digit values) or pure encoding mechanics, are
 * implemented in unicode.qd rather than here.
 */

#ifndef QD_UNICODE_UNICODE_H
#define QD_UNICODE_UNICODE_H

#include <quadrate/rt/runtime.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief unicode::is_upper - Stack Effect: ( c:i -- flag:i ) */
int usr_unicode_is_upper(qd_context* ctx);

/** @brief unicode::is_lower - Stack Effect: ( c:i -- flag:i ) */
int usr_unicode_is_lower(qd_context* ctx);

/** @brief unicode::is_alpha - Stack Effect: ( c:i -- flag:i ) */
int usr_unicode_is_alpha(qd_context* ctx);

/** @brief unicode::is_digit - Stack Effect: ( c:i -- flag:i ) */
int usr_unicode_is_digit(qd_context* ctx);

/** @brief unicode::is_alnum - Stack Effect: ( c:i -- flag:i ) */
int usr_unicode_is_alnum(qd_context* ctx);

/** @brief unicode::is_space - Stack Effect: ( c:i -- flag:i ) */
int usr_unicode_is_space(qd_context* ctx);

/** @brief unicode::is_control - Stack Effect: ( c:i -- flag:i ) */
int usr_unicode_is_control(qd_context* ctx);

/** @brief unicode::is_print - Stack Effect: ( c:i -- flag:i ) */
int usr_unicode_is_print(qd_context* ctx);

/** @brief unicode::is_punct - Stack Effect: ( c:i -- flag:i ) */
int usr_unicode_is_punct(qd_context* ctx);

/** @brief unicode::to_lower - Stack Effect: ( c:i -- lowered:i ) */
int usr_unicode_to_lower(qd_context* ctx);

/** @brief unicode::to_upper - Stack Effect: ( c:i -- uppered:i ) */
int usr_unicode_to_upper(qd_context* ctx);

/**
 * @brief Register this module's words on a context
 *
 * For interpreted execution: the compiler resolves these words through the archive, but the
 * interpreter looks them up by name and needs them registered first. Declared in
 * unicode.qd's import block; the table is generated from it, so the two cannot drift.
 *
 * @return false if ctx is NULL or a registration failed
 */
bool qd_unicode_register(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_UNICODE_UNICODE_H
