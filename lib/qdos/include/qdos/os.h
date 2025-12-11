/**
 * @file os.h
 * @brief Operating system interface for Quadrate (os:: module)
 *
 * Provides system-level operations such as process control and environment access.
 */

#ifndef QD_QDOS_OS_H
#define QD_QDOS_OS_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

/**
 * @brief Error codes for os module
 *
 * These match the constants defined in os/module.qd (using POSIX errno values)
 */
#define OS_ERR_NONE 0			/**< No error (success) */
#define OS_ERR_NOT_FOUND 2		/**< No such file or directory (ENOENT) */
#define OS_ERR_IO 5				/**< I/O error (EIO) */
#define OS_ERR_OUT_OF_MEMORY 12 /**< Out of memory (ENOMEM) */
#define OS_ERR_PERMISSION 13	/**< Permission denied (EACCES) */
#define OS_ERR_EXISTS 17		/**< File already exists (EEXIST) */
#define OS_ERR_NOT_DIRECTORY 20 /**< Not a directory (ENOTDIR) */
#define OS_ERR_IS_DIRECTORY 21	/**< Is a directory (EISDIR) */
#define OS_ERR_INVALID_ARG 22	/**< Invalid argument (EINVAL) */
#define OS_ERR_NO_SPACE 28		/**< No space left on device (ENOSPC) */
#define OS_ERR_READ_ONLY 30		/**< Read-only file system (EROFS) */
#define OS_ERR_NAME_TOO_LONG 36 /**< File name too long (ENAMETOOLONG) */

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

#endif // QD_QDOS_OS_H
