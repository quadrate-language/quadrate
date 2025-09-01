#include <lexer/scanner.h>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <u8/str.h>

namespace Qd {
	Scanner::Scanner(const std::string& source)
		: mSource(source)
	{}

	void Scanner::lex() {
		std::string::const_iterator itr = mSource.begin();
		while (itr != mSource.end()) {
			U8::Rune r = U8::Str::next(itr, mSource.end());

			std::string glyph;
			U8::Str::append(r, glyph);
			std::cout << "Read codepoint: U+" << std::hex << static_cast<uint32_t>(r) << std::dec << " " << glyph << "\n";
		}
	}
}

