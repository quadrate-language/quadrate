#include "tokenizer.h"
#include <istream>
#include <sstream>
#include <iostream>
#include <cstdint>

namespace Qd {
	char32_t getNextCodepoint(std::istream& stream) {
		unsigned char c;
		if (!stream.get(reinterpret_cast<char&>(c))) {
			return U'\0'; // End of stream
		}

		if (c < 0x80) {
			return c;
		}

		int extra = (c >> 5 == 0x6) ? 1 :
				(c >> 4 == 0xE) ? 2 :
				(c >> 3 == 0x1E) ? 3 : 0;
		char32_t cp = c & ((1 << (7 - extra)) - 1);
		for (int i = 0; i < extra; i++) {
			if (!stream.get(reinterpret_cast<char&>(c)) || (c >> 6) != 0x2) {
				return U'\uFFFD';
			}
			cp = (cp << 6) | (c & 0x3F);
		}
		return cp;
	}

	std::string readIdentifier(std::istream& stream) {
		std::string identifier;
		return identifier;
	}

	std::vector<std::string> Tokenizer::tokenize(const std::u8string_view& str) {
		std::vector<std::string> tokens;
	
		std::string bytes(reinterpret_cast<const char*>(str.data()), str.size());
		std::istringstream reader(bytes);

		while (true) {
			char32_t cp = getNextCodepoint(reader);
			if (cp == U'\0') {
				tokens.push_back("EOF");
				break;
			}
			switch (cp) {
				case '(':
				case ')':
				case '{':
				case '}':
				case ' ':
					tokens.push_back(std::string(1, static_cast<char>(cp)));
					break;
				case '\n':
					tokens.push_back("EOL");
					break;
				default:
					std::string ident = readIdentifier(reader);
					tokens.push_back(ident);
					break;
			}
			std::cout << "Read codepoint: U+" << std::hex << static_cast<uint32_t>(cp) << std::dec << " " << static_cast<char>(cp) << "\n";
		}

		return tokens;
	}

}

