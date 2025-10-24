#ifndef QD_CGEN_KEYWORDS_H
#define QD_CGEN_KEYWORDS_H

namespace Qd {
	/**
	 * Checks if the given identifier is a Quadrate language keyword.
	 *
	 * @param identifier The identifier string to check
	 * @return true if the identifier is a keyword, false otherwise
	 */
	bool isKeyword(const char* identifier);

	/**
	 * Checks if the given identifier is a built-in instruction.
	 * Instructions are built-in stack operations like print, sq, div, dup, rot, etc.
	 *
	 * @param identifier The identifier string to check
	 * @return true if the identifier is a built-in instruction, false otherwise
	 */
	bool isInstruction(const char* identifier);
}

#endif
