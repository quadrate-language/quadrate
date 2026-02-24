/**
 * @file exec_result.h
 * @brief Error codes for Quadrate runtime functions
 */

#ifndef QD_QUADRATE_RUNTIME_EXEC_RESULT_H
#define QD_QUADRATE_RUNTIME_EXEC_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standard error codes for runtime functions
 *
 * Runtime functions return int: 0 for success, non-zero (typically negative) for errors.
 */
enum {
	QD_OK = 0,					///< Success
	QD_ERR_GENERIC = -1,		///< Generic error
	QD_ERR_STACK_OVERFLOW = -2, ///< Stack overflow
	QD_ERR_TYPE_MISMATCH = -3,	///< Type mismatch
	QD_ERR_NULL_POINTER = -4,	///< Null pointer
	QD_ERR_INVALID_OP = -5		///< Invalid operation for type
};

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_EXEC_RESULT_H
