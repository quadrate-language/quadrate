/**
 * @file os.h
 * @brief Operating system interface for Quadrate (os:: module)
 *
 * Provides system-level operations such as process control and environment access.
 */

#ifndef QD_QDOS_OS_H
#define QD_QDOS_OS_H

#include <quadrate/rt/context.h>
#include <quadrate/rt/exec_result.h>

/**
 * @brief Error codes for os module
 *
 * These match the constants defined in os/module.qd
 * Ok=1 (success), specific errors start at 2
 */
#define OS_ERR_OK 1				/**< Success (matches builtin Ok) */
#define OS_ERR_NOT_FOUND 2		/**< No such file or directory */
#define OS_ERR_PERMISSION 3		/**< Permission denied */
#define OS_ERR_EXISTS 4			/**< File already exists */
#define OS_ERR_NOT_DIRECTORY 5	/**< Not a directory */
#define OS_ERR_IS_DIRECTORY 6	/**< Is a directory */
#define OS_ERR_IO 7				/**< I/O error */
#define OS_ERR_NO_SPACE 8		/**< No space left on device */
#define OS_ERR_READ_ONLY 9		/**< Read-only file system */
#define OS_ERR_NAME_TOO_LONG 10 /**< File name too long */
#define OS_ERR_OUT_OF_MEMORY 11 /**< Out of memory */
#define OS_ERR_INVALID_ARG 12	/**< Invalid argument */

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
int usr_os_exit(qd_context* ctx);

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
int usr_os_system(qd_context* ctx);

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
 * "HOME" os::getenv print  // Print home directory
 * @endcode
 */
int usr_os_getenv(qd_context* ctx);

/**
 * @brief Set an environment variable
 *
 * @par Stack Effect: ( name:s value:s -- )
 *
 * Sets the specified environment variable to the given value.
 * Overwrites any existing value.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "MY_VAR" "hello" os::setenv  // Set MY_VAR to "hello"
 * @endcode
 */
int usr_os_setenv(qd_context* ctx);

/**
 * @brief Unset an environment variable
 *
 * @par Stack Effect: ( name:s -- )
 *
 * Removes the specified environment variable.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "MY_VAR" os::unsetenv  // Remove MY_VAR from environment
 * @endcode
 */
int usr_os_unsetenv(qd_context* ctx);

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
 * "/tmp/test.txt" os::exists print  // Check if file exists
 * @endcode
 */
int usr_os_exists(qd_context* ctx);

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
int usr_os_delete(qd_context* ctx);

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
int usr_os_rename(qd_context* ctx);

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
int usr_os_copy(qd_context* ctx);

/**
 * @brief Create a directory and all parent directories
 *
 * @par Stack Effect: ( path:s -- result:i )
 *
 * Creates the directory and any missing parent directories (like mkdir -p).
 * Returns 0 on success, error code on failure.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp/a/b/c" os::mkdir drop  // Creates /tmp/a, /tmp/a/b, /tmp/a/b/c
 * @endcode
 */
int usr_os_mkdir(qd_context* ctx);

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
int usr_os_list(qd_context* ctx);

/**
 * @brief Execute a command and stream output to callback
 *
 * @par Stack Effect: ( cmd:s callback:p -- exitcode:i )
 *
 * Executes a command using popen() and calls the callback function
 * for each line of output. The callback receives a string and should
 * have the signature: fn (line:str -- )
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "ls -la" fn (line:str -- ) { line . nl } os::popen! -> exitcode
 * @endcode
 */
int usr_os_popen(qd_context* ctx);

/**
 * @brief Execute command and capture all output
 *
 * @par Stack Effect: ( cmd:s -- stdout:s exitcode:i )!
 *
 * Simpler alternative to os::popen when you just need the full output.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "echo hello" os::exec! -> exitcode -> stdout
 * @endcode
 */
int usr_os_exec(qd_context* ctx);

/**
 * @brief Get the current process ID
 *
 * @par Stack Effect: ( -- pid:i )
 *
 * Returns the process ID of the current process.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * os::getpid print  // Print current PID
 * @endcode
 */
int usr_os_getpid(qd_context* ctx);

