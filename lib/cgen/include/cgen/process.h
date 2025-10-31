#ifndef QD_CGEN_PROCESS_H
#define QD_CGEN_PROCESS_H

#include <string>
#include <vector>

namespace Qd {
	/**
	 * Safely execute a command without shell injection vulnerabilities.
	 * Uses fork() and execvp() instead of system() to prevent command injection.
	 *
	 * @param program The program to execute (e.g., "gcc")
	 * @param args The arguments to pass to the program
	 * @return The exit code of the process, or -1 on error
	 */
	int executeProcess(const std::string& program, const std::vector<std::string>& args);
}

#endif
