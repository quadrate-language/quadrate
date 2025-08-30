#include <lexer/scanner.h>
#include <sstream>
#include <iostream>
#include <cstdint>
#include "tokenizer.h"

namespace Qd {
	Scanner::Scanner(const std::u8string_view& source)
		: mSource(source)
	{}

	void Scanner::lex() {
		auto tokens = Tokenizer::tokenize(mSource);
		for (const auto& token : tokens) {
			std::cout << "Token: " << token << "\n";
		}
	}
}

