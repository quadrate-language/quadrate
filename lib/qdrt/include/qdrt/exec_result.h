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
 * @brief Standard error codes for runtime functions
 */
enum {
	QD_OK = 0,					///< Success
	QD_ERR_GENERIC = -1,		///< Generic error
	QD_ERR_STACK_OVERFLOW = -2, ///< Stack overflow
	QD_ERR_TYPE_MISMATCH = -3,	///< Type mismatch
	QD_ERR_NULL_POINTER = -4,	///< Null pointer
	QD_ERR_INVALID_OP = -5		///< Invalid operation for type
};

/**
 * @brief Result code from runtime function execution
 *
 * Returned by all Quadrate runtime functions to indicate success or failure.
 *
 * @note A code of 0 indicates success; non-zero indicates an error
 */
typedef struct {
	int code; ///< Result code (0 = success, non-zero = error)
} qd_exec_result;

/** Shorthand for successful result */
#define QD_RESULT_OK ((qd_exec_result){QD_OK})

/** Shorthand for error result */
#define QD_RESULT_ERR(code) ((qd_exec_result){code})

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_EXEC_RESULT_H
