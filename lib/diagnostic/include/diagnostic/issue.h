#ifndef QD_DIAGNOSTIC_ISSUE_H
#define QD_DIAGNOSTIC_ISSUE_H

#include "source_span.h"
#include <string>
#include <vector>

namespace Qd {
	enum class Category;
	enum class Severity;

	struct Issue {
		SourceSpan span;
		std::string message;
		std::vector<std::string> notes;
		Category category;
		Severity severity;
	};
}

#endif
