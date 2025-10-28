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

	bool isInstruction(const char* identifier) {
		if (identifier == nullptr) {
			return false;
		}

		// List of all built-in instructions (stack operations)
		static const char* instructions[] = {
				".",
				"div",
				"dup",
				"print",
				"printv",
				"rot",
				"sq",
				"swap",
		};

		static const size_t instructionCount = sizeof(instructions) / sizeof(instructions[0]);

		for (size_t i = 0; i < instructionCount; i++) {
			if (strcmp(identifier, instructions[i]) == 0) {
				return true;
			}
		}

		return false;
	}
}
