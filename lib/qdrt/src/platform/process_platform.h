#ifndef QD_QDRT_PROCESS_PLATFORM_H
#define QD_QDRT_PROCESS_PLATFORM_H

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

#ifdef __cplusplus
}
#endif

#endif // QD_QDRT_PROCESS_PLATFORM_H
