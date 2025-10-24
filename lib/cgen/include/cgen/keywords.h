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
}

#endif