/**
 * @brief Check if path is a directory
 *
 * @par Stack Effect: ( path:s -- is_dir:i )
 *
 * Returns 1 if path is a directory, 0 otherwise.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_os_is_dir(qd_context* ctx);

/**
 * @brief Check if path is a regular file
 *
 * @par Stack Effect: ( path:s -- is_file:i )
 *
 * Returns 1 if path is a regular file, 0 otherwise.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_os_is_file(qd_context* ctx);

/**
 * @brief Remove directory and all contents recursively
 *
 * @par Stack Effect: ( path:s -- )
 *
 * Removes a directory and all its contents (like rm -rf).
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_os_rmdir(qd_context* ctx);

/**
 * @brief Walk a directory tree recursively
 *
 * @par Stack Effect: ( path:s callback:p -- )
 *
 * Calls the callback for each file and directory in the tree.
 * Callback signature: fn (path:str is_dir:i64 depth:i64 -- )
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_os_walk(qd_context* ctx);

/**
 * @brief Match files using glob pattern
 *
 * @par Stack Effect: ( pattern:s -- entries:p count:i )
 *
 * Returns array of paths matching the glob pattern.
 * Supports *, ?, and ** for recursive matching.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_os_glob(qd_context* ctx);

/**
 * @brief Create a unique temporary directory
 *
 * @par Stack Effect: ( -- path:s )
 *
 * Creates a unique temporary directory in /tmp and returns its path.
 * The directory is created with mode 0700 (owner read/write/execute only).
 * Caller is responsible for removing the directory when done (use os::rmdir).
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * os::mktemp! -> tmpdir
 * // Use tmpdir...
 * tmpdir os::rmdir!  // Clean up when done
 * @endcode
 */
int usr_os_mktemp(qd_context* ctx);

/**
 * @brief Get current working directory
 *
 * @par Stack Effect: ( -- path:s )
 *
 * Returns the current working directory as a string.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * os::cwd! -> dir
 * dir print nl
 * @endcode
 */
int usr_os_cwd(qd_context* ctx);

/**
 * @brief Create a symbolic link
 *
 * @par Stack Effect: ( target:s linkpath:s -- )!
 *
 * Creates a symbolic link at linkpath pointing to target.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/usr/bin/python3" "/tmp/python" os::symlink!
 * @endcode
 */
int usr_os_symlink(qd_context* ctx);

/**
 * @brief Read the target of a symbolic link
 *
 * @par Stack Effect: ( path:s -- target:s )!
 *
 * Returns the target path that the symbolic link points to.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp/mylink" os::readlink! -> target
 * @endcode
 */
int usr_os_readlink(qd_context* ctx);

/**
 * @brief Check if path is a symbolic link
 *
 * @par Stack Effect: ( path:s -- is_symlink:i )
 *
 * Returns 1 if path is a symbolic link, 0 otherwise.
 * Does not follow the link to check if the target exists.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp/mylink" os::is_symlink print
 * @endcode
 */
int usr_os_is_symlink(qd_context* ctx);

/**
 * @brief Change file permissions
 *
 * @par Stack Effect: ( path:s mode:i -- )!
 *
 * Changes the permissions of a file or directory.
 * Mode is specified as an octal value (e.g., 0644, 0755).
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp/myfile" 0o644 os::chmod!
 * @endcode
 */
int usr_os_chmod(qd_context* ctx);

/**
 * @brief Change file owner and group
 *
 * @par Stack Effect: ( path:s uid:i gid:i -- )!
 *
 * Changes the owner (uid) and group (gid) of a file or directory.
 * Use -1 for uid or gid to leave unchanged.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp/myfile" 1000 1000 os::chown!
 * @endcode
 */
int usr_os_chown(qd_context* ctx);

/**
 * @brief Get the current user ID
 *
 * @par Stack Effect: ( -- uid:i )
 *
 * Returns the real user ID of the calling process.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_os_getuid(qd_context* ctx);

/**
 * @brief Get the current group ID
 *
 * @par Stack Effect: ( -- gid:i )
 *
 * Returns the real group ID of the calling process.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_os_getgid(qd_context* ctx);

/**
 * @brief Get the hostname
 *
 * @par Stack Effect: ( -- hostname:s )!
 *
 * Returns the hostname of the current system.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_os_hostname(qd_context* ctx);

/**
 * @brief Change the current working directory
 *
 * @par Stack Effect: ( path:s -- )!
 *
 * Changes the current working directory to the specified path.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "/tmp" os::chdir!
 * @endcode
 */
int usr_os_chdir(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_QDOS_OS_H
