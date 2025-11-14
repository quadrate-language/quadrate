/**
 * @file exec_result.h
 * @brief Execution result structure for Quadrate runtime functions
 */

#ifndef QD_QUADRATE_RUNTIME_EXEC_RESULT_H
#define QD_QUADRATE_RUNTIME_EXEC_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result code from runtime function execution
 *
 * Returned by all Quadrate runtime functions to indicate success or failure.
 *
 * @note A code of 0 indicates success; non-zero indicates an error
 */
typedef struct {
	int code;  ///< Result code (0 = success, non-zero = error)
} qd_exec_result;

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_EXEC_RESULT_H
