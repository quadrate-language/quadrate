#ifndef QD_LEXER_SCANNER_H
#define QD_LEXER_SCANNER_H

#include <string>
#include <vector>
#include "token.h"

namespace Qd {
	class Scanner {
	public:
		Scanner(const std::string& source);

		void lex(std::vector<Token>& tokens);
	
	private:
		const std::string& mSource;
		size_t mCursor = 0;
		size_t mLine = 1;
		size_t mColumn = 1;
	};
}

#endif

