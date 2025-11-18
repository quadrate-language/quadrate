/**
 * @file os.h
 * @brief Operating system interface for Quadrate (os:: module)
 *
 * Provides system-level operations such as process control and environment access.
 */

#ifndef QD_STDQD_OS_H
#define QD_STDQD_OS_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Exit the program with the given exit code
 *
 * @par Stack Effect: ( code:i -- )
 *
 * Terminates the program immediately with the specified exit code.
 *
 * @param ctx Execution context
 * @return Execution result (never returns)
 *
 * @par Example:
 * @code
 * 0 os::exit  // Exit with success code
 * 1 os::exit  // Exit with error code
 * @endcode
 */
qd_exec_result usr_os_exit(qd_context* ctx);

/**
 * @brief Execute a shell command and return the exit code
 *
 * @par Stack Effect: ( cmd:s -- exitcode:i )
 *
 * Executes the command in a system shell and returns the exit code.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "ls -la" os::system .        // Execute ls and print exit code
 * "echo hello" os::system drop  // Execute echo and discard exit code
 * @endcode
 */
qd_exec_result usr_os_system(qd_context* ctx);

/**
 * @brief Get an environment variable
 *
 * @par Stack Effect: ( varname:s -- value:s )
 *
 * Retrieves the value of the specified environment variable.
 * Pushes an empty string if the variable is not set.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "HOME" os::getenv .  // Print home directory
 * @endcode
 */
qd_exec_result usr_os_getenv(qd_context* ctx);

/**
 * @brief Check if a file or directory exists
 *
 * @par Stack Effect: ( path:s -- exists:i )
 *
 * Returns 1 if the file or directory exists, 0 otherwise.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp/test.txt" os::exists .  // Check if file exists
 * @endcode
 */
qd_exec_result usr_os_exists(qd_context* ctx);

/**
 * @brief Delete a file
 *
 * @par Stack Effect: ( path:s -- result:i )
 *
 * Deletes the specified file. Returns 0 on success, -1 on failure.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp/test.txt" os::delete drop  // Delete file
 * @endcode
 */
qd_exec_result usr_os_delete(qd_context* ctx);

/**
 * @brief Rename or move a file
 *
 * @par Stack Effect: ( oldpath:s newpath:s -- result:i )
 *
 * Renames or moves a file from oldpath to newpath.
 * Returns 0 on success, -1 on failure.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp/old.txt" "/tmp/new.txt" os::rename drop
 * @endcode
 */
qd_exec_result usr_os_rename(qd_context* ctx);

/**
 * @brief Copy a file
 *
 * @par Stack Effect: ( srcpath:s dstpath:s -- result:i )
 *
 * Copies a file from srcpath to dstpath.
 * Returns 0 on success, -1 on failure.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp/src.txt" "/tmp/dst.txt" os::copy drop
 * @endcode
 */
qd_exec_result usr_os_copy(qd_context* ctx);

/**
 * @brief Create a directory
 *
 * @par Stack Effect: ( path:s -- result:i )
 *
 * Creates a directory at the specified path.
 * Returns 0 on success, -1 on failure.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp/mydir" os::mkdir drop
 * @endcode
 */
qd_exec_result usr_os_mkdir(qd_context* ctx);

/**
 * @brief List directory contents
 *
 * @par Stack Effect: ( path:s -- entries:p count:i )
 *
 * Lists the contents of a directory.
 * Returns an array of strings (entries) and the count.
 * Caller is responsible for freeing the array and strings.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp" os::list -> count -> entries
 * // Process entries...
 * entries mem::free
 * @endcode
 */
qd_exec_result usr_os_list(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_STDQD_OS_H
