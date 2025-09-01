#include <lexer/scanner.h>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <u8/str.h>

namespace Qd {
	Scanner::Scanner(const std::string& source)
		: mSource(source)
	{}

	void Scanner::lex(std::vector<Token>& tokens) {
		mCursor = 0;
		mLine = 1;
		mColumn = 1;
		std::string::const_iterator itr = mSource.begin();
		while (itr != mSource.end()) {
			Rune r = Str::next(itr, mSource.end());

			if (isDigit(r)) {
				// TODO: Support digits.
				continue;
			}

			switch (r) {
				case U' ':
				case U'\t':
					++mCursor;
					++mColumn;
					continue;;
				case U'\0':
					// Should not reach here.
					break;
				case U'(':
					++mColumn;
					++mCursor;
					tokens.push_back(Token{ SourceSpan(mLine, mColumn, mLine, mColumn), TokenType::LParen, "(" });
					break;
				case U')':
					++mColumn;
					++mCursor;
					tokens.push_back(Token{ SourceSpan(mLine, mColumn, mLine, mColumn), TokenType::RParen, "(" });
					break;
				case U'{':
					++mColumn;
					++mCursor;
					tokens.push_back(Token{ SourceSpan(mLine, mColumn, mLine, mColumn), TokenType::LBrace, "{" });
					break;
				case U'}':
					++mColumn;
					++mCursor;
					tokens.push_back(Token{ SourceSpan(mLine, mColumn, mLine, mColumn), TokenType::RBrace, "}" });
					break;
				default:
					break;
			}

			std::string glyph;
			Str::append(r, glyph);
			std::cout << "Read codepoint: U+" << std::hex << static_cast<uint32_t>(r) << std::dec << " " << glyph << "\n";
		}
	}
}

