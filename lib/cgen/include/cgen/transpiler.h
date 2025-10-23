#ifndef QD_CGEN_TRANSPILER_H
#define QD_CGEN_TRANSPILER_H

#include "source_file.h"
#include <optional>

namespace Qd {
	class IAstNode;

	class Transpiler {
	public:
		std::optional<SourceFile> emit(const char* filename, const char* package, const char* source) const;
	};
}

#endif
