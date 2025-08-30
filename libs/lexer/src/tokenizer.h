#ifndef QD_LEXER_TOKENIZER_H
#define QD_LEXER_TOKENIZER_H

#include <string>
#include <vector>

namespace Qd {
	class Tokenizer {
	public:
		static std::vector<std::string> tokenize(const std::u8string_view& str);
	};
}

#endif

