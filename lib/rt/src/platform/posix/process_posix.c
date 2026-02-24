// POSIX implementation of process_platform using fork/exec
#define _POSIX_C_SOURCE 200809L
#include "../process_platform.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int process_platform_exec_wait(const char* path, char* const argv[]) {
	if (!path || !argv) {
		return -1;
	}

	pid_t pid = fork();
	if (pid == -1) {
		return -1;
	}

	if (pid == 0) {
		// Child process
		execv(path, argv);
		// If execv returns, it failed
		_exit(127);
	}

	// Parent process - wait for child
	int status;
	if (waitpid(pid, &status, 0) == -1) {
		return -1;
	}

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}

	// Process didn't exit normally (killed by signal, etc.)
	return -1;
}

int process_platform_exec_capture(const char* command, char* output, size_t output_size) {
	if (!command || !output || output_size == 0) {
		return -1;
	}

	output[0] = '\0';

	FILE* pipe = popen(command, "r");
	if (!pipe) {
		return -1;
	}

	size_t total = 0;
	char buffer[256];
	while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
		size_t len = strlen(buffer);
		if (total + len < output_size) {
			memcpy(output + total, buffer, len);
			total += len;
		}
	}
	output[total] = '\0';

	int status = pclose(pipe);
	if (status == -1) {
		return -1;
	}

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}

	return -1;
}

unsigned long process_platform_getpid(void) {
	return (unsigned long)getpid();
}
