// POSIX implementation of process_platform using fork/exec
#define _POSIX_C_SOURCE 200809L
#include "../process_platform.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int process_platform_exec_wait_signal(const char* path, char* const argv[], int* term_signal) {
	if (term_signal) {
		*term_signal = 0;
	}
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
	while (waitpid(pid, &status, 0) == -1) {
		if (errno != EINTR) {
			return -1;
		}
	}

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}

	if (WIFSIGNALED(status) && term_signal) {
		*term_signal = WTERMSIG(status);
	}
	return -1;
}

int process_platform_exec_wait(const char* path, char* const argv[]) {
	return process_platform_exec_wait_signal(path, argv, NULL);
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
	int truncated = 0;
	char buffer[4096];
	size_t len;
	while ((len = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
		size_t room = output_size - 1 - total;
		if (len > room) {
			truncated = 1;
			len = room;
		}
		memcpy(output + total, buffer, len);
		total += len;
	}
	output[total] = '\0';

	int status = pclose(pipe);
	if (status == -1) {
		return -1;
	}

	if (truncated) {
		return PROCESS_PLATFORM_ERR_TRUNCATED;
	}

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}

	return -1;
}

unsigned long process_platform_getpid(void) {
	return (unsigned long)getpid();
}
