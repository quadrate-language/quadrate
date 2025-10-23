#include <cgen/compiler.h>
#include <iostream>

namespace Qd {
	std::optional<TranslationUnit> Compiler::compile(const char* filename, const char* flags) const {
		std::string cmd =
				"gcc -c " + std::string(filename) + " -o " + std::string(filename) + ".o " + std::string(flags);
		std::cout << "Compiling: " << cmd << std::endl;
		int ret = std::system(cmd.c_str());
		if (ret != 0) {
			return std::nullopt;
		}
		return TranslationUnit{std::string(filename) + ".o"};
	}
}
