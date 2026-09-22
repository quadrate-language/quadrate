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
 * Execute a program and wait for it to complete, reporting a terminating signal.
 *
 * @param path Path to the executable
 * @param argv Null-terminated array of arguments (argv[0] should be program name)
 * @param term_signal If non-NULL, set to the signal that killed the program, or 0
 * @return Exit code of the program, or -1 if it did not exit normally or failed to execute
 */
int process_platform_exec_wait_signal(const char* path, char* const argv[], int* term_signal);

/** Returned by process_platform_exec_capture when the output did not fit the buffer. */
#define PROCESS_PLATFORM_ERR_TRUNCATED (-2)

/**
 * Execute a shell command and capture its stdout output.
 *
 * @param command Shell command to execute
 * @param output Buffer to store output (caller allocated, always NUL-terminated)
 * @param output_size Size of output buffer
 * @return Exit code of the command, -1 on failure, or PROCESS_PLATFORM_ERR_TRUNCATED when
 *         the output exceeded the buffer (which then holds the part that fitted)
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
