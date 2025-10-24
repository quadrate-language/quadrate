#include <cgen/keywords.h>
#include <cstring>

namespace Qd {
	bool isKeyword(const char* identifier) {
		if (identifier == nullptr) {
			return false;
		}

		static const char* keywords[] = {
				"break",
				"case",
				"const",
				"continue",
				"default",
				"defer",
				"else",
				"fn",
				"for",
				"if",
				"print",
				"return",
				"switch",
				"use",
		};

		static const size_t keywordCount = sizeof(keywords) / sizeof(keywords[0]);

		for (size_t i = 0; i < keywordCount; i++) {
			if (strcmp(identifier, keywords[i]) == 0) {
				return true;
			}
		}

		return false;
	}
}
