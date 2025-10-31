#include <cgen/process.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <cstring>

namespace Qd {
	int executeProcess(const std::string& program, const std::vector<std::string>& args) {
		// Create argument array for execvp
		// Need to include program name as argv[0] and null terminator
		std::vector<char*> argv;
		argv.reserve(args.size() + 2);

		// argv[0] should be the program name
		argv.push_back(const_cast<char*>(program.c_str()));

		// Add all arguments
		for (const auto& arg : args) {
			argv.push_back(const_cast<char*>(arg.c_str()));
		}

		// Null terminator required by execvp
		argv.push_back(nullptr);

		// Fork a child process
		pid_t pid = fork();

		if (pid < 0) {
			// Fork failed
			return -1;
		}

		if (pid == 0) {
			// Child process - execute the command
			execvp(program.c_str(), argv.data());

			// If execvp returns, it failed
			_exit(127);
		}

		// Parent process - wait for child to complete
		int status;
		if (waitpid(pid, &status, 0) < 0) {
			return -1;
		}

		// Return the exit code
		if (WIFEXITED(status)) {
			return WEXITSTATUS(status);
		}

		// Process was terminated by a signal
		return -1;
	}
}
