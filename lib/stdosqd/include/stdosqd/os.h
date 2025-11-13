#ifndef QD_STDQD_OS_H
#define QD_STDQD_OS_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Exit the program with the given exit code.
 * Stack effect: ( code:i -- )
 *
 * Example in Quadrate:
 *   0 os::exit  // Exit with success code
 *   1 os::exit  // Exit with error code
 */
qd_exec_result usr_os_exit(qd_context* ctx);

/**
 * Execute a shell command and return the exit code.
 * Stack effect: ( cmd:s -- exitcode:i )
 *
 * Example in Quadrate:
 *   "ls -la" os::system .  // Execute ls and print exit code
 *   "echo hello" os::system drop  // Execute echo and discard exit code
 */
qd_exec_result usr_os_system(qd_context* ctx);

qd_exec_result usr_os_getenv(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif
