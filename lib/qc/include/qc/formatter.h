#ifndef QD_QC_FORMATTER_H
#define QD_QC_FORMATTER_H

#include <string>

namespace Qd {
	// Source-based formatter - works directly on source text
	// Only formats structure (function signatures, if/for/loop, indentation)
	// Never breaks user's lines
	std::string formatSource(const std::string& source);
}

#endif
