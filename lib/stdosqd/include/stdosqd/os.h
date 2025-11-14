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

#ifdef __cplusplus
}
#endif

#endif // QD_STDQD_OS_H
