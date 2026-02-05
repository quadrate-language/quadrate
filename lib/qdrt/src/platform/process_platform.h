#ifndef QD_QDRT_PROCESS_PLATFORM_H
#define QD_QDRT_PROCESS_PLATFORM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Execute a program and wait for it to complete.
 *
 * @param path Path to the executable
 * @param argv Null-terminated array of arguments (argv[0] should be program name)
 * @return Exit code of the program, or -1 on failure to execute
 */
int process_platform_exec_wait(const char* path, char* const argv[]);

/**
 * Execute a shell command and capture its stdout output.
 *
 * @param command Shell command to execute
 * @param output Buffer to store output (caller allocated)
 * @param output_size Size of output buffer
 * @return Exit code of the command, or -1 on failure
 */
int process_platform_exec_capture(const char* command, char* output, size_t output_size);

/**
 * Get current process ID.
 *
 * @return Process ID as unsigned integer
 */
unsigned long process_platform_getpid(void);

#ifdef __cplusplus
}
#endif

#endif // QD_QDRT_PROCESS_PLATFORM_H
