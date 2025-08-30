#ifndef QD_LEXER_SCANNER_H
#define QD_LEXER_SCANNER_H

#include <string>

namespace Qd {
	class Scanner {
	public:
		Scanner(const std::u8string_view& source);
	
	private:
		std::u8string_view source;
		size_t cursor = 0;
		size_t line = 1;
		size_t column = 1;
	};
}

#endif

