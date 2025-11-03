#ifndef QD_CGEN_TRANSPILER_H
#define QD_CGEN_TRANSPILER_H

#include "source_file.h"
#include <optional>
#include <sstream>

namespace Qd {
	class IAstNode;

	class Transpiler {
	public:
		std::optional<SourceFile> emit(
				const char* filename, const char* package, const char* source, bool verbose, bool dumpTokens) const;

	private:
		void generateImportWrappers(IAstNode* node, const char* package, std::stringstream& out,
				std::unordered_set<std::string>& importedLibraries) const;
		mutable int mVarCounter = 0;
	};
}

#endif
