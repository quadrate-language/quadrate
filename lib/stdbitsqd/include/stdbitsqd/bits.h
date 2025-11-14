/**
 * @file bits.h
 * @brief Bitwise operations for Quadrate (bits:: module)
 *
 * Provides bitwise logical and shift operations on integers.
 */

#ifndef QD_STDQD_BITS_H
#define QD_STDQD_BITS_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bitwise AND operation
 * @par Stack Effect: ( a:i b:i -- result:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * 0b1100 0b1010 bits::and  // Result: 0b1000 (8)
 * @endcode
 */
qd_exec_result usr_bits_and(qd_context* ctx);

/**
 * @brief Bitwise OR operation
 * @par Stack Effect: ( a:i b:i -- result:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * 0b1100 0b1010 bits::or  // Result: 0b1110 (14)
 * @endcode
 */
qd_exec_result usr_bits_or(qd_context* ctx);

/**
 * @brief Bitwise XOR operation
 * @par Stack Effect: ( a:i b:i -- result:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * 0b1100 0b1010 bits::xor  // Result: 0b0110 (6)
 * @endcode
 */
qd_exec_result usr_bits_xor(qd_context* ctx);

/**
 * @brief Bitwise NOT operation
 * @par Stack Effect: ( a:i -- result:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * 0b1100 bits::not  // Result: ~0b1100
 * @endcode
 */
qd_exec_result usr_bits_not(qd_context* ctx);

/**
 * @brief Left shift operation
 * @par Stack Effect: ( value:i shift:i -- result:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * 5 2 bits::lshift  // Result: 20 (5 << 2)
 * @endcode
 */
qd_exec_result usr_bits_lshift(qd_context* ctx);

/**
 * @brief Right shift operation
 * @par Stack Effect: ( value:i shift:i -- result:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * 20 2 bits::rshift  // Result: 5 (20 >> 2)
 * @endcode
 */
qd_exec_result usr_bits_rshift(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_STDQD_BITS_H
