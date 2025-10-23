#ifndef QD_CGEN_COMPILER_H
#define QD_CGEN_COMPILER_H

#include "translation_unit.h"
#include <optional>

namespace Qd {
	class Compiler {
	public:
		std::optional<TranslationUnit> compile(const char* filename, const char* flags) const;
	};
}

#endif
