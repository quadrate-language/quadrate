// process_platform for a target with no processes, such as a microcontroller:
// every request to start one fails
#include "../process_platform.h"

int process_platform_exec_wait(const char* path, char* const argv[]) {
	(void)path;
	(void)argv;
	return -1;
}

int process_platform_exec_wait_signal(const char* path, char* const argv[], int* term_signal) {
	(void)path;
	(void)argv;
	if (term_signal) {
		*term_signal = 0;
	}
	return -1;
}

int process_platform_exec_capture(const char* command, char* output, size_t output_size) {
	(void)command;
	if (output && output_size > 0) {
		output[0] = '\0';
	}
	return -1;
}

unsigned long process_platform_getpid(void) {
	return 1;
}
