#include <cgen/linker.h>
#include <iostream>
#include <sstream>

namespace Qd {
	bool Linker::link(
			const std::vector<TranslationUnit>& translationUnits, const char* outputFilename, const char* flags) const {
		std::stringstream cmd;
		cmd << "gcc ";
		for (const auto& obj : translationUnits) {
			cmd << obj.objectFilename << " ";
		}
		cmd << "-o " << outputFilename << " " << flags;

		std::cout << "Linking: " << cmd.str() << std::endl;
		int ret = std::system(cmd.str().c_str());
		return ret == 0;
	}
}
