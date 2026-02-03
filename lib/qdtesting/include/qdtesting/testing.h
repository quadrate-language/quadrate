#ifndef QD_QDTESTING_TESTING_H
#define QD_QDTESTING_TESTING_H

#include <qdrt/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Assert that two values on the stack are equal (any type)
/// @param ctx Quadrate context
/// @return Execution result (0 for success, non-zero for failure)
int usr_testing_assert_eq(qd_context* ctx);

/// Assert that two values on the stack are not equal (any type)
/// @param ctx Quadrate context
/// @return Execution result (0 for success, non-zero for failure)
int usr_testing_assert_ne(qd_context* ctx);

/// Assert that a value on the stack is truthy (non-zero for int, non-empty for str)
/// @param ctx Quadrate context
/// @return Execution result (0 for success, non-zero for failure)
int usr_testing_assert_true(qd_context* ctx);

/// Assert that a value on the stack is falsy (zero for int, empty for str)
/// @param ctx Quadrate context
/// @return Execution result (0 for success, non-zero for failure)
int usr_testing_assert_false(qd_context* ctx);

/// Unconditionally fail a test with a message
/// @param ctx Quadrate context
/// @return Execution result (always non-zero)
int usr_testing_fail(qd_context* ctx);

/// Assert that two floats are approximately equal within epsilon
/// @param ctx Quadrate context
/// @return Execution result (0 for success, non-zero for failure)
int usr_testing_assert_approx_eq(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QDTESTING_H
