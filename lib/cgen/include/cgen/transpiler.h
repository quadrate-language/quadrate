#ifndef QD_CGEN_TRANSPILER_H
#define QD_CGEN_TRANSPILER_H

#include "source_file.h"
#include <optional>
#include <string>

namespace Qd {
	class IAstNode;

	class Transpiler {
	public:
		std::optional<SourceFile> emit(const char* filename, const char* package, const char* source) const;

	private:
		void traverse(IAstNode* node, const char* packageName, std::stringstream& out, int indent) const;
	};
}

#endif
