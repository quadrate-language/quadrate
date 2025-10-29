#ifndef QD_CGEN_LINKER_H
#define QD_CGEN_LINKER_H

#include "translation_unit.h"
#include <vector>

namespace Qd {
	class Linker {
	public:
		bool link(const std::vector<TranslationUnit>& translationUnits, const char* outputFilename, const char* flags,
				bool verbose) const;
	};
}

#endif
