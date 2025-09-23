#ifndef QD_DIAGNOSTIC_ISSUE_H
#define QD_DIAGNOSTIC_ISSUE_H

#include <string>
#include <vector>
#include "source_span.h"

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

