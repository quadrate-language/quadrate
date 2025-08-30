#ifndef QD_LEXER_SCANNER_H
#define QD_LEXER_SCANNER_H

#include <string>

namespace Qd {
	class Scanner {
	public:
		Scanner(const std::u8string_view& source);

		void lex();
	
	private:
		char32_t getNextCodepoint(std::istream& stream) const;

		std::u8string_view mSource;
		size_t mCursor = 0;
		size_t mLine = 1;
		size_t mColumn = 1;
	};
}

#endif

