/**
 * @file io.h
 * @brief File I/O operations for the Quadrate standard library
 *
 * This module provides functions for reading from and writing to files.
 * File handles are represented as pointers on the stack.
 */

#ifndef QD_QDIO_IO_H
#define QD_QDIO_IO_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

/**
 * @brief Error codes for io module
 *
 * These match the constants defined in io/module.qd
 * Ok=1, Err=0 are reserved, specific errors start at 2
 */
#define IO_ERR_OK 1				/**< Success (matches builtin Ok) */
#define IO_ERR_NOT_FOUND 2		/**< File not found */
#define IO_ERR_PERMISSION 3		/**< Permission denied */
#define IO_ERR_INVALID_HANDLE 4 /**< Invalid file handle */
#define IO_ERR_READ 5			/**< Read operation failed */
#define IO_ERR_WRITE 6			/**< Write operation failed */
#define IO_ERR_SEEK 7			/**< Seek operation failed */
#define IO_ERR_EOF 8			/**< End of file reached */
#define IO_ERR_INVALID_ARG 9	/**< Invalid argument */

/**
 * @brief Open a file for reading, writing, or both
 *
 * Stack effect: ( path:s mode:s -- handle:p )
 *
 * Modes:
 * - "r"  : Open for reading (file must exist)
 * - "w"  : Open for writing (truncates existing file)
 * - "a"  : Open for appending (creates if doesn't exist)
 * - "r+" : Open for reading and writing (file must exist)
 * - "w+" : Open for reading and writing (truncates existing file)
 * - "a+" : Open for reading and appending (creates if doesn't exist)
 *
 * Returns a file handle pointer on success, or null pointer on failure.
 * Call io::error to get error message if null is returned.
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_open(qd_context* ctx);

/**
 * @brief Close a file handle
 *
 * Stack effect: ( handle:p -- )
 *
 * Closes the file associated with the given handle.
 * After closing, the handle should not be used again.
 * It is safe to close a null handle (no-op).
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_close(qd_context* ctx);

/**
 * @brief Read bytes from a file into a buffer (unified buffer-based API)
 *
 * Stack effect: ( handle:p buffer:p count:i -- bytes_read:i )
 *
 * Reads up to 'count' bytes from the file into the provided buffer.
 * The buffer must be pre-allocated with at least 'count' bytes.
 * Returns the number of bytes actually read.
 * Returns -1 on error.
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_read(qd_context* ctx);

/**
 * @brief Write bytes from a buffer to a file (unified buffer-based API)
 *
 * Stack effect: ( handle:p buffer:p count:i -- bytes_written:i )
 *
 * Writes 'count' bytes from the buffer to the file.
 * Returns the number of bytes actually written.
 * Returns -1 on error.
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_write(qd_context* ctx);

/**
 * @brief Read bytes from a file (legacy string-based API)
 *
 * Stack effect: ( handle:p count:i -- data:s bytes_read:i )
 *
 * Reads up to 'count' bytes from the file.
 * Returns the data as a string and the number of bytes actually read.
 * If bytes_read < count, either EOF was reached or an error occurred.
 * Returns empty string and -1 on error.
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_read_string(qd_context* ctx);

/**
 * @brief Write bytes to a file (legacy string-based API)
 *
 * Stack effect: ( handle:p data:s -- bytes_written:i )
 *
 * Writes the given data to the file.
 * Returns the number of bytes written.
 * Returns -1 on error.
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_write_string(qd_context* ctx);

/**
 * @brief Seek to a position in a file
 *
 * Stack effect: ( handle:p offset:i whence:i -- position:i )
 *
 * Sets the file position indicator for the stream.
 *
 * whence values:
 * - 0 (SEEK_SET): Beginning of file
 * - 1 (SEEK_CUR): Current position
 * - 2 (SEEK_END): End of file
 *
 * Returns the new position from the beginning of the file.
 * Returns -1 on error.
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_seek(qd_context* ctx);

/**
 * @brief Get current position in a file
 *
 * Stack effect: ( handle:p -- position:i )
 *
 * Returns the current file position from the beginning of the file.
 * Returns -1 on error.
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_tell(qd_context* ctx);

// Legacy name for backwards compatibility
qd_exec_result usr_io_seekg(qd_context* ctx);

/**
 * @brief Check if end-of-file has been reached
 *
 * Stack effect: ( handle:p -- handle:p eof:i )
 *
 * Returns 1 if EOF has been reached, 0 otherwise.
 * Leaves the handle on the stack for chaining operations.
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_eof(qd_context* ctx);

/**
 * @brief Read a line from stdin
 *
 * Stack effect: ( -- line:s )!
 *
 * Reads a line from stdin, stripping the trailing newline.
 * Returns empty string on EOF or error.
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_readline(qd_context* ctx);

/**
 * @brief Read entire file contents
 *
 * Stack effect: ( path:s -- contents:s )!
 *
 * Opens the file at the given path, reads the entire contents,
 * and returns it as a string. The file is automatically closed.
 *
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_io_read_file(qd_context* ctx);

#endif // STDIOQD_IO_H
