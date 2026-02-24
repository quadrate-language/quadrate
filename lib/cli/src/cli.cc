// SPDX-License-Identifier: GPL-3.0-or-later
// Common CLI utilities for Quadrate tools

#include "quadrate/cli/cli.h"

#include <cstring>
#include <iostream>

#include "version.h"

namespace qdcli {

	bool parseArgs(int argc, char* argv[], BaseOptions& opts, const char* toolName, OptionHandler handler) {
		for (int i = 1; i < argc; i++) {
			const char* arg = argv[i];

			if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
				opts.help = true;
				return true;
			}

			if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
				opts.version = true;
				return true;
			}

			if (arg[0] == '-') {
				// Tool-specific option
				if (handler && handler(arg, i, argc, argv)) {
					continue;
				}
				std::cerr << toolName << ": unknown option: " << arg << "\n";
				std::cerr << "Try '" << toolName << " --help' for more information.\n";
				return false;
			}

			// Non-option argument (file/path)
			opts.paths.push_back(arg);
		}

		return true;
	}

	void printVersion(const char* toolName) {
		std::cout << quadrate_version_string(toolName) << "\n";
	}

	bool checkNoInputFiles(const BaseOptions& opts, const char* toolName) {
		if (opts.paths.empty() && !opts.help && !opts.version) {
			std::cerr << toolName << ": no input files\n";
			std::cerr << "Try '" << toolName << " --help' for more information.\n";
			return true;
		}
		return false;
	}

} // namespace qdcli
