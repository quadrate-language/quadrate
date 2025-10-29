#include <cgen/compiler.h>
#include <iostream>
#include <qc/colors.h>

namespace Qd {
	std::optional<TranslationUnit> Compiler::compile(const char* filename, const char* flags, bool verbose) const {
		std::string cmd =
				"gcc -c " + std::string(filename) + " -o " + std::string(filename) + ".o " + std::string(flags);
		if (verbose) {
			std::cout << Colors::bold() << "quadc: " << Colors::reset() << cmd << std::endl;
		}
		int ret = std::system(cmd.c_str());
		if (ret != 0) {
			return std::nullopt;
		}
		return TranslationUnit{std::string(filename) + ".o"};
	}
}
