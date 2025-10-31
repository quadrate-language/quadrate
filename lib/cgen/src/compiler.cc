#include <cgen/compiler.h>
#include <cgen/process.h>
#include <iostream>
#include <qc/colors.h>
#include <sstream>

namespace Qd {
	std::optional<TranslationUnit> Compiler::compile(const char* filename, const char* flags, bool verbose) const {
		// Build argument list for safe execution
		std::vector<std::string> args;
		args.push_back("-c");
		args.push_back(filename);
		args.push_back("-o");
		args.push_back(std::string(filename) + ".o");

		// Parse flags string into individual arguments
		std::istringstream flagStream(flags);
		std::string flag;
		while (flagStream >> flag) {
			args.push_back(flag);
		}

		if (verbose) {
			std::cout << Colors::bold() << "quadc: " << Colors::reset() << "gcc";
			for (const auto& arg : args) {
				std::cout << " " << arg;
			}
			std::cout << std::endl;
		}

		// Execute gcc safely without shell injection vulnerability
		int ret = executeProcess("gcc", args);
		if (ret != 0) {
			return std::nullopt;
		}
		return TranslationUnit{std::string(filename) + ".o"};
	}
}
