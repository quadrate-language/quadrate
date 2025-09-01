#ifndef QD_LEXER_SCANNER_H
#define QD_LEXER_SCANNER_H

#include <string>

namespace Qd {
	class Scanner {
	public:
		Scanner(const std::string& source);

		void lex();
	
	private:
		const std::string& mSource;
		size_t mCursor = 0;
		size_t mLine = 1;
		size_t mColumn = 1;
	};
}

#endif

