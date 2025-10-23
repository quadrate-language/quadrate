#ifndef QUADC_COMPILER_H
#define QUADC_COMPILER_H

#include "translation_unit.h"
#include <optional>
#include <string>
#include <vector>

class Compiler {
public:
	Compiler(const char* outputDir);

	bool transpile(const char* filename, const char* package, const char* source);
	std::optional<TranslationUnit> compile(const char* filename, const char* flags);
	bool link(const std::vector<TranslationUnit>& translationUnits, const char* outputFilename, const char* flags);

private:
	std::string mOutputDir;
};

#endif
