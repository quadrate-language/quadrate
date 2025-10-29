#include <cgen/linker.h>
#include <iostream>
#include <qc/colors.h>
#include <sstream>

namespace Qd {
	bool Linker::link(const std::vector<TranslationUnit>& translationUnits, const char* outputFilename,
			const char* flags, bool verbose) const {
		std::stringstream cmd;
		cmd << "gcc ";
		for (const auto& obj : translationUnits) {
			cmd << obj.objectFilename << " ";
		}
		cmd << "-o " << outputFilename << " " << flags;

		if (verbose) {
			std::cout << Colors::bold() << "quadc: " << Colors::reset() << cmd.str() << std::endl;
		}
		int ret = std::system(cmd.str().c_str());
		return ret == 0;
	}
}
