#include <cgen/linker.h>
#include <cgen/process.h>
#include <iostream>
#include <qc/colors.h>
#include <sstream>

namespace Qd {
	bool Linker::link(const std::vector<TranslationUnit>& translationUnits, const char* outputFilename,
			const char* flags, bool verbose) const {
		// Build argument list for safe execution
		std::vector<std::string> args;

		// Add all object files
		for (const auto& obj : translationUnits) {
			args.push_back(obj.objectFilename);
		}

		// Add output flag
		args.push_back("-o");
		args.push_back(outputFilename);

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
		return ret == 0;
	}
}
