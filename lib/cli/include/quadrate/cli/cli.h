// SPDX-License-Identifier: GPL-3.0-or-later
// Common CLI utilities for Quadrate tools

#ifndef QDCLI_CLI_H
#define QDCLI_CLI_H

#include <functional>
#include <string>
#include <vector>

namespace qdcli {

	// Base options common to all tools
	struct BaseOptions {
		std::vector<std::string> paths;
		bool help = false;
		bool version = false;
	};

	// Handler for tool-specific options
	// Called for each argument starting with '-' that isn't -h/-v/--help/--version
	// Parameters: arg (current argument), i (current index, can be incremented to consume next arg),
	//             argc, argv
	// Return: true if option was handled, false if unknown (will print error)
	using OptionHandler = std::function<bool(const char* arg, int& i, int argc, char* argv[])>;

	// Parse command-line arguments
	// Returns true on success, false on error (error message already printed)
	// If handler is nullptr, any option other than -h/-v/--help/--version is an error
	bool parseArgs(int argc, char* argv[], BaseOptions& opts, const char* toolName, OptionHandler handler = nullptr);

	// Print version string to stdout
	void printVersion(const char* toolName);

	// Check if paths is empty and print "no input files" error if so
	// Returns true if paths is empty (caller should return 1)
	bool checkNoInputFiles(const BaseOptions& opts, const char* toolName);

	// Returns true if colors should be disabled (NO_COLOR env var is set)
	bool noColor();

} // namespace qdcli

#endif // QDCLI_CLI_H
