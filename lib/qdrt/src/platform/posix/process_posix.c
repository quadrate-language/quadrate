// POSIX implementation of process_platform using fork/exec
#include "../process_platform.h"
#include <errno.h>
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
